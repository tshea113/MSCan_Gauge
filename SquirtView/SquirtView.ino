#include <Metro.h>
#include <FlexCAN_T4.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimeLib.h>
#include <FastLED.h>
#include <EncoderTool.h>
#include <EEPROM.h>

#include "MegasquirtMessages.h"
#include "definitions.h"
#include "constants.h"
#include "GaugeData.h"

using namespace EncoderTool;

// FastLED
CRGB leds[NUM_LEDS];

// Encoder
Encoder myEnc;
int16_t encoderIndex;
bool buttonPressed;
volatile unsigned long last_millis;   //switch debouncing

// FlexCAN
// Teensy 3.2 only has CAN0, but Teensy 4.0 has CAN1, CAN2, and CAN3, so we must
// set these up based upon the device for which we are compiling.
#if defined(__MK20DX256__) || defined(__MK64FX512__) // Teensy 3.2/3.5
  FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> myCan;
#endif
#if defined(__IMXRT1062__) // Teensy 4.0
  FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> myCan;
#endif

// OLED Display I2C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Metro ticks are milliseconds
Metro commTimer = Metro(CAN_TIMEOUT);
Metro displayTimer = Metro(DISPLAY_REFRESH);
Metro ledTimer = Metro(LED_FLASH_TIMER);
Metro gaugeBlinkTimer = Metro(GAUGE_FLASH_TIMER);
boolean connectionState = false;
boolean gaugeBlink = false;

//MS data vars
GaugeData gaugeData;

// Menu vars
MenuState menuState;
Settings gaugeSettings;

byte neo_brightness = 1;
byte g_textsize = 1;
char tempchars[11];

static CAN_message_t txmsg,rxmsg;

msg_packed rxmsg_id,txmsg_id;
msg_req_data_raw msg_req_data;

unsigned long validity_window; // for hi/low + histogram window update
unsigned long validity_window2;

byte histogram[64]; // 512 memory usage
byte histogram_index;

// -------------------------------------------------------------
static void ledBlink()
{
  ledTimer.reset();
  digitalWrite(TEENSY_LED, 1);
}

// -------------------------------------------------------------
void setup(void)
{
  pinMode(TEENSY_LED, OUTPUT);
  digitalWrite(TEENSY_LED, 1);

  // On first run of the gauge, the EEPROM will have garbage values. To fix this,
  // we check for an identifier in the first address and write default values if it
  // is not present.
  if (EEPROM.read(EEPROM_INIT) == EEPROM_VALID)
  {
    gaugeSettings.LEDRingEnable = EEPROM.read(RING_ENABLE_ADDR);
    gaugeSettings.shiftRPM = EEPROM.read((SHIFT_RPM_ADDR + 1)) << 8;
    gaugeSettings.shiftRPM |= EEPROM.read(SHIFT_RPM_ADDR);
    gaugeSettings.warningsEnable = EEPROM.read(WARN_ENABLE_ADDR);
    gaugeSettings.coolantWarning = EEPROM.read((CLT_WARN_ADDR + 1)) << 8;
    gaugeSettings.coolantWarning |= EEPROM.read(CLT_WARN_ADDR);
  }
  else
  {
    EEPROM.write(RING_ENABLE_ADDR, gaugeSettings.LEDRingEnable);
    EEPROM.write(SHIFT_RPM_ADDR, gaugeSettings.shiftRPM);
    EEPROM.write(SHIFT_RPM_ADDR + 1, gaugeSettings.shiftRPM >> 8);
    EEPROM.write(WARN_ENABLE_ADDR, gaugeSettings.warningsEnable);
    EEPROM.write(CLT_WARN_ADDR, gaugeSettings.coolantWarning);
    EEPROM.write(CLT_WARN_ADDR + 1, gaugeSettings.coolantWarning >> 8);
    EEPROM.write(EEPROM_INIT, EEPROM_VALID);
  }

  if (DEBUG_MODE)
  {
    Serial.print("LED Ring Enable: ");
    Serial.println(gaugeSettings.LEDRingEnable);
    Serial.print("Shift RPM: ");
    Serial.println(gaugeSettings.shiftRPM);
    Serial.print("Warnings Enable: ");
    Serial.println(gaugeSettings.warningsEnable);
    Serial.print("Coolant Warning: ");
    Serial.println(gaugeSettings.coolantWarning);
  }

  myCan.begin();
  myCan.setBaudRate(CAN_BAUD);

  // Set encoder pins as input with internal pull-up resistors enabled
  pinMode(RBUTTON_INT, INPUT);
  digitalWrite(RBUTTON_INT, HIGH);
  attachInterrupt(RBUTTON_INT, ISR_debounce, FALLING);

  // By default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR);

  // Show splashscreen
  display.clearDisplay();
  display.drawBitmap(0,0, miata_logo, 128, 64, 1);
  display.display();

  FastLED.addLeds<NEOPIXEL, LEDPIN>(leds, NUM_LEDS);

  // Ring initialization animation
  if (gaugeSettings.LEDRingEnable)
  {
    for(int i = 0; i < NUM_LEDS; i++)
    {
      leds[i].setRGB(16,16,16);
      FastLED.show();
      delay(20);
    }
    delay(200);
    // TODO: Don't like this magic number, maybe have this tied to some brightness variable?
    for (int j = 16; j > -1; j--) 
    {
      for(int i = 0; i < NUM_LEDS; i++)
      {
        leds[i].setRGB(j, j, j);
      }
      FastLED.show();
      delay(20);
    }
  }

  myEnc.begin(ENC_PIN_1, ENC_PIN_2);
  buttonPressed = false;

  delay(1000);
  digitalWrite(TEENSY_LED, 0);
}

