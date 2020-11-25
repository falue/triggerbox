// LIBRARIES ----------------------------------------------------------------------------------
#include <MD_REncoder.h>
#include <ArduinoSort.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"

// PIN DECLARATION ----------------------------------------------------------------------------
const int triggerPin = 2;
volatile const int safetySwitchPin = 3;
const int triggerLedPin = 13;
const int rotaryHomePin = A5;
MD_REncoder rotaryEncoder = MD_REncoder(0, 1);
#define TFT_CS 10  // tft
#define TFT_DC 11  // tft
#define TFT_RST 12  // tft

// VARS ---------------------------------------------------------------------------------------
volatile boolean triggerIsActive = false;
boolean hasShownTriggeronTft = false;
volatile boolean safetyIsOn = false;
long initDelay = 1234567;  // MICROSECONDS eg. 1000000 (= 1s)
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
  {4, 0, 1000000, true},
	{5, 1000000, 2000000, true},
	{6, 1600000, 2600000, true},
	{7, 2000000, 2200000, true},
	{8, 1000000, 2000000, true},
	{9, 7000000, 8000000, true},

  // test with every second HIGH 0.1 LOW
  /* {4, 0, 100000, false},
	{5, 1000000, 1100000, false},
	{6, 2000000, 2100000, false},
	{7, 3000000, 3100000, false},
	{8, 4000000, 4100000, false},
	{9, 5000000, 5100000, false}, */
};

typedef struct action {
  long timestamp;
  int triggerAction;  // number of triggerSetup pins...
  triggerSetup pins[numOfTriggers];  // "pins[numOfTriggers]" USES 34% MEMORY // DO NOT USE POINTER "*"
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

// ACTIONS FOR INTERRUPTS --------------------------------------------------------------------------
// Push btn: Trigger
void triggerIsPushed() {
  if(millis() > lastTimeTriggered + 500) {
    #ifdef debug
      Serial.println("triggerIsPushed");
    #endif
    if(!editSettings && !safetyIsOn) {
      triggerIsActive = !triggerIsActive;
      if(!triggerIsActive) reset();
    } else {
      triggerIsActive = false;
      Serial.println("Safety first! Cannot trigger when in menu or when safety is on.");
    }
  }
  lastTimeTriggered = millis();
}

// Safety siwtch: two pole single throw
void safetySwitchChanged() {
  safetyIsOn = digitalRead(safetySwitchPin);
  Serial.print("safetyIsOn: ");
  Serial.println(safetyIsOn);
  if(!safetyIsOn) editSettings = false;  // exit menu when switch is thrown in edit mode
  drawLayout();
  // If, during rendering of TFT, switch was changed again, re-run function:
  if(digitalRead(safetySwitchPin) != safetyIsOn) safetySwitchChanged();
}

// SETUP --------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Setup TFT
  tft.begin();
  tft.setRotation(1);
  screenWidth = tft.width();
  screenHeight = tft.height();

  // Draw Welcome screen
  splashScreen();

  delay(1500); // wait for serial monitor to catch up...
  Serial.println("~~:::=[ TRIGGERBOX ]=:::~~");

  // Setup buttons
  pinMode(rotaryHomePin, INPUT);
  pinMode(triggerPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(triggerPin), triggerIsPushed, RISING);
  pinMode(safetySwitchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(safetySwitchPin), safetySwitchChanged, CHANGE);

  // Setup rotary encoder
  rotaryEncoder.begin();

  // Set pinMode for relays
  for(int i = 0; i < numOfTriggers; i++) {
    /* Serial.print("triggers[i].pin: ");
    Serial.println(triggers[i].pin); */
    pinMode(triggers[i].pin, OUTPUT);
  }

  // TODO: Load "triggers" and settings (initDelay) from internal SPI Flash memory
  // https://learn.adafruit.com/adafruit-metro-m0-express-designed-for-circuitpython/using-spi-flash
  getDataFromMemory();

  // Initial save of settings
  calculateActions();
  printSettings();
  printActions();
  printTriggers();

  safetyIsOn = digitalRead(safetySwitchPin);

  // Create base layout
  drawLayout();
};


// LOOP ---------------------------------------------------------------------------------------
void loop() {
  editSettings = digitalRead(rotaryHomePin);
  if(editSettings && !safetyIsOn) {
    drawLock();
    drawFooter();  // redraw because buttons show up
    drawInitDelayButton();  // redraw because button is now selected
    Serial.println("---------");
    Serial.println("~~:::=[ TRIGGERBOX MENU ]=:::~~");
    Serial.println("---------");
    Serial.println("Set initial delay..");
    // menuSelector = 0; do not reset menu position. but i could :) or should? pointer always on "save" or "exit"
    delay(250); // wait for button to be released
  }

  while(editSettings) {
    menu();
    // If inside menu() exit was selected, calculate actions
    if(!editSettings) {
      calculateActions();
      printSettings();
      printActions();
      printTriggers();
      saveDataToMemory();
      drawLayout();
      Serial.println("Triggers and settings saved.");
    }
  }
  
  // Ff trigger pushbtn is pressed
  if(triggerIsActive) {  //  
    Serial.println("---------");
    triggerAction();
  }
  delay(100);  // Cut the loop() some slack
};


// ACTIONS ------------------------------------------------------------------------------------
void menu() {

  // Names of menu items to display
  String menuDisplay[numOfMenuIds] = {
    F("Set initial delay.."),
    F("setRelay-00-startPoint"), F("setRelay-00-endPoint"), F("toggleRelay-00"),
    F("setRelay-01-startPoint"), F("setRelay-01-endPoint"), F("toggleRelay-01"),
    F("setRelay-02-startPoint"), F("setRelay-02-endPoint"), F("toggleRelay-02"),
    F("setRelay-03-startPoint"), F("setRelay-03-endPoint"), F("toggleRelay-03"),
    F("setRelay-04-startPoint"), F("setRelay-04-endPoint"), F("toggleRelay-04"),
    F("setRelay-05-startPoint"), F("setRelay-05-endPoint"), F("toggleRelay-05"),
    F("Set all to default"),
    F("Save and exit")
  };

  String selectedMenu = menuIds[menuSelector];
  String selectedMenuDisplay = menuDisplay[menuSelector];

  rotaryDirection = readRotaryEncoder(false);
  if(rotaryDirection != 0) {
    //Serial.println(rotaryDirection);
    menuSelector += rotaryDirection;
    if(menuSelector > numOfMenuIds-1) menuSelector = 0;
    if(menuSelector < 0) menuSelector = numOfMenuIds-1;
    // menu level
    selectedMenu = menuIds[menuSelector];
    selectedMenuDisplay = menuDisplay[menuSelector];
    /* Serial.print("\t-->\tnumOfMenuIds: ");
    Serial.print(numOfMenuIds); */
    /* Serial.print("menuSelector: ");
    Serial.print(menuSelector);
    Serial.print("\t-->\t"); */
    /* Serial.print("Id: ");
    Serial.print(selectedMenu);
    Serial.print("\t->\tDisplay: ");
    Serial.print(selectedMenuDisplay);
    Serial.print("\t->\tCalc.Display: "); */

    drawFooterButtons();  // redraw buttons to select or deselect
    drawInitDelayButton();  // redraw buttons to select or deselect

    // highlight relay parts
    if(selectedMenu.startsWith("setRelay") || selectedMenu.startsWith("toggleRelay")) {
      if(selectedMenu.startsWith("setRelay")) {
        relayIndex = selectedMenu.substring(9, 11).toInt();  // i know...
      } else {
        relayIndex = selectedMenu.substring(12, 14).toInt();  // i know...
      }
      if(relayIndex - 1 >= 0) drawTrigger(relayIndex-1);  // clear previous highlight
      drawTrigger(relayIndex);  // Draw to display
      if(relayIndex + 1 < numOfTriggers) drawTrigger(relayIndex+1);  // clear next highlight
      Serial.println(getRelayMenuItem(relayIndex, selectedMenuDisplay));  
    } else {
      Serial.println(selectedMenuDisplay);
    }

    if(selectedMenu == "initDelay") {  // redraw selection of trigger
      drawTrigger(0);
    }
    if(selectedMenu == "reset") {  // redraw selection of trigger
      drawTrigger(numOfTriggers-1);
    }
  }

  // Edit initDelay
  if(digitalRead(rotaryHomePin) && selectedMenu == "initDelay") {
    drawPopUp("Set initial delay");
    drawPopUpContent(floatMicrosToString(initDelay, "s"));
    delay(250);  // wait for button to be depressed
    Serial.print("Edit initDelay:\t");
    Serial.println(initDelay);
    long lastTimeMoved = millis();
    while(!digitalRead(rotaryHomePin)) {
      long reading = readRotaryEncoder(true);
      if(reading != 0) {
        initDelay += reading;
        initDelay = initDelay >= 0 ? initDelay : 0;
        /* Serial.print(initDelay);
        Serial.println("μs ("+String(initDelay / 1000000.0)+"..s)"); */
        lastTimeMoved = millis();
      }
      // to preserve inertia of rotary encoder, only safe this once in a while
      if(millis() - lastTimeMoved > 200 && lastTimeMoved != 0) {
        drawPopUpContent(floatMicrosToString(initDelay, "s"));
        lastTimeMoved = 0;
      }
    }
    initDelay = initDelay < 2147483647 ? initDelay : 2147483647;  // max size of type long (35.79min)
    Serial.print("Saved initDelay:\t");
    Serial.print(initDelay);
    Serial.println("μs ("+String(initDelay / 1000000.0)+"..s)");
    drawLayout();
    delay(250);  // wait for btn release
  }

  // Edit start & end timestamps of relays
  if(digitalRead(rotaryHomePin) && selectedMenu.startsWith("setRelay")) {
    String relayTimepointToEdit = selectedMenu.substring(12);
    delay(250);  // wait for button to be depressed
    //int relayIndex = selectedMenu.substring(9, 11).toInt();  // i know, i know
    long lastTimeMoved = millis();

    if(relayTimepointToEdit == "startPoint") {
      drawPopUp("Set startpoint #" + String(relayIndex+1));
      drawPopUpContent(floatMicrosToString(triggers[relayIndex].startPoint, "s"));
      Serial.println("Edit start point:");
      long initialGap = triggers[relayIndex].endPoint - triggers[relayIndex].startPoint;
      while(!digitalRead(rotaryHomePin)) {
        long reading = readRotaryEncoder(true);
        if(reading != 0) {
          triggers[relayIndex].startPoint += reading;
          triggers[relayIndex].startPoint = triggers[relayIndex].startPoint >= 0
            ? triggers[relayIndex].startPoint
            : 0;
          /* Serial.print(triggers[relayIndex].startPoint);
          Serial.println("μs ("+String(triggers[relayIndex].startPoint / 1000000.0)+"..s)"); */
          lastTimeMoved = millis();
        }
        // to preserve inertia of rotary encoder, only safe this once in a while
        if(millis() - lastTimeMoved > 200 && lastTimeMoved != 0) {
          drawPopUpContent(floatMicrosToString(triggers[relayIndex].startPoint, "s"));
          lastTimeMoved = 0;
        }
      }
      triggers[relayIndex].startPoint =
        triggers[relayIndex].startPoint < 2147483647 - initialGap
          ? triggers[relayIndex].startPoint
          : 2147483647 - initialGap;  // max size of type long (35.79min) - minus initialGap than endpoint
      // re-set endpoint
      triggers[relayIndex].endPoint = triggers[relayIndex].startPoint + initialGap;
      Serial.print("Saved startPoint");
      Serial.print(triggers[relayIndex].startPoint);
      Serial.println("μs ("+String(triggers[relayIndex].startPoint / 1000000.0)+"..s)");

    } else {
      drawPopUp("Set duration #" + String(relayIndex+1));
      drawPopUpContent(floatMicrosToString(triggers[relayIndex].endPoint - triggers[relayIndex].startPoint, "s"));
      Serial.println("Edit end point:");
      while(!digitalRead(rotaryHomePin)) {
        long reading = readRotaryEncoder(true);
        if(reading != 0) {
          triggers[relayIndex].endPoint += reading;
          triggers[relayIndex].endPoint = triggers[relayIndex].endPoint > triggers[relayIndex].startPoint
            ? triggers[relayIndex].endPoint
            : triggers[relayIndex].startPoint+1;  // Always at least 1 micros later to turn off
          /* Serial.print(triggers[relayIndex].endPoint);
          Serial.println("μs ("+String(triggers[relayIndex].endPoint / 1000000.0)+"..s)"); */
          lastTimeMoved = millis();
        }
        // to preserve inertia of rotary encoder, only safe this once in a while
        if(millis() - lastTimeMoved > 200 && lastTimeMoved != 0) {
          drawPopUpContent(floatMicrosToString(triggers[relayIndex].endPoint - triggers[relayIndex].startPoint, "s"));
          lastTimeMoved = 0;
        }
      }
      triggers[relayIndex].endPoint =
        triggers[relayIndex].endPoint < 2147483647
          ? triggers[relayIndex].endPoint
          : 2147483647;  // max size of type long (35.79min)
      Serial.print("Saved endPoint ");
      Serial.print(triggers[relayIndex].endPoint);
      Serial.println("μs ("+String(triggers[relayIndex].endPoint / 1000000.0)+"..s)");
    }
    drawLayout();
    delay(250);  // wait for btn release
  }

  if(digitalRead(rotaryHomePin) && selectedMenu.startsWith("toggleRelay")) {
    //  relayIndex = selectedMenu.substring(12, 14).toInt();  // gets overwritten
    Serial.print("Edit active nr. ");
    Serial.print("di/enable ");
    Serial.println(relayIndex);
    triggers[relayIndex].active = !triggers[relayIndex].active;
    delay(250);  // wait for btn release
    editSettings = false;
  }

  // Reset to default
  if(digitalRead(rotaryHomePin) && selectedMenu == "reset") {
    initDelay = 0;
    triggers[0] = {4, 0, 1000000, true};
    triggers[1] = {5, 1000000, 2000000, true};
    triggers[2] = {6, 1600000, 2600000, true};
    triggers[3] = {7, 2000000, 2200000, true};
    triggers[4] = {8, 1000000, 2000000, true};
    triggers[5] = {9, 7000000, 8000000, true};

    delay(250);  // wait for btn release
    editSettings = false;
  }

  // Exit menu
  if(digitalRead(rotaryHomePin) && (selectedMenu == "exit")) {
    Serial.println("Exit menu.");
    delay(250);  // wait for btn release
    editSettings = false;
  }
}

