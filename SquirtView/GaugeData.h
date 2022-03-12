// Constants
const int NUM_VIEWS                     = 4;      // Number of gauge views
const int NUM_SETTINGS                  = 5;      // Number of settings
const int NUM_GAUGES                    = 16;     // Number of gauges
const int NUM_GRAPHS                    = 3;      // Number of graph gauges

const String VIEWS[NUM_VIEWS] = {"Dashboard", "Single", "Graph", "Settings"};
const String SETTINGS[NUM_SETTINGS] = {"LED Ring", "Shift RPM", "Warnings", "Coolant Warn", "Exit"};
const String GAUGES[NUM_GAUGES] = {"RPM", "AFR", "Coolant", "MAP", "MAT", "Timing", "Voltage", "TPS", "Knock", "Barometer", "EGO Corr", "IAC", "Spark Dwell", "Boost Duty", "Idle Target", "AFR Target"};
const String GRAPHS[NUM_GRAPHS] = {"AFR", "MAP", "MAT"};

enum ViewsMenu : uint8_t
{
  Dashboard = 0,
  Single = 1,
  Graph = 2,
  Setting = 3
};

enum SettingMenu : uint8_t
{
  LedRingEnable = 0,
  ShiftRPM = 1,
  WarningsEnable = 2,
  CoolantWarning = 3,
  Exit = 4
};

enum Gauges : uint8_t
{
  RPM = 0,
  AFR = 1,
  Coolant = 2,
  MAP = 3,
  MAT = 4,
  Timing = 5,
  Voltage = 6,
  TPS = 7,
  Knock = 8,
  Barometer = 9,
  EGOCorrection = 10,
  IAC = 11,
  SparkDwell = 12,
  BoostDuty = 13,
  IdleTarget = 14,
  AfrTarget = 15
};

enum Graphs : uint8_t
{
  AFR = 0,
  MAP = 1,
  MAT = 2
};

// EEPROM Addresses
const uint8_t EEPROM_INIT       = 0;
const uint8_t RING_ENABLE_ADDR  = 1;
const uint8_t SHIFT_RPM_ADDR    = 2;
const uint8_t WARN_ENABLE_ADDR  = 4;
const uint8_t CLT_WARN_ADDR     = 5;

const uint8_t EEPROM_VALID = 13;

// Data Structures
struct MenuState
{
  bool inMenu = false;
  bool inSettings = false;
  bool settingSelect = false;
  uint8_t menuPos = 0;
  uint8_t settingsPos = 0;
  uint8_t gaugeSinglePos = 0;
  uint8_t gaugeGraphPos = 0;
};

struct Settings
{
  bool dirty = false;
  bool LEDRingEnable = true;
  uint16_t shiftRPM = 6800;
  bool warningsEnable = true;
  uint16_t coolantWarning = 240;
};

struct GaugeData
{
  unsigned int RPM;
  unsigned int CLT;
  unsigned int MAP;
  unsigned int MAT;
  unsigned int SPKADV;
  unsigned int BATTV;
  unsigned int TPS;
  unsigned int Knock;
  unsigned int Baro;
  unsigned int EGOc;
  unsigned int IAC;
  unsigned int dwell;
  unsigned int bstduty;
  unsigned int idle_tar;
  int AFR;
  int AFR_tar;
  unsigned int MAP_highest;
  unsigned int RPM_highest;
  unsigned int CLT_highest;
  unsigned int MAT_highest;
  unsigned int Knock_highest;
  int AFR_highest;
  int AFR_lowest;
  uint8_t engine;
  uint8_t CEL;
  uint8_t status1;
  uint8_t status2;
  uint8_t status3;
  uint8_t status6;
  uint8_t status7;
};