// -------------------------------------------------------------
void loop(void)
{
  if (ledTimer.check() && digitalRead(TEENSY_LED))
  {
    digitalWrite(TEENSY_LED, 0);
    ledTimer.reset();
  }
  if (gaugeBlinkTimer.check())
  {
    gaugeBlink = !gaugeBlink;
    gaugeBlinkTimer.reset();
  }

  // See if we have gotten any CAN messages in the last second. display an error if not
  if (commTimer.check() && !DEBUG_MODE)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,56);
    display.println("Waiting for data...");
    display.setCursor(0,0);
    display.display();

    no_light();
    commTimer.reset();
    connectionState = false;
  }

  // main display routine
  if ((connectionState && displayTimer.check()) || DEBUG_MODE)
  {
    menu_check();

    if (gaugeSettings.LEDRingEnable)
    {
      shift_light();
    }
    else
    {
      no_light();
    }

    if (menuState.inMenu)
    {
      menuState.inSettings ? display_settings() : display_menu();
    }
    else
    {
      if (value_oob() && gaugeSettings.warningsEnable)
      {
        gauge_warning();
      }
      else
      {
        switch (static_cast<ViewsMenu>(menuState.menuPos))
        {
        case Dashboard:
          gauge_dashboard();
          break;
        case Single:
          gauge_single();
          break;
        case Graph:
          gauge_graph();
          break;
        default:
          Serial.println("Invalid view!");
        }
      } 
    }
    display.display();
    displayTimer.reset();
  }

  // handle received CAN frames
  if (myCan.read(rxmsg) && !DEBUG_MODE)
  {
    commTimer.reset();
    connectionState = true;
    ledBlink();

    read_CAN_message();
  }
}

// interrupt handler for the encoder button
void ISR_debounce ()
{
  // TODO: Probably shouldn't delay inside of an interrupt
  if((unsigned long)(millis() - last_millis) >= (DEBOUNCING_TIME * 10))
  {
    buttonPressed = true;

    // Return to a safe state after button press
    encoderIndex = 0;
    myEnc.setValue(0);
  }
  else
  {
    //end button
    return;
  }
  last_millis = millis();
}

void divby10(int val)
{
  byte length;

  itoa(val, tempchars, 10);
  length=strlen(tempchars);

  tempchars[length + 1]=tempchars[length]; // null shift right
  tempchars[length]=tempchars[length - 1];
  tempchars[length - 1]='.';
}