long readRotaryEncoder(boolean inertia) {
  uint8_t reading = rotaryEncoder.read();
  if (reading) {
    int speed = rotaryEncoder.speed();
    int direction = reading == DIR_CW ? 1 : -1;
    // /10
    long step = inertia ? (pow(10, int(speed/5))+.99) * direction : direction;  // speedramps! +.99 to round up int
    /* Serial.print(direction);
    Serial.print("\t");
    Serial.print(speed);  // linear
    Serial.print("\t");
    Serial.println(step); */
    return step;
  } else {
    return 0;
  }
}

String getRelayMenuItem(int relayIndex, String nameOfMenu) {
  //int relayIndex = nameOfMenu.substring(9, 11).toInt();  // i know, i know
  String relayTimepointToEdit = nameOfMenu.substring(12);
  if(relayTimepointToEdit == "startPoint"){
    return "Relay " + String(relayIndex+1) +
           " - Start:  T+" +String(triggers[relayIndex].startPoint / 1000000.0) + "s";
  } else {                              //  Start: T+0
    return "Relay " + String(relayIndex+1) + " - Duration: " +
           String((triggers[relayIndex].endPoint-triggers[relayIndex].startPoint) / 1000000.0) + "s" +
           " (T+"+String(triggers[relayIndex].endPoint / 1000000.0)+"s)";
  }
}

