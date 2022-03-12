// Pin definitions
#define LEDPIN      5   // Neopixel LED
#define RBUTTON_INT 15  //  
#define ENC_PIN_1   16  //  
#define ENC_PIN_2   17  // 
#define TEENSY_LED  13  // Onboard LED on the Teensy
#define OLED_RESET  20  // OLED Reset

#define SCREEN_WIDTH    128 // OLED display width, in pixels
#define SCREEN_HEIGHT   64 // OLED display height, in pixels

#define MIN_RPM         5000    // Minimum RPM for shift light setting
#define MAX_RPM         9900    // Maximum RPM for shift light setting
#define RPM_INTERVAL    100     // Smallest interval for shift light setting
#define MAX_CLT         300     // Maximum coolant temp for warning