void read_CAN_message()
{
  // ID's 1520+ are Megasquirt CAN broadcast frames
  switch (rxmsg.id)
  {
  case 1520: // 0
    gaugeData.RPM = (int)(word(rxmsg.buf[6], rxmsg.buf[7]));
    break;
  case 1521: // 1
    gaugeData.SPKADV = (int)(word(rxmsg.buf[0], rxmsg.buf[1]));
    gaugeData.engine = rxmsg.buf[3];
    gaugeData.AFR_tar = (int)(word(0x00, rxmsg.buf[4]));
    break;
  case 1522: // 2
    gaugeData.Baro = (int)(word(rxmsg.buf[0], rxmsg.buf[1]));
    gaugeData.MAP = (int)(word(rxmsg.buf[2], rxmsg.buf[3]));
    gaugeData.MAT = (int)(word(rxmsg.buf[4], rxmsg.buf[5]));
    gaugeData.CLT = (int)(word(rxmsg.buf[6], rxmsg.buf[7]));
    break;
  case 1523: // 3
    gaugeData.TPS = (int)(word(rxmsg.buf[0], rxmsg.buf[1]));
    gaugeData.BATTV = (int)(word(rxmsg.buf[2], rxmsg.buf[3]));
    break;
  case 1524: // 4
    gaugeData.Knock = (int)(word(rxmsg.buf[0], rxmsg.buf[1]));
    gaugeData.EGOc = (int)(word(rxmsg.buf[2], rxmsg.buf[3]));
    break;
  case 1526: // 6
    gaugeData.IAC = (int)(word(rxmsg.buf[6], rxmsg.buf[7])); //IAC = (IAC * 49) / 125;
  case 1529: // 9
    gaugeData.dwell = (int)(word(rxmsg.buf[4], rxmsg.buf[5]));
    break;
  case 1530: // 10
    gaugeData.status1 = rxmsg.buf[0];
    gaugeData.status2 = rxmsg.buf[1];
    gaugeData.status3 = rxmsg.buf[2];
    gaugeData.status6 = rxmsg.buf[6];
    gaugeData.status7 = rxmsg.buf[7];
    break;
  case 1537: // 17
    gaugeData.bstduty = (int)(word(rxmsg.buf[4], rxmsg.buf[5]));
    break;
  case 1548: // 28
    gaugeData.idle_tar = (int)(word(rxmsg.buf[0], rxmsg.buf[1]));
    break;
  case 1551: // 31
    gaugeData.AFR = (int)(word(0x00, rxmsg.buf[0]));
    break;
  case 1574: // 54
    gaugeData.CEL = rxmsg.buf[2];
    break;
  default: 
    // not a broadcast packet
    // assume this is a normal Megasquirt CAN protocol packet and decode the header
    if (rxmsg.flags.extended)
    {
      rxmsg_id.i = rxmsg.id;
      // is this being sent to us?
      if (rxmsg_id.values.to_id == myCANid)
      {
        switch (rxmsg_id.values.msg_type)
        {
        case 1: // MSG_REQ - request data
          // the data required for the MSG_RSP header is packed into the first 3 data bytes
          msg_req_data.bytes.b0 = rxmsg.buf[0];
          msg_req_data.bytes.b1 = rxmsg.buf[1];
          msg_req_data.bytes.b2 = rxmsg.buf[2];
          // Create the tx packet header
          txmsg_id.values.msg_type = 2; // MSG_RSP
          txmsg_id.values.to_id = msCANid; // Megasquirt CAN ID should normally be 0
          txmsg_id.values.from_id = myCANid;
          txmsg_id.values.block = msg_req_data.values.varblk;
          txmsg_id.values.offset = msg_req_data.values.varoffset;
          txmsg.flags.extended = 1;
          txmsg.id = txmsg_id.i;
          txmsg.len = 8;
          // Use the same block and offset as JBPerf IO expander board for compatibility reasons
          // Docs at http://www.jbperf.com/io_extender/firmware/0_1_2/io_extender.ini (or latest version)

          // realtime clock
          if (rxmsg_id.values.block == 7 && rxmsg_id.values.offset == 110)
          {
            /*
              rtc_sec          = scalar, U08,  110, "", 1,0
              rtc_min          = scalar, U08,  111, "", 1,0
              rtc_hour         = scalar, U08,  112, "", 1,0
              rtc_day          = scalar, U08,  113, "", 1,0 // not sure what "day" means. seems to be ignored...
              rtc_date         = scalar, U08,  114, "", 1,0
              rtc_month        = scalar, U08,  115, "", 1,0
              rtc_year         = scalar, U16,  116, "", 1,0
            */

            // only return clock info if the local clock has actually been set (via GPS or RTC)
            if (timeStatus() == timeSet)
            {
              txmsg.buf[0] = second();
              txmsg.buf[1] = minute();
              txmsg.buf[2] = hour();
              txmsg.buf[3] = 0;
              txmsg.buf[4] = day();
              txmsg.buf[5] = month();
              txmsg.buf[6] = year() / 256;
              txmsg.buf[7] = year() % 256;
              // send the message!
              myCan.write(txmsg);
            }
          } 
        }
      }
    }
    else
    {
      Serial.write("ID: ");
      Serial.print(rxmsg.id);
    }
  }
}

boolean value_oob()
{
  if (gaugeData.RPM > 100)
  {
    if ((gaugeData.CLT/10) > gaugeSettings.coolantWarning) return true;
    // if (gaugeData.CEL != 0) return true;
    if (bitRead(gaugeData.status2,6)) return true; // overboost
  } 
  return false;
}