//action* calculateActions() {
void calculateActions() {
  // Reset actions struct array
  memset(actions, 0, maxActions);

  // set "actions" according to "triggers"
  Serial.println("Calculate actions..");
  // Serial.println("----------------\nbefore collections: ");

  //printTriggers();

  long actionTimestamps[maxActions];
  numOfActions = 0;

  // Collect all start- and endpoints
  for(int i = 0; i < numOfTriggers; i++) {
    if(triggers[i].active) {
      actionTimestamps[numOfActions] = triggers[i].startPoint;
      numOfActions++;
      actionTimestamps[numOfActions] = triggers[i].endPoint;
      numOfActions++;
    }
  }
  /* Serial.println("----------------\nnumOfTriggers: ");
  Serial.print(numOfTriggers);
  Serial.println("\n----------------\nnumOfActions: ");
  Serial.print(numOfActions);
  Serial.println("\n----------------\nUnsorted:"); */
  
  // proof
  // Print collected timestamps
  /* for(int i = 0; i < numOfActions; i++) {
    Serial.println(actionTimestamps[i]);
  }
  Serial.println("\n----------------\nSorted:"); */

  // sort "actionTimestamps" linear 0 > ...
  sortArray(actionTimestamps, numOfActions);

  // proof
  /* Serial.print("numOfActions: ");
  Serial.println(numOfActions);
  for(int i = 0; i < numOfActions; i++) {
    Serial.println(actionTimestamps[i]);
  } */

  // Overwrite doublicated elements
  long lastValue = -1;
  int newNumOfActions = numOfActions;
  for(int i = 0; i < numOfActions; i++) {
    if(actionTimestamps[i] == lastValue) {
      lastValue = actionTimestamps[i];
      actionTimestamps[i] = 2147483647;  // move to back of array
      newNumOfActions -= 1;
    } else {
      lastValue = actionTimestamps[i];  // Yes must be here.
    }
  }

  // Resort to remove doublicates
  sortArray(actionTimestamps, numOfActions);  
  numOfActions = newNumOfActions;

  // proof
  /* Serial.println("-------");
  Serial.print("numOfActions: ");
  Serial.println(numOfActions);
  for(int i = 0; i < numOfActions; i++) {
    Serial.println(actionTimestamps[i]);
  }
  Serial.println("-------"); */


  // iterate over "actionTimestamps"
  long lastTimestamp = 0;
  for(int i = 0; i < numOfActions; i++) {
    actions[i].timestamp = actionTimestamps[i]-lastTimestamp;  // Relative time from action to action
    //actions[i].pins = {};
    /* Serial.println();
    Serial.print("Fill trigger #");
    Serial.print(i);
    Serial.print("\tactionTimestamps: ");
    Serial.print(actionTimestamps[i]); */
    /* Serial.print("\tI: ");
    Serial.print(i);
    Serial.print("\tnumOfTriggers: ");
    Serial.println(numOfTriggers); */

    /* Serial.print("lastTimestamp:\t");
    Serial.print(lastTimestamp); */
    /* Serial.print("\tDelay:\t");
    Serial.println(actionTimestamps[i]-lastTimestamp); */

    int countOfTriggerActions = 0;  // 1;
    for(int x = 0; x < numOfTriggers; x++) {
      // add startPoints and endPoints
      if(
        triggers[x].active &&
        (triggers[x].startPoint == actionTimestamps[i] || triggers[x].endPoint == actionTimestamps[i])
        ) {
        /* Serial.print("\tAction #");
        Serial.print(countOfTriggerActions);
        Serial.print(",\tX: ");
        Serial.print(x);
        Serial.print("\t.startPoint: ");
        Serial.print(triggers[x].startPoint);
        Serial.print("\t.endPoint: ");
        Serial.print(triggers[x].endPoint);
        Serial.print("\t.pin: ");
        Serial.print(triggers[x].pin);
        Serial.print("\tOn/Off: ");
        Serial.println(triggers[x].startPoint == actionTimestamps[i]); */

        /* 
        typedef struct action {
        long timestamp;
        int triggerAction;  // number of triggerSetup* pins...
        triggerSetup* pins;
        
        typedef struct triggerSetup {
        int pin;
        long startPoint;
        long endPoint;
        boolean active;
        }data;
        }; */

        actions[i].pins[countOfTriggerActions] = {
          triggers[x].pin,
          triggers[x].startPoint,  // this is not used in triggerActions()
          triggers[x].endPoint,  // this is not used in triggerActions()
          triggers[x].startPoint == actionTimestamps[i],  // Set "active" to true or false
        };
       
        countOfTriggerActions += 1;
      }
    }
    actions[i].triggerAction = countOfTriggerActions;
    lastTimestamp = actionTimestamps[i];
  }

  /* Serial.println();
  Serial.println("Calculation ended."); */
};


