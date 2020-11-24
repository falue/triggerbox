// LIBRARIES ----------------------------------------------------------------------------------
#include <MD_REncoder.h>
#include <ArduinoSort.h>

// PIN DECLARATION ----------------------------------------------------------------------------
const int triggerPin = 2;
const int triggerLedPin = 13;
MD_REncoder rotaryEncoder = MD_REncoder(0, 1);
const int rotaryHomePin = 3;

// VARS ---------------------------------------------------------------------------------------
boolean triggerIsActive = false;
long initDelay = 0;  // MICROSECONDS eg. 1000000
long lastTimeTriggered = 0;
long rotaryOldPosition = 0;
long rotaryCurrentDirection = 0;
int rotaryDirection = 0;
boolean editSettings = false;
int menuSelector = 0;

// DEFINE -------------------------------------------------------------------------------------
#define development  // Serial.print infos when triggering

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
  {4, 0, 1000000, false},
	{5, 1000000, 2000000, false},
	{6, 1600000, 2600000, false},
	{7, 2000000, 3000000, false},
	{8, 1000000, 2000000, false},
	{9, 7000000, 8000000, false},

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


// ACTIONS FOR SETUP --------------------------------------------------------------------------
void triggerIsPushed() {
  if(millis() > lastTimeTriggered + 500) {
    #ifdef development
      Serial.println("triggerIsPushed");
    #endif
    if(!editSettings) {
      triggerIsActive = !triggerIsActive;
      if(!triggerIsActive) reset();
    } else {
      triggerIsActive = false;
      Serial.println("Safety first! Cannot trigger when in menu.");
    }
  }
  lastTimeTriggered = millis();
}

// SETUP --------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500); // wait for serial monitor to catch up...
  Serial.println("~~:::=[ TRIGGERBOX ]=:::~~");

  pinMode(triggerPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(triggerPin), triggerIsPushed, RISING);
  pinMode(rotaryHomePin, INPUT);

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
};