void menu_check()
{
  // Pressing the button brings up the menu or selects position
  if (buttonPressed)
  {
    // Settings Menu
    if (menuState.inSettings)
    {
      // Exit is always the last item
      if (menuState.settingsPos == NUM_SETTINGS - 1)
      {
        menuState.settingsPos = 0;
        menuState.inSettings = false;
        myEnc.setLimits(0, NUM_VIEWS - 1, true);
        myEnc.setValue(menuState.menuPos);

        if (gaugeSettings.dirty)
        {
          if (DEBUG_MODE)
          {
            Serial.println("Writing to EEPROM!");
          }

          EEPROM.write(RING_ENABLE_ADDR, gaugeSettings.LEDRingEnable);
          EEPROM.write(SHIFT_RPM_ADDR, gaugeSettings.shiftRPM);
          EEPROM.write(SHIFT_RPM_ADDR + 1, gaugeSettings.shiftRPM >> 8);
          EEPROM.write(WARN_ENABLE_ADDR, gaugeSettings.warningsEnable);
          EEPROM.write(CLT_WARN_ADDR, gaugeSettings.coolantWarning);
          EEPROM.write(CLT_WARN_ADDR + 1, gaugeSettings.coolantWarning >> 8);
          gaugeSettings.dirty = false;
        }
      }
      // Setting is selected
      else
      {
        // Confirm settings value
        if (menuState.settingSelect)
        {
          menuState.settingSelect = false;
          myEnc.setLimits(0, NUM_SETTINGS - 1, true);
          myEnc.setValue(menuState.settingsPos);
        }
        // A setting is selected
        else
        {
          menuState.settingSelect = true;
          switch (static_cast<SettingMenu>(menuState.settingsPos))
          {
          case LedRingEnable:
            myEnc.setLimits(0, 1, true);
            myEnc.setValue(gaugeSettings.LEDRingEnable);
            break;
          case ShiftRPM:
            myEnc.setLimits(MIN_RPM/RPM_INTERVAL, MAX_RPM/RPM_INTERVAL, true);
            myEnc.setValue(gaugeSettings.shiftRPM/RPM_INTERVAL);
            break;
          case WarningsEnable:
            myEnc.setLimits(0, 1, true);
            myEnc.setValue(gaugeSettings.warningsEnable);
            break;
          case CoolantWarning:
            myEnc.setLimits(0, MAX_CLT, true);
            myEnc.setValue(gaugeSettings.coolantWarning);
            break;
          }
        }
      }
    }
    // Views Menu
    else
    {
      // Settings is always the last item
      if (menuState.menuPos == NUM_VIEWS - 1)
      {
        menuState.inSettings = true;
        myEnc.setLimits(0, NUM_SETTINGS - 1, true);
      }
      else
      {
        if (!menuState.inMenu)
        {
          menuState.inMenu = true;
          myEnc.setLimits(0, NUM_VIEWS - 1, true);
          myEnc.setValue(menuState.menuPos);
        }
        else
        {
          menuState.inMenu = false;
          switch (static_cast<ViewsMenu>(menuState.menuPos))
          {
          case Dashboard:
            break;
          case Single:
            myEnc.setLimits(0, NUM_GAUGES - 1, true);
            myEnc.setValue(menuState.gaugeSinglePos);
            break;
          case Graph:
            myEnc.setLimits(0, NUM_GRAPHS - 1, true);
            myEnc.setValue(menuState.gaugeGraphPos);
            break;
          }
        }
      }
    }
    buttonPressed = false;
  }
}

void display_menu()
{
  // Check for rotations
  if (myEnc.valueChanged())
  {
    encoderIndex=myEnc.getValue();
    menuState.menuPos = encoderIndex;

    if (DEBUG_MODE)
    {
      Serial.print("Encoder: ");
      Serial.print(encoderIndex);
      Serial.print("Menu: ");
      Serial.println(menuState.menuPos);
    }
  }

  display.clearDisplay();
  print_menu(VIEWS, NUM_VIEWS, menuState.menuPos);
  display.display();
}

void display_settings()
{
  // Check for rotations
  if (myEnc.valueChanged())
  {
    encoderIndex=myEnc.getValue();
    if (menuState.settingSelect)
    {
      switch (static_cast<SettingMenu>(menuState.settingsPos))
      {
      case LedRingEnable:
        gaugeSettings.LEDRingEnable = encoderIndex;
        break;
      case ShiftRPM:
        gaugeSettings.shiftRPM = encoderIndex * RPM_INTERVAL;
        break;
      case WarningsEnable:
        gaugeSettings.warningsEnable = encoderIndex;
        break;
      case CoolantWarning:
        gaugeSettings.coolantWarning = encoderIndex;
        break;
      }
      gaugeSettings.dirty = true;      
    }
    else
    {
      menuState.settingsPos = encoderIndex;
    }

    if (DEBUG_MODE)
    {
      Serial.print("Encoder: ");
      Serial.print(encoderIndex);
      Serial.print("Settings: ");
      Serial.println(menuState.settingsPos);
    }
  }

  display.clearDisplay();

  display.setTextSize(1);
  for (int i = 0; i < NUM_SETTINGS; i++)
  {
    if (menuState.settingsPos == i && !menuState.settingSelect)
    {
      display.setTextColor(BLACK, WHITE);
    }
    else
    {
      display.setTextColor(WHITE);
    }

    display.setCursor(2, (10 * i) + 2);
    display.print(SETTINGS[i]);

    if (menuState.settingsPos == i && menuState.settingSelect)
    {
      display.setTextColor(BLACK, WHITE);   
    }
    else
    {
      display.setTextColor(WHITE);       
    }

    display.setCursor(100, (10 * i) + 2);
    switch (static_cast<SettingMenu>(i))
    {
    case LedRingEnable:
      display.print(gaugeSettings.LEDRingEnable ? "On" : "Off");
      break;
    case ShiftRPM:
      display.print(gaugeSettings.shiftRPM);
      break;
    case WarningsEnable:
      display.print(gaugeSettings.warningsEnable ? "On" : "Off"); 
      break;
    case CoolantWarning:
      display.print(gaugeSettings.coolantWarning);
      break;
    }
  }

  display.display();
}

void gauge_warning()
{
  byte dlength;
  int midpos;

  display.clearDisplay();

  if ((gaugeData.CLT/10) > gaugeSettings.coolantWarning)
  {
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("WARNING");

    dlength = 3;
    midpos = (63 - ((dlength * 23) / 2));
    display.setCursor(midpos, 16);
    display.setTextSize(4);
    display.print(gaugeData.CLT / 10);

    display.setTextSize(2);
    display.setCursor(0, 48);
    display.print("CLT");
  }

  display.display();

  // if (bitRead(gaugeData.CEL, 0))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("MAP");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if (bitRead(gaugeData.CEL, 1))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("MAT");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");

  // }

  // if (bitRead(gaugeData.CEL, 2))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("CLT");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if (bitRead(gaugeData.CEL, 3))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("TPS");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");

  // }

  // if (bitRead(gaugeData.CEL, 4))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("BATT");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if (bitRead(gaugeData.CEL, 5))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("AFR");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if (bitRead(gaugeData.CEL, 6))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("Sync");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if (bitRead(gaugeData.CEL, 7))
  // {
  //   display.setTextColor(WHITE);
  //   dlength=3;
  //   midpos=(63 - ((dlength * 23) / 2));
  //   display.setCursor(29, 0);
  //   display.setTextSize(4);
  //   display.print("EGT");
  //   display.setTextSize(2);
  //   display.setCursor(8, 48);
  //   display.print("Error");
  // }

  // if ( bitRead(gaugeData.status2,6) == 1)
  // {
  //   gauge_danger();
  // }
}

void gauge_dashboard()
{
  //hard coded for style
  // fonts are 5x7 * textsize
  // size 1 .. 5 x 7
  // size 2 .. 10 x 14
  //Vitals - AFR, RPM, MAP,
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.setCursor(41, 0); // 4 char normally - 4 * 10 = 40, - 128 = 88, /2 = 44
  display.println(gaugeData.RPM);
  display.setTextSize(1);
  display.setCursor(21, 7);
  display.print("RPM");

  //line2
  display.setCursor(0, 26);
  display.setTextSize(1);
  display.print("AFR");
  display.setCursor(20, 19);
  display.setTextSize(2);
  divby10(gaugeData.AFR);
  display.print(tempchars);

  display.setCursor(72, 25);
  display.setTextSize(1);
  display.print("CLT");
  display.setCursor(92, 18);
  display.setTextSize(2);
  display.print(gaugeData.CLT/10);

  //line3
  display.setCursor(0, 40);
  display.setTextSize(1);
  display.print("MAP");
  display.setCursor(20, 40);
  display.setTextSize(2);
  display.print(gaugeData.MAP/10);

  // contextual gauge - if idle on, show IAC%
  if ( bitRead(gaugeData.status2,7) == 1)
  {
    display.setCursor(72, 40);
    display.setTextSize(1);
    display.print("IAC");
    display.setCursor(92, 40);
    display.setTextSize(2);
    display.print(gaugeData.IAC);
  }
  else if (gaugeData.MAP > gaugeData.Baro)
  {
    int psi;
    display.setCursor(72, 38);
    display.setTextSize(1);
    display.print("PSI");
    display.setCursor(92, 38);
    display.setTextSize(1);
    // 6.895kpa = 1psi
    psi = gaugeData.MAP - gaugeData.Baro;
    psi=(psi * 200) / 1379;
    divby10(psi);
    display.print(tempchars);

    display.setCursor(72, 40);
    display.setTextSize(1);
    display.print("MAT");
    display.setCursor(92, 47);
    display.print(gaugeData.MAT/10);

  }
  else
  {
    display.setCursor(72, 40);
    display.setTextSize(1);
    display.print("MAT");
    display.setCursor(92,40);
    display.setTextSize(2);
    display.print(gaugeData.MAT/10);
  }

  gauge_bottom();
}