void printTriggers() {
  for(int i = 0; i < numOfTriggers; i++) {
    Serial.print("-------\nTrigger #");
    Serial.print(i);
    Serial.print("\t");
    Serial.print(triggers[i].pin);
    Serial.print("\t");
    Serial.print(triggers[i].startPoint);
    Serial.print("\t");
    Serial.print(triggers[i].endPoint);
    Serial.print("\t");
    Serial.println(triggers[i].active);
  }
}

void printActions() {
  Serial.println("Current Actions:");
  for(int i = 0; i < numOfActions; i++) {
    Serial.print("-------\nTrigger #");
    Serial.print(i);
    Serial.print("\tDelay: ");
    Serial.print(actions[i].timestamp);
    Serial.print("\ttriggerAction: ");
    Serial.println(actions[i].triggerAction);
    for(int x = 0; x < actions[i].triggerAction; x++) {
      Serial.print("  action #");
      Serial.print(x);
      Serial.print("\tpin: ");
      Serial.print(actions[i].pins[x].pin);
      /* Serial.print("\tstartPoint:");
      Serial.print(actions[i].pins[x].startPoint);
      Serial.print("\tendPoint:");
      Serial.print(actions[i].pins[x].endPoint); */
      Serial.print("\tOn/Off: ");
      Serial.println(actions[i].pins[x].active);
    }
  }
}

void printSettings() {
  Serial.println("---------");
  Serial.println("SETTINGS");
  Serial.println("---------");
  Serial.print("Initial delay:\t");
  Serial.print(initDelay);
  Serial.println("μs");
}

