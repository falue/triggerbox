// PIN DECLARATION ----------------------------------------------------------------------------
const int triggerPin = 2;
volatile const int safetySwitchPin = 3;
const int triggerLedPin = 13;
const int rotaryHomePin = A5;
MD_REncoder rotaryEncoder = MD_REncoder(A4, A3);
#define TFT_CS 10
#define TFT_DC 11
#define TFT_RST 12

// VARS ---------------------------------------------------------------------------------------
volatile boolean triggerIsActive = false;
boolean hasShownTriggeronTft = false;
volatile boolean safetyIsOn = false;
long initDelay = 0;  // MICROSECONDS eg. 1000000 (= 1s)
volatile long lastTimeTriggered = 0;
long rotaryOldPosition = 0;
long rotaryCurrentDirection = 0;
int rotaryDirection = 0;
volatile boolean editSettings = false;
int menuSelector = 0;

// Menu system
int relayIndex = 0;
const int numOfMenuIds = 21;
String menuIds[numOfMenuIds] = {
  F("initDelay"),
  F("setRelay-00-startPoint"), F("setRelay-00-endPoint"), F("toggleRelay-00"),
  F("setRelay-01-startPoint"), F("setRelay-01-endPoint"), F("toggleRelay-01"),
  F("setRelay-02-startPoint"), F("setRelay-02-endPoint"), F("toggleRelay-02"),
  F("setRelay-03-startPoint"), F("setRelay-03-endPoint"), F("toggleRelay-03"),
  F("setRelay-04-startPoint"), F("setRelay-04-endPoint"), F("toggleRelay-04"),
  F("setRelay-05-startPoint"), F("setRelay-05-endPoint"), F("toggleRelay-05"),
  F("reset"),
  F("exit")
};
int marginTopTriggerlist = 87;  // from top of display to table of triggers

// DEFINE -------------------------------------------------------------------------------------
#define debug  // More Serial.print statements everywhere

// define struct type data
typedef struct triggerSetup {
  int pin;
  long startPoint;
  long endPoint;
  boolean active;
};

const int numOfTriggers = 6;  // max physical connections
// TIMESTAMPS IN MICROSECONDS.
triggerSetup triggers[numOfTriggers] = {
  {4, 0, 100000, true},
	{5, 1000000, 1200000, true},
	{6, 2000000, 2300000, true},
	{7, 3000000, 3400000, true},
	{8, 4000000, 4500000, true},
	{9, 5000000, 5600000, true},
};

typedef struct action {
  long timestamp;
  int triggerAction;  // number of triggerSetup pins...
  triggerSetup pins[numOfTriggers];  // "pins[numOfTriggers]" USES LOTS OF MEMORY
};

// max possibilities of actions - each trigger an unique start/end timestamp
const int maxActions = numOfTriggers*2;
action actions[maxActions];
int numOfActions = 0;

// TFT
// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
int screenWidth = 0;
int screenHeight = 0;