void gauge_bottom()
{
  display.setTextSize(1);
  display.drawFastHLine(1, (63 - 7), 126, WHITE);
  display.setCursor(0, 57);
  display.setTextColor(BLACK, WHITE);

  //CEL
  if ( gaugeData.CEL != 0 )
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(2, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(3, 57);
  display.print("CEL");
  display.drawFastVLine(1, 57, 8, WHITE);

  //Fan
  if ( bitRead(gaugeData.status6,6) == 1)
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(23, 57, 8, WHITE);
    display.drawFastVLine(22, 57, 8, WHITE);
    display.drawFastVLine(42, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(24, 57);
  display.print("Fan");
  display.drawFastVLine(21, 57, 8, WHITE);

  //Idle
  if ( bitRead(gaugeData.status2,7) == 1)
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(44, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(45, 57);
  display.print("Idl");
  display.drawFastVLine(43, 57, 8, WHITE);

  //Knock
  if ( bitRead(gaugeData.status7,4) == 1)
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(65, 57, 8, WHITE);
    display.drawFastVLine(64, 57, 8, WHITE);
    display.drawFastVLine(84, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(66, 57);
  display.print("Knk");
  display.drawFastVLine(63, 57, 8, WHITE);

  //Overboost
  if ( bitRead(gaugeData.status2,6) == 1)
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(87, 57, 8, WHITE);
    display.drawFastVLine(86, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(88, 57);
  display.print("Bst");
  display.drawFastVLine(85, 57, 8, WHITE);

  //WUE
  if ( bitRead(gaugeData.engine,3) == 1)
  {
    display.setTextColor(BLACK, WHITE);
    display.drawFastVLine(107, 57, 8, WHITE);
  }
  else
  {
    display.setTextColor(WHITE);
  }
  display.setCursor(108, 57);
  display.print("WUE");
  display.drawFastVLine(106, 57, 8, WHITE);
  display.drawFastVLine(126, 57, 8, WHITE);

  // FAN, WUE, ASE, CEL, Idl, Knk, over boost
  // CEL - Idl - FAN - KnK - BST - AFR
  display.display();
}

void gauge_single()
{
  char data[10];
  String label;
  // byte temp_index;
  display.clearDisplay();

  // Check for rotations
  if (myEnc.valueChanged())
  {
    encoderIndex=myEnc.getValue();
    menuState.gaugeSinglePos = encoderIndex;

    if (DEBUG_MODE)
    {
      Serial.print("Encoder: ");
      Serial.print(encoderIndex);
      Serial.print("Single: ");
      Serial.println(menuState.gaugeSinglePos);
    }
  }

  switch (static_cast<Gauges>(menuState.gaugeSinglePos))
  {
    case RPM:
      label="RPM";
      itoa(gaugeData.RPM, data, 10);
      break;
    case AFR:
      label="AFR";
      divby10(gaugeData.AFR);
      strcpy(data, tempchars);
      break;
    case Coolant:
      label="Coolant";
      divby10(gaugeData.CLT);
      strcpy(data, tempchars);
      break;
    case MAP:
      label="MAP";
      divby10(gaugeData.MAP);
      strcpy(data, tempchars);
      break;
    case MAT:
      label="MAT";
      divby10(gaugeData.MAT);
      strcpy(data, tempchars);
      break;
    case Timing:
      label="Timing";
      divby10(gaugeData.SPKADV);
      strcpy(data, tempchars);
      break;
    case Voltage:
      label="Voltage";
      divby10(gaugeData.BATTV);
      strcpy(data, tempchars);
      break;
    case TPS:
      label="TPS";
      divby10(gaugeData.TPS);
      strcpy(data, tempchars);
      break;
    case Knock:
      label="Knock";
      divby10(gaugeData.Knock);
      strcpy(data, tempchars);
      break;
    case Barometer:
      label="Barometer";
      divby10(gaugeData.Baro);
      strcpy(data, tempchars);
      break;
    case EGOCorrection:
      label="EGO Corr";
      divby10(gaugeData.EGOc);
      strcpy(data, tempchars);
      break;
    case IAC:
      label="IAC";
      itoa(gaugeData.IAC, data, 10);
      break;
    case SparkDwell:
      label="Spark Dwell";
      divby10(gaugeData.dwell);
      strcpy(data, tempchars);
      break;
    case BoostDuty:
      label="Boost Duty";
      itoa(gaugeData.bstduty, data, 10);
      break;
    case IdleTarget:
      label="Idle Target";
      itoa(gaugeData.idle_tar, data, 10);
      break;
    case AfrTarget:
      label="AFR Target";
      divby10(gaugeData.AFR_tar);
      strcpy(data, tempchars);
      break;
  }
  // }
  // TODO: I'm not sure if this is even needed. Need to figure out if this data is useful. If so the data should loop properly. Maybe update the UI for this a bit?
  // else
  // {
  //   temp_index = encoderIndex - 15;
  //   char temporary[15];
  //   byte sbyte, bitp, dbit;
  //   strcpy_P(temporary, MSDataBin[temp_index].name);
  //   label=temporary;

  //   sbyte=pgm_read_byte(&MSDataBin[temp_index].sbyte);
  //   bitp=pgm_read_byte(&MSDataBin[temp_index].bitp);
  //   dbit=bitRead(indicator[sbyte], bitp);
  //   if ( dbit == 1 )
  //   {
  //     data[0]='O';
  //     data[1]='n';
  //     data[2]='\0';
  //   }
  //   else
  //   {
  //     data[0]='O';
  //     data[1]='f';
  //     data[2]='f';
  //     data[3]='\0';
  //   }

  // }

  byte dlength=strlen(data);
  int midpos;

  // dlength * (width of font) / 2 -1
  // size 2 = 11
  // size 3 = 17
  // size 4 = 23

  midpos = (63 - ((dlength * 23)/ 2));

  display.setTextColor(WHITE);
  display.setCursor(midpos,0);
  display.setTextSize(4);
  display.print(data);

  display.setTextSize(2);
  display.setCursor(8, (63 - 15));
  display.print(label);

  //Additional data for highest/lowest value
  if (menuState.gaugeSinglePos == 0)
  {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.RPM_highest = gaugeData.RPM;
      validity_window=millis();
    }
    if (gaugeData.RPM > gaugeData.RPM_highest)
    {
      gaugeData.RPM_highest = gaugeData.RPM;
      validity_window=millis();
    }
    display.setTextSize(2);
    display.setCursor((127 - 48), 31);
    display.print(gaugeData.RPM_highest);
  }

  if (menuState.gaugeSinglePos == 1)
  {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.AFR_highest = gaugeData.AFR;
      validity_window=millis();
    }
    if (millis() > (validity_window2 + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.AFR_lowest = gaugeData.AFR;
      validity_window2=millis();
    }
    if (gaugeData.AFR > gaugeData.AFR_highest)
    {
      gaugeData.AFR_highest = gaugeData.AFR;
      validity_window=millis();
    }
    if (gaugeData.AFR < gaugeData.AFR_lowest)
    {
      gaugeData.AFR_lowest = gaugeData.AFR;
      validity_window2=millis();
    }
    display.setTextSize(2);
    display.setCursor(0, 31);
    divby10(gaugeData.AFR_lowest);
    display.print(tempchars);
    display.setCursor((127 - 48), 31);
    divby10(gaugeData.AFR_highest);
    display.print(tempchars);
  }

  if (menuState.gaugeSinglePos == 2)
   {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.CLT_highest = gaugeData.CLT;
      validity_window=millis();
    }
    if (gaugeData.CLT > gaugeData.CLT_highest)
    {
      gaugeData.CLT_highest = gaugeData.CLT;
      validity_window=millis();
    }
    display.setTextSize(2);
    display.setCursor((127 - 60), 31);
    divby10(gaugeData.CLT_highest);
    display.print(tempchars);
  }

  if (menuState.gaugeSinglePos == 3)
  {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.MAP_highest = gaugeData.MAP;
      validity_window=millis();
    }
    if (gaugeData.MAP > gaugeData.MAP_highest)
    {
      gaugeData.MAP_highest = gaugeData.MAP;
      validity_window=millis();
    }
    display.setTextSize(2);
    display.setCursor((127 - 48), 31);
    divby10(gaugeData.MAP_highest);
    display.print(tempchars);
  }

  if (menuState.gaugeSinglePos == 4)
  {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.MAT_highest = gaugeData.MAT;
      validity_window=millis();
    }
    if (gaugeData.MAT > gaugeData.MAT_highest)
    {
      gaugeData.MAT_highest = gaugeData.MAT;
      validity_window=millis();
    }
    display.setTextSize(2);
    display.setCursor((127 - 48), 31);
    divby10(gaugeData.MAT_highest);
    display.print(tempchars);
  }

  if (menuState.gaugeSinglePos == 8)
  {
    if (millis() > (validity_window + 30000))
    {
      // Highest value resets after 30 seconds
      gaugeData.Knock_highest = gaugeData.Knock;
      validity_window=millis();
    }
    if (gaugeData.Knock > gaugeData.Knock_highest)
    {
      gaugeData.Knock_highest = gaugeData.Knock;
      validity_window=millis();
    }
    display.setTextSize(2);
    display.setCursor((127 - 48), 31);
    divby10(gaugeData.Knock_highest);
    display.print(tempchars);
  }
  display.display();
}

void gauge_graph()
{
  byte val;
  
  // Check for rotations
  if (myEnc.valueChanged())
  {
    encoderIndex=myEnc.getValue();
    menuState.gaugeGraphPos = encoderIndex;

    if (DEBUG_MODE)
    {
      Serial.print("Encoder: ");
      Serial.print(encoderIndex);
      Serial.print("Graph: ");
      Serial.println(menuState.gaugeGraphPos);
    }
  }

  // 10hz update time
  if (millis() > (validity_window + 80))
  {
    display.clearDisplay();

    switch (static_cast<Graphs>(menuState.gaugeGraphPos))
    {
    // 0-50 value normalization
    case AFR:
      val = (gaugeData.AFR - 100) / 2;  // real rough estimation here here of afr on a 0-50 scale
      break;
    case MAP:
      val = ((gaugeData.MAP/10) - 30) / 4;
      break;
    case MAT:
      val = (gaugeData.MAT/10) / 4;
      break;
    default:
      val = 0;
    }

    if (val > 50)
    {
      val = 50;
    }

    histogram_index++;
    if (histogram_index >=64)
    {
      histogram_index=0;
    }
    histogram[histogram_index]=val;

    for (byte i = 0; i < 64; i++)
    {
      int x = histogram_index - i;
      if ( x < 0)
      {
        x = 64 + histogram_index - i;
      }
      display.drawFastVLine((128 - (i * 2)), (64 - histogram[x]), 64, WHITE);
      display.drawFastVLine((127 - (i * 2)), (64 - histogram[x]), 64, WHITE);
    }

    display.setCursor(8,0);
    display.setTextSize(2);
    display.setTextColor(WHITE);

    switch (static_cast<Graphs>(menuState.gaugeGraphPos))
    {
    case AFR:
      display.print("AFR ");
      divby10(gaugeData.AFR);
      display.print(tempchars);
      display.drawFastHLine(0, 40, 128, WHITE); // stoich 14.7 line
      for (byte x=1; x < 128; x = x + 2)
      {
        display.drawPixel(x, 40, BLACK);
      }
      break;
    case MAP:
      display.print("MAP ");
      display.print(gaugeData.MAP/10);
      display.drawFastHLine(0, 47, 128, WHITE); // Baro line.. roughly 98kpa
      for (byte x=1; x < 128; x = x + 2)
      {
        display.drawPixel(x, 47, BLACK);
      }
      break;
    case MAT:
      display.print("MAT ");
      display.print(gaugeData.MAT / 10);
      break;
    }

    validity_window = millis();
  }
}

void gauge_danger()
{
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);

  display.setCursor(4,0);
  display.setTextSize(2);
  display.print("Warning");
  display.print("!");
  display.print("!");
  display.print("!");

  display.setCursor(10,28);
  display.print("Danger to");
  display.setCursor(12,45);
  display.println("Manifold");
  display.display();
}