void triggerAction() {
  #ifdef debug
    Serial.println("Actions triggered");
  #endif

  digitalWrite(triggerLedPin, HIGH);

  // Wait at beginning
  if(initDelay > 0) {
    if(initDelay > 4000 && !hasShownTriggeronTft) {  // it takes ~3031ys to draw the red circle with white border
      delaySometime(initDelay - drawTriggerAction(true));
    } else {
      delaySometime(initDelay);
    }
  }

  for(int i = 0; i < numOfActions && triggerIsActive; i++) {
    #ifdef debug
      Serial.println("---------");
      Serial.print("Trigger #");
      Serial.print(i);
      Serial.println("..");
    #endif

    if(actions[i].timestamp > 4000 && !hasShownTriggeronTft) {  // it takes ~3031ys to draw the red circle with white border
      delaySometime(actions[i].timestamp - drawTriggerAction(true));
    } else {
      delaySometime(actions[i].timestamp);
    }



    // If trigger was not aborted in the meantime...
    if(triggerIsActive) {
      // Do all the action now
      executeAction(actions[i].triggerAction, actions[i].pins);
    } else {
      // Trigger btn was pressed again to abort.
      // reset() happens when while loop in main loop() is exited.
      Serial.println("Abort senquence initiated prematurely.");
    }
  }
  // End of for loop reached
  if(triggerIsActive) {
    Serial.println("End of all actions reached.");
    reset();
  }
};

void executeAction(int triggerActions, triggerSetup* pins) {
  #ifdef debug
    Serial.print("\tNo. of actions: ");
    Serial.println(triggerActions);
  #endif

  for(int i = 0; i < triggerActions; i++) {
    #ifdef debug
      Serial.print("\tPin: ");
      Serial.print(pins[i].pin);
      Serial.print(", startPoint: ");
      Serial.print(pins[i].startPoint);
      Serial.print(", endPoint: ");
      Serial.print(pins[i].endPoint);
      Serial.print(", active: ");
      Serial.println(pins[i].active);
    #endif

    // Set the relay according to specs:
    digitalWrite(pins[i].pin, pins[i].active);
  }
}

// Reset all relays to LOW
void reset() {
  triggerIsActive = false;
  // Set every pin to LOW
  Serial.println("reset.");
  for(int i = 0; i < numOfTriggers; i++) {
    digitalWrite(triggers[i].pin, LOW);
  }
    digitalWrite(triggerLedPin, LOW);
    drawTriggerAction(false);
};

// Save triggers and settings (initDelay) to spi-flash
void saveDataToMemory() {
  // https://learn.adafruit.com/adafruit-metro-m0-express-designed-for-circuitpython/using-spi-flash

  // Save "initDelay" to memory
  // EEPROM.writeLong(EEPROMaddressLong,initDelay);

  // Save "triggers" to memory
  /* for(int i = 0; i < numOfTriggers; i++) {
    EEPROM.writeInt(EEPROMaddressInt, triggers[i].pin);
    EEPROM.writeLong(EEPROMaddressLong, triggers[i].startPoint);
    EEPROM.writeLong(EEPROMaddressLong, triggers[i].endPoint);
    EEPROM.writeInt(EEPROMaddressInt, triggers[i].active);
  } */
}

// Get triggers and settings (initDelay) from spi-flash
void getDataFromMemory() {
  // https://learn.adafruit.com/adafruit-metro-m0-express-designed-for-circuitpython/using-spi-flash

  // Get "initDelay" from memory
  // initDelay = EEPROM.readLong(EEPROMaddressLong);

  // Get "triggers" from memory
  /* for(int i = 0; i < numOfTriggers; i++) {
    triggers[i].pin = EEPROM.readInt(EEPROMaddressInt);
    triggers[i].startPoint = EEPROM.readLong(EEPROMaddressLong);
    triggers[i].endPoint = EEPROM.readLong(EEPROMaddressLong);
    triggers[i].active = EEPROM.readInt(EEPROMaddressInt);
  } */
}

// Use delay() & delayMicroseconds() if microseconds is bigger
// than largest technically accurate value for delayMicroseconds()
void delaySometime(long delayTime) {
  if(delayTime > 16383) {
    // Serial.print("delay milliseconds: ");
    // Serial.println(delayTime/1000);
    delay(delayTime/1000);
    delayMicroseconds(delayTime - (delayTime/1000)*1000);  // delay rest of millis
    // eg. 123456-(123456/1000)*1000 => delayMicroseconds(456)
  } else {
    // Serial.print("delay microseconds: ");
    // Serial.println(delayTime);
    delayMicroseconds(delayTime);
  }
};

