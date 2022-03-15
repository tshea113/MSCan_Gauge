#include <avr/pgmspace.h>

// User configuerable constants
const int  kMyCanId      = 10;     // CAN ID of this unit
const int  kMsCanId      = 0;      // CAN ID of the Megasquirt (should almost always be 0)
const bool kDebugMode    = false;  // Debug mode for testing menus, screens, etc.

// Constants
const uint8_t kScreenI2cAddress           = 0x3D;   // I2C address of the OLED screen
const int     kCanTimeout                 = 1000;   // Display an error message if no CAN data during this interval
const int     kDisplayRefresh             = 100;    // Refresh the display at this interval
const int     kLedFlashTimer              = 1;      // How long to flash the led upon CAN frame receive/transmit
const int     kGaugeFlashTimer            = 50;     // Blink the led ring pixels during certain conditions
const int     kNumLeds                    = 16;     // Number of LEDs on the NeoPixel ring
const int     kDebounceTime               = 25;     // Debouncing Time - 150 is good, 200 is better, 250 seems worse
const int     kOledHeight                 = 64;     // Height of the OLED screen in pixels
const int     kOledWidth                  = 128;    // Width of the OLED screen in pixels
const int     kCanBaud                    = 500000; // CAN baud rate
const int     kMsDataNameMaxLength        = 10;     // Maximum length of MS data field name
const int     kMsDataBinNameMaxLength     = 14;     // Maximum length of MS data field name
const int     kButtonInterval             = 10;      // Button debounce time in ms

// Settings Constraints
const int kMinRpm          = 5000;    // Minimum RPM for shift light setting
const int kMaxRpm          = 9900;    // Maximum RPM for shift light setting
const int kRpmInterval     = 100;     // Smallest interval for shift light setting
const int kMaxCoolantTemp  = 300;     // Maximum coolant temp for warning

// Program memory constants
const uint8_t miata_logo [] PROGMEM = 
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x03, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x03, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x07, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x1F, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00,
0x00, 0x3F, 0xFB, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00,
0x00, 0xFF, 0xF1, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00,
0x01, 0xFF, 0xC3, 0xF8, 0x0F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00,
0x03, 0xFF, 0x83, 0xF8, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00,
0x1B, 0xFF, 0x03, 0xF8, 0x7F, 0xC0, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00,
0x1F, 0xFE, 0x07, 0xF1, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x00,
0x3F, 0xFE, 0x07, 0xF3, 0xFF, 0xC0, 0x00, 0x7F, 0x00, 0x3F, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
0x3F, 0xFC, 0x07, 0xF7, 0xFF, 0xC0, 0x00, 0xFF, 0x00, 0x7F, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x00,
0x7F, 0xF8, 0x0F, 0xCF, 0xFF, 0xC0, 0x01, 0xFF, 0x00, 0x7F, 0xFF, 0xFF, 0xF8, 0x00, 0x00, 0x00,
0x7F, 0xF0, 0x1F, 0xDF, 0xFF, 0xC3, 0x01, 0xFF, 0x00, 0x7F, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
0x7F, 0xE0, 0x1F, 0xFF, 0xDF, 0xDF, 0xE0, 0xFF, 0x00, 0x00, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00,
0x7F, 0xE0, 0x3F, 0xFF, 0xBF, 0xFF, 0xF0, 0x7F, 0x00, 0xE0, 0x01, 0xFE, 0x00, 0x00, 0x00, 0x00,
0x3F, 0xC0, 0x3F, 0xFF, 0x3F, 0xFF, 0xF8, 0x0F, 0x01, 0xFC, 0x01, 0xFC, 0x00, 0x00, 0x00, 0x00,
0x1F, 0xC0, 0x7F, 0xFE, 0x3F, 0xFF, 0xF8, 0x07, 0x03, 0xFF, 0xF1, 0xFC, 0x1E, 0x00, 0x00, 0x00,
0x0F, 0x00, 0x7F, 0xFC, 0x3F, 0xFF, 0xFC, 0x1F, 0x07, 0xFF, 0xF1, 0xFC, 0x3F, 0x80, 0x00, 0x00,
0x00, 0x00, 0xFF, 0xF8, 0x7F, 0xFF, 0xFC, 0x3F, 0x07, 0xFF, 0xF1, 0xFC, 0x7F, 0xC0, 0x00, 0x00,
0x00, 0x00, 0xFF, 0xF0, 0x7F, 0xFF, 0xFC, 0x3F, 0x0F, 0xFF, 0xF9, 0xFC, 0xFF, 0xF8, 0x00, 0x00,
0x00, 0x01, 0xFF, 0xE0, 0x7F, 0xFB, 0xFE, 0x1F, 0x9F, 0xFB, 0xF9, 0xFD, 0xFF, 0xFC, 0x00, 0x00,
0x00, 0x01, 0xFF, 0xC0, 0x7F, 0xFB, 0xFE, 0x1F, 0x9F, 0xFB, 0xF9, 0xFD, 0xFF, 0x3F, 0x00, 0x00,
0x00, 0x01, 0xFF, 0xC0, 0x7F, 0xFB, 0xFE, 0x1F, 0xFF, 0xF3, 0xFC, 0xFF, 0xFE, 0x3F, 0x80, 0x00,
0x00, 0x03, 0xFF, 0x80, 0x7F, 0xFB, 0xFE, 0x1F, 0xFF, 0xE1, 0xFC, 0xFF, 0xFC, 0x3F, 0xC0, 0x00,
0x00, 0x03, 0xFF, 0x00, 0x7F, 0xF3, 0xFE, 0x1F, 0xFF, 0xC1, 0xFE, 0xFF, 0xFC, 0x1F, 0xC0, 0x00,
0x00, 0x07, 0xFF, 0x00, 0x3F, 0xF3, 0xFE, 0x3F, 0xFF, 0x81, 0xFF, 0xFF, 0xFC, 0x1F, 0xC0, 0x00,
0x00, 0x07, 0xFE, 0x00, 0x3F, 0xF3, 0xFE, 0x7F, 0xFF, 0x83, 0xFF, 0xFF, 0xF8, 0x1F, 0xE0, 0x00,
0x00, 0x0F, 0xFC, 0x00, 0x1F, 0xE3, 0xFF, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xF8, 0x0F, 0xF8, 0x00,
0x00, 0x0F, 0xFC, 0x00, 0x00, 0xC3, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xFC, 0x1F, 0xFF, 0x00,
0x00, 0x1F, 0xF8, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0x98,
0x00, 0x1F, 0xF0, 0x00, 0x00, 0x01, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x78,
0x00, 0x3F, 0xE0, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xF8,
0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xE0, 0x1F, 0xFF, 0x87, 0xFF, 0xFF, 0xE0, 0xFF, 0xF8,
0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x7F, 0xC0, 0x07, 0xFF, 0x00, 0x01, 0xFF, 0xE0, 0x1F, 0xF0,
0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x03, 0xF6, 0x00, 0x00, 0x0F, 0x80, 0x03, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct MSDataObject
{
  char name[kMsDataNameMaxLength];
  int block;                      // max val 32?
  int offset;                     // max val?
  int reqbytes;                   // max val 8
  int mult;                       // does this need to be * 0.1 ?
};