void shift_light()
{
  if (gaugeData.RPM > 4000)
  {
    if (gaugeData.RPM < gaugeSettings.shiftRPM)
    {
      uint8_t currLights = (gaugeData.RPM - 4000) / ((gaugeSettings.shiftRPM - 4000) / NUM_LEDS);

      for (int i = 0; i < currLights; i++)
      {
        leds[i].setRGB(16, 0, 0);
      }
    }
    else
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (gaugeBlink)
        {
          uint8_t red = (255 * neo_brightness) / 16;
          leds[i].setRGB(red, 0, 0);
        }
        else
        {
          leds[i].setRGB(0, 0, 0);
        }
      }
    }
  }
  else
  {
    for(int i = 0; i < NUM_LEDS; i++)
    {
      leds[i].setRGB(0, 0, 0);
    }
  }
  FastLED.show();
}

void warning_light()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (gaugeBlink)
    {
      uint8_t red = (255 * neo_brightness) / 16;
      leds[i].setRGB(red, 0, 0);
    }
    else
    {
      leds[i].setRGB(0, 0, 0);
    }
  }
  FastLED.show();
}

void no_light()
{
  for(int i = 0; i < NUM_LEDS; i++)
  {
    leds[i].setRGB(0, 0, 0);
  }
  FastLED.show();
}

void print_menu(const String items[], int numItems, int currPos)
{
  display.setTextSize(1);
  for (int i = 0; i < numItems; i++)
  {
    if (currPos == i)
    {
      display.setTextColor(BLACK, WHITE);
      display.setCursor(2, (10 * i) + 2);
      display.print(items[i]);
    }
    else
    {
      display.setTextColor(WHITE);
      display.setCursor(2, (10 * i) + 2);
      display.print(items[i]);
    }
  }
}