String floatToString(float number, String append) {
  int seconds = int(number);
  long fractals = (number - seconds) * 1000000000;
  return String(seconds) + "." + String(fractals) + append;
}

String floatMicrosToString(int timestamp, String append) {
  return String(timestamp) + "us";
  //return floatToString(timestamp/1000000.0, append);
}

/* String rightPad(String text, int length, String padd) {
  int currentLength = text.length();
  int isPAddingUseful = currentlength < length;
  if(isPAddingUseful) {
    return String(length - length, padd) + text;
  } else {
    return text;
  }
} */


// TFT
// GUI Elements & helpers
void centerText(const String &text, int x, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, x, y, &x1, &y1, &w, &h); //calc width of new string
  tft.setCursor(x - w / 2, y);
  tft.print(text);
}

void drawButton(String text, int x, int y, int w, int h, boolean selected, uint16_t selectedColor=HX8357_BLUE) {
  if(selected && editSettings) {
    tft.fillRoundRect(x, y, w, h, 5, selectedColor);
    tft.drawRoundRect(x, y, w, h, 5, HX8357_WHITE);
  } else {
    tft.fillRoundRect(x, y, w, h, 5, 0x4A69);
    tft.drawRoundRect(x, y, w, h, 5, 0x94B2);
  }
  tft.setTextColor(HX8357_WHITE);
  tft.setTextSize(2);
  tft.setCursor(x+10, y+4);
  tft.println(text);
}

// SPLASHSCREEN
void splashScreen() {
  tft.fillScreen(HX8357_WHITE);
  tft.setCursor(100, 1250);
  tft.setTextColor(HX8357_BLUE);
  tft.setTextSize(4);
  centerText("TRIGGERBOX", screenWidth/2, screenHeight/2-25);
  tft.setCursor(200, screenHeight/2+25);
  tft.setTextSize(2);
  tft.setTextColor(0xC638);  // light grey
  centerText("by filmkulissen.ch", screenWidth/2, screenHeight/2+25);
}

// Draw complete screen
void drawLayout() {
  // Clear the whole screen
  tft.fillScreen(HX8357_BLACK);

  // Draw elements
  drawNavbar();
  drawSettings();
  drawTriggerHeader();
  drawAllTriggers();
  drawFooter();

  if(safetyIsOn) {
    drawPopUp("Safety switch is on");
    tft.fillCircle(screenWidth/2, screenHeight/2+15, 36, HX8357_RED);
    tft.fillCircle(screenWidth/2, screenHeight/2+15, 30, HX8357_WHITE);
    tft.fillTriangle( screenWidth/2-27, screenHeight/2+38, screenWidth/2+23, screenHeight/2-12, screenWidth/2+25, screenHeight/2-9, HX8357_RED);
    tft.fillTriangle( screenWidth/2-27, screenHeight/2+38, screenWidth/2-25, screenHeight/2+41, screenWidth/2+25, screenHeight/2-9, HX8357_RED);
    drawLock();
  }
}

void drawNavbar() {
  // OVERWRITE EXISTING
  tft.fillRect(0, 0, screenWidth, 50, 0x4A69);  // dark grey

  tft.setCursor(10, 7);
  tft.setTextColor(HX8357_WHITE);
  tft.setTextSize(5);
  tft.println("TRIGGERBOX");

  if(editSettings) drawLock();
}

void drawLock() {
  // the thing on top
  tft.fillCircle(screenWidth-25, 20, 6, HX8357_YELLOW);
  tft.fillCircle(screenWidth-25, 20, 4, 0x4A69);
  // lock body
  tft.fillRoundRect(screenWidth-34, 23, 18, 14, 1, HX8357_YELLOW);
  // lock hole
  tft.fillCircle(screenWidth-25, 27, 3, 0x4A69);
  tft.fillRect(screenWidth-26, 30, 3, 3, 0x4A69);
}

int drawTriggerAction(boolean show) {
  long startTrigger = micros();
  if(show) {
    tft.fillCircle(screenWidth-25, 25, 8, HX8357_RED);
    tft.drawCircle(screenWidth-25, 25, 9, HX8357_WHITE);
  } else {
    tft.fillRect(screenWidth-34, 14, 22, 22, 0x4A69);  // dark grey
  }
  hasShownTriggeronTft = show;
  return(micros() - startTrigger);
}

void drawSettings() {
  // OVERWRITE EXISTING
  tft.fillRect(0, 55, screenWidth, 25, HX8357_BLACK);
  tft.setCursor(0, 60);
  tft.setTextColor(0x94B2);  // grey
  tft.setTextSize(2);
  tft.print("Initial delay: ");
  drawInitDelayButton();
}