// LOOP ---------------------------------------------------------------------------------------
void loop() {
  editSettings = digitalRead(rotaryHomePin);
  if(editSettings) {
    Serial.println("---------");
    Serial.println("~~:::=[ TRIGGERBOX MENU ]=:::~~");
    Serial.println("---------");
    Serial.println("Home");
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
  // TODO: Draw "EDIT MODE" to display, with save and home button

  // Menu system
  const int numOfMenuIds = 16;
  String menuIds[numOfMenuIds] = {
    F("main"),  // for what?
    F("initDelay"),
    F("setRelay-00-startPoint"), F("setRelay-00-endPoint"), F("setRelay-01-startPoint"), F("setRelay-01-endPoint"),
    F("setRelay-02-startPoint"), F("setRelay-02-endPoint"), F("setRelay-03-startPoint"), F("setRelay-03-endPoint"), 
    F("setRelay-04-startPoint"), F("setRelay-04-endPoint"), F("setRelay-05-startPoint"), F("setRelay-05-endPoint"),
    F("reset"),
    F("exit")
  };

  // Names of menu items to display
  String menuDisplay[numOfMenuIds] = {
    F("Home"),
    F("Set initial delay.."),
    F("setRelay-00-startPoint"), F("setRelay-00-endPoint"), F("setRelay-01-startPoint"), F("setRelay-01-endPoint"),
    F("setRelay-02-startPoint"), F("setRelay-02-endPoint"), F("setRelay-03-startPoint"), F("setRelay-03-endPoint"), 
    F("setRelay-04-startPoint"), F("setRelay-04-endPoint"), F("setRelay-05-startPoint"), F("setRelay-05-endPoint"),
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
    Serial.println(
      selectedMenuDisplay.startsWith("setRelay")
      ? getRelayMenuItem(selectedMenuDisplay)
      : selectedMenuDisplay
    );

    // TODO: Draw changes to display
  }

  // Edit initDelay
  if(digitalRead(rotaryHomePin) && selectedMenu == "initDelay") {
    Serial.print("Edit initDelay:\t");
    Serial.println(initDelay);
    delay(250);  // wait for button to be depressed
    while(!digitalRead(rotaryHomePin)) {
      long reading = readRotaryEncoder(true);
      if(reading != 0) {
        initDelay += reading;
        initDelay = initDelay >= 0 ? initDelay : 0;
        Serial.print(initDelay);
        Serial.println("μs ("+String(initDelay / 1000000.0)+"..s)");
      }
    }
    Serial.print("Saved initDelay:\t");
    Serial.print(initDelay);
    Serial.println("μs ("+String(initDelay / 1000000.0)+"..s)");
    delay(250);  // wait for btn release
  }

  // Edit start & end timestamps of relays
  if(digitalRead(rotaryHomePin) && selectedMenu.startsWith("setRelay")) {
    delay(250);  // wait for button to be depressed
    int relayIndex = selectedMenu.substring(9, 11).toInt();  // i know, i know
    String relayTimepointToEdit = selectedMenu.substring(12);

    if(relayTimepointToEdit == "startPoint") {
      Serial.println("Edit start point:");
      long initialGap = triggers[relayIndex].endPoint - triggers[relayIndex].startPoint;
      while(!digitalRead(rotaryHomePin)) {
        long reading = readRotaryEncoder(true);
        if(reading != 0) {
          triggers[relayIndex].startPoint += reading;
          triggers[relayIndex].startPoint = triggers[relayIndex].startPoint >= 0
            ? triggers[relayIndex].startPoint
            : 0;
          Serial.print(triggers[relayIndex].startPoint);
          Serial.println("μs ("+String(triggers[relayIndex].startPoint / 1000000.0)+"..s)");
        }
      }
      // re-set endpoint
      triggers[relayIndex].endPoint = triggers[relayIndex].startPoint + initialGap;
      Serial.print("Saved startPoint");
      Serial.print(triggers[relayIndex].startPoint);
      Serial.println("μs ("+String(triggers[relayIndex].startPoint / 1000000.0)+"..s)");
    } else {
      Serial.println("Edit end point:");
      while(!digitalRead(rotaryHomePin)) {
        long reading = readRotaryEncoder(true);
        if(reading != 0) {
          triggers[relayIndex].endPoint += reading;
          triggers[relayIndex].endPoint = triggers[relayIndex].endPoint > triggers[relayIndex].startPoint
            ? triggers[relayIndex].endPoint
            : triggers[relayIndex].startPoint+1;  // Always at least 1 micros later to turn off
          Serial.print(triggers[relayIndex].endPoint);
          Serial.println("μs ("+String(triggers[relayIndex].endPoint / 1000000.0)+"..s)");
        }
      }
      Serial.print("Saved endPoint ");
      Serial.print(triggers[relayIndex].endPoint);
      Serial.println("μs ("+String(triggers[relayIndex].endPoint / 1000000.0)+"..s)");
    }
    delay(250);  // wait for btn release
  }

  // Reset to default
  if(digitalRead(rotaryHomePin) && selectedMenu == "reset") {
    initDelay = 0;
    triggers[0] = {4, 0, 1000000, false};
    triggers[1] = {5, 1000000, 2000000, false};
    triggers[2] = {6, 1600000, 2600000, false};
    triggers[3] = {7, 2000000, 3000000, false};
    triggers[4] = {8, 1000000, 2000000, false};
    triggers[5] = {9, 7000000, 8000000, false};

    delay(250);  // wait for btn release
    editSettings = false;
  }

  // Exit menu
  if(digitalRead(rotaryHomePin) && (selectedMenu == "exit" || selectedMenu == "main")) {
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

String getRelayMenuItem(String nameOfMenu) {
  int index = nameOfMenu.substring(9, 11).toInt();  // i know, i know
  String relayTimepointToEdit = nameOfMenu.substring(12);
  if(relayTimepointToEdit == "startPoint"){
    return "Relay " + String(index+1) +
           " - Start:  T+" +String(triggers[index].startPoint / 1000000.0) + "s";
  } else {                              //  Start: T+0
    return "Relay " + String(index+1) + " - Duration: " +
           String((triggers[index].endPoint-triggers[index].startPoint) / 1000000.0) + "s" +
           " (T+"+String(triggers[index].endPoint / 1000000.0)+"s)";
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
    actionTimestamps[numOfActions] = triggers[i].startPoint;
    numOfActions++;
    actionTimestamps[numOfActions] = triggers[i].endPoint;
    numOfActions++;
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
      if(triggers[x].startPoint == actionTimestamps[i] || triggers[x].endPoint == actionTimestamps[i]) {
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
  #ifdef development
    Serial.println("Actions triggered");
  #endif

  digitalWrite(triggerLedPin, HIGH);

  // Wait at beginning
  if(initDelay > 0) delaySometime(initDelay);

  for(int i = 0; i < numOfActions && triggerIsActive; i++) {
    #ifdef development
      Serial.println("---------");
      Serial.print("Trigger #");
      Serial.print(i);
      Serial.println("..");
    #endif
    delaySometime(actions[i].timestamp);

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
  #ifdef development
    Serial.print("\tNo. of actions: ");
    Serial.println(triggerActions);
  #endif

  for(int i = 0; i < triggerActions; i++) {
    #ifdef development
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