const MSDataObject MSData[] PROGMEM =
{
// string,  block, offset, reqbytes, mult,      div
  {"RPM",     7,    6,        2,      0   }, // 0
  {"AFR",     7,    252,      1,      0   }, // 1
  {"CLT",     7,    22,       2,      1   }, // 2
  {"MAP",     7,    18,       2,      1   }, // 3
  {"MAT",     7,    20,       2,      1   }, // 4
  {"SPKADV",  7,    8,        2,      1   }, // 5
  {"BATTV",   7,    26,       2,      1   }, // 6
  {"TPS",     7,    24,       2,      1   }, // 7
  {"Knock",   7,    32,       2,      1   }, // 8
  {"Baro",    7,    16,       2,      1   }, // 9
  {"EGOc",    7,    34,       2,      1   }, // 10
  {"IAC",     7,    54,       2,      0   }, // 11 -- this was GFC's to 49 / 125
  {"dwell",   7,    62,       2,      1   }, // 12
  {"bstduty", 7,    39,       1,      0   }, // 13 boost duty cycle
  {"idletar", 7,    380,      2,      0   }, // 14
  {"AFRtgt",  7,    12,       1,      1   }, // 15
};

struct MSDataBinaryObject 
{
  char name[kMsDataBinNameMaxLength];
  uint8_t sbyte;
  uint8_t bitp;
};

const MSDataBinaryObject MSDataBin[] PROGMEM =
{
// "1234567890"
//"name", indicator byte, bit position
//0 engine - block 7 offset 11
  {"Ready",            0,  0 },
  {"Crank",            0,  1 },
  {"ASE",              0,  2 },
  {"WUE",              0,  3 },
  {"TPS acc",          0,  4 },
  {"TPS dec",          0,  5 },
  {"MAP acc",          0,  6 },
  {"MAP dec",          0,  7 },
// 1 status1 - block 7 offset 78
  {"Need Burn",        1,  0,},
  {"Data Lost",        1,  1 },
  {"Config Error",     1,  2 },
  {"Synced",           1,  3 },
  {"VE1/2",            1,  5 },
  {"SPK1/2",           1,  6 },
  {"Full-sync",        1,  7 },
// 2 status2 - block 7 offset 79
  {"N2O 1",            2,  0 },
  {"N2O 2",            2,  1 },
  {"Rev lim",          2,  2 },
  {"Launch",           2,  2 },
  {"Flat shift",       2,  4 },
  {"Spark cut",        2,  5 },
  {"Over boost",       2,  6 },
  {"CL Idle",          2,  7 },
// 3 status3 - block 7 offset 80
  {"Fuel cut",         3,  0 },
//{"T-log",            3,  1 },
//{"3 step",           3,  2 },
//{"Test mode",        3,  3 },
//{"3 step",           3,  4 },
  {"Soft limit",       3,  5 },
//{"Bike shift",       3,  6 },
  {"Launch",           3,  7 },
// 4 check engine lamps - block 7 offset 425
  {"cel_map",          4,  0 },
  {"cel_mat",          4,  1 },
  {"cel_clt",          4,  2 },
  {"cel_tps",          4,  3 },
  {"cel_batt",         4,  4 },
  {"cel_afr0",         4,  5 },
  {"cel_sync",         4,  6 },
  {"cel_egt",          4,  7 },
// 5 port status - block 7 offset 70
  {"port0",            5,  0 },
  {"port1",            5,  1 },
  {"port2",            5,  2 },
  {"port3",            5,  3 },
  {"port4",            5,  4 },
  {"port5",            5,  5 },
  {"port6",            5,  6 },
// 6 status6 - block 7 offset 233
  {"EGT warn",         6,  0 },
  {"EGT shutdown",     6,  1 },
  {"AFR warn",         6,  2 },
  {"AFR shutdown",     6,  3 },
  {"Idle VE",          6,  4 },
  {"Idle VE",          6,  5 },
  {"Fan",              6,  6 },
// 7 status7 - block 7 offset 351
  {"Knock",            7,  4 },
  {"AC",               7,  5 },
};