void drawInitDelayButton() {
  drawButton(floatMicrosToString(initDelay, "s"), 180, 55, 200, 23, menuIds[menuSelector] == "initDelay");
}

void drawTriggerHeader() {
  tft.setCursor(0, marginTopTriggerlist);
  tft.setTextSize(2);
  tft.setTextColor(0x94B2);  // grey
  tft.println("#  start           duration     active");
}

void drawAllTriggers() {
  // OVERWRITE EXISTING
  tft.fillRect(0, marginTopTriggerlist+22, screenWidth, 143, HX8357_BLACK);
  tft.setCursor(0, marginTopTriggerlist+22);
  tft.setTextSize(3);
  tft.setTextColor(HX8357_WHITE);
  for(int i = 0; i < numOfTriggers; i++) {
    drawTrigger(i);
  }
}

void drawTrigger(int i) {
  // fill row
  tft.fillRect(0, marginTopTriggerlist+22+i*30, screenWidth, 24, HX8357_BLACK);
  tft.setCursor(0, marginTopTriggerlist+22+i*30);
  tft.setTextSize(3);
  boolean relayIsActive = triggers[i].active;
  if(relayIsActive) {
    tft.setTextColor(HX8357_WHITE);
  } else {
    tft.setTextColor(0x94B2);
  }
  tft.print(i+1);
  tft.setTextColor(HX8357_WHITE, HX8357_BLACK); tft.print(" ");

  /* tft.print(menuSelector);
  tft.print(": ");
  tft.print(highlightSelected);
  tft.print(": ");
  tft.print(highlightSelected == menuSelector);
  tft.print(": "); */

  // STARTPOINT
  highlightTextIfSelected(menuSelector == 1+i*3, relayIsActive);  // +1 because relay edit starts at index 1 of menuIds
  tft.print(floatMicrosToString(triggers[i].startPoint, "s"));
  tft.setTextColor(HX8357_WHITE, HX8357_BLACK); tft.print(" ");

  // DURATION
  highlightTextIfSelected(menuSelector == 2+i*3, relayIsActive);  // +1 because relay edit starts at index 1 of menuIds
  tft.print(floatMicrosToString(triggers[i].endPoint - triggers[i].startPoint, "s"));
  //tft.print("μs");
  tft.setTextColor(HX8357_WHITE, HX8357_BLACK); tft.print(" ");
  
  // ACTIVE
  highlightTextIfSelected(menuSelector == 3+i*3, relayIsActive);  // +1 because relay edit starts at index 1 of menuIds
  tft.print(triggers[i].active ? "Y" : "N");
}

void highlightTextIfSelected(boolean selected, boolean relayIsActive) {
  if(selected && editSettings) {
    tft.setTextColor(HX8357_BLUE, HX8357_WHITE);
  } else if(relayIsActive) {
    tft.setTextColor(HX8357_WHITE, HX8357_BLACK);
  } else {
    tft.setTextColor(0x94B2, HX8357_BLACK);
  }
}

void drawPopUp(String title) {
  tft.fillRoundRect(25, 75, screenWidth-50, screenHeight-150, 5, HX8357_WHITE);
  tft.drawRoundRect(24, 73, screenWidth-48, screenHeight-147, 5, HX8357_BLACK);
  tft.setTextColor(0x94B2);  // grey
  tft.setTextSize(2);
  centerText(title, screenWidth/2, screenHeight/2-50);
}

void drawPopUpContent(String content) {
    tft.fillRect(30, screenHeight/2-5, screenWidth-60, 30, HX8357_WHITE);
    tft.setTextColor(HX8357_BLACK);
    tft.setTextSize(3);
    centerText(content, screenWidth/2, screenHeight/2);
}

void drawFooter() {
  // OVERWRITE EXISTING
  tft.fillRect(0, screenHeight-25, screenWidth, 25, 0x4A69);  // dark grey
  // make footer with btns etc.
  tft.setCursor(10, screenHeight-20);
  tft.setTextSize(2);
  tft.setTextColor(0x94B2);  // grey
  tft.println("by FL");
  drawFooterButtons();
}

void drawFooterButtons() {
  if(!editSettings) {
    tft.setCursor(170, screenHeight-20);
    tft.println("Press knob for adjustment");
  } else {
    drawButton("Reset", screenWidth-180, screenHeight-24, 80, 23, menuIds[menuSelector] == "reset", HX8357_RED);
    drawButton("Save", screenWidth-90, screenHeight-24, 80, 23, menuIds[menuSelector] == "exit");
  }
}