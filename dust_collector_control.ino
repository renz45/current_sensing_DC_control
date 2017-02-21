// Uncomment this line to turn serial logging on
//#define consoleLog(x)  (Serial.println(x))
#define consoleLog(x)  (x)

#include <RCSwitch.h> // https://github.com/sui77/rc-switch
#include "EmonLib.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
// Contains EEPROM.read() and EEPROM.write()
#include <EEPROM.h>
// ID of the settings block
#define CONFIG_VERSION "DC1"
// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

// menu state values
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[4];
  // The variables of your settings
  int menuState[7];
} storage = {
  CONFIG_VERSION,
  // The default values
  {0,2,5,5,5,5,1}
};

EnergyMonitor emon1;
EnergyMonitor emon2;
EnergyMonitor emon3;
EnergyMonitor emon4;

RCSwitch mySwitch = RCSwitch();
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

double Irms1;
double Irms2;
double Irms3;
double Irms4;

int sensorTripped;
int data = 0; // from RC receiver
int rcSwitchState = 0; // RC switch on/off
int powerState = 0; // actual power state
int initialized = 0; // wait to start the loop until a value of 0.00 is found on the current sensors
boolean startMessageDisplayed = false;
unsigned long lastDebounceTime = 0;
unsigned long lastScreenSleep = 0;
boolean screenAwake = true;
int buttonPressed;
int currentState;
int buttonState;

// Option menu related
int currentMenuState = 0;
int displayedMenuState;

int defaultState = 0;
int manualPowerState = 1;
int delay1State = 2;
int delay2State = 3;
int delay3State = 4;
int delay4State = 5;
int rcState = 6;

int const POWER_OFF = 0;
int const POWER_ON = 1;
int const POWER_AUTO = 2;
int const ON = 1;
int const OFF = 0;

String stateNames[] = {
  "Status",
  "Power", 
  "Sens 1 Off Delay",
  "Sens 2 Off Delay",
  "Sens 3 Off Delay",
  "Sens 4 Off Delay",
  "RC Toggle"
};

// Settings variables
int currentSampleRate = 300; // Sample rate to measure current from sensors
int powerOffDelay = 5000; // Delay before switching off after current is cut
int powerOnSmoothingDelay = 1000; // Delay after turning on so false negatives are weeded out
int relayCtrlPin = 5; // control pin for the beef cake relay
unsigned long debounceDelay = 1000; 
float currentOffset = 0.02;
boolean isCurrent = false;
int screenSleepDelay = 5000;

// buttons
int selectBtn = 8;
int leftBtn = 10;
int rightBtn = 9;
int toggleBtn = 11;

#define arrayLength(x)  (sizeof( x ) / sizeof( *x ))

int btns[] = {selectBtn, leftBtn, rightBtn, toggleBtn};

void lcdPrint(String line1, String line2 = ""){
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
  lastScreenSleep = millis();
}

void setup()
{
  initConfig();
  initGeneral();
  initButtons();
  initCurrentSensors();

  if(storage.menuState[manualPowerState] == POWER_OFF) {
    lcdPrint("Manual power", "is off"); 
  }

  if(storage.menuState[manualPowerState] == POWER_ON) {
    lcdPrint("Manual power", "is on");
  }

  if(storage.menuState[manualPowerState] == POWER_AUTO) {
    lcdPrint("Auto current", "sensing is on");
    delay(1500);
  }
}


void initConfig(){
  loadConfig();
}

void initCurrentSensors() {
  emon1.current(A0, 111.1);             // Current: input pin, calibration.
  emon2.current(A1, 111.1); 
  emon3.current(A2, 111.1); 
  emon4.current(A3, 111.1);
}

void initGeneral(){
  Serial.begin(9600);
  lcd.init();                      // initialize the lcd 
  lcd.setCursor(1,0);
  mySwitch.enableReceive(0);
  pinMode(relayCtrlPin, OUTPUT);
}

void initButtons() {
  pinMode(selectBtn, INPUT_PULLUP);   // button as input
  pinMode(leftBtn, INPUT_PULLUP);
  pinMode(rightBtn, INPUT_PULLUP);
  pinMode(toggleBtn, INPUT_PULLUP);
}

void handleCurrentSensors() {
  Irms1 = emon1.calcIrms(currentSampleRate);
  Irms2 = emon1.calcIrms(currentSampleRate);
  Irms3 = emon1.calcIrms(currentSampleRate);
  Irms4 = emon1.calcIrms(currentSampleRate);

  if(Irms1 > currentOffset){
    isCurrent = true;
    sensorTripped = 0;
  } else if(Irms2 > currentOffset){
    isCurrent = true;
    sensorTripped = 1;
  } else if(Irms3 > currentOffset){
    isCurrent = true;
    sensorTripped = 2;
  } else if(Irms4 > currentOffset){
    isCurrent = true;
    sensorTripped = 3;
  } else {
    isCurrent = false;
  }


  if(Irms1 < 0.001 && !initialized) {
    initialized = 1;
    consoleLog("initialized!");
    lcdPrint("Current Sensors", "Ready!");
  }

  if(initialized == 0 && !startMessageDisplayed) {
    lcdPrint("Current Sensors", "Initializing...");
    startMessageDisplayed = true;
  }
  
  consoleLog(Irms1);

  if(initialized == 1 ) {
    consoleLog(Irms1*230.0);           // Apparent power
    consoleLog(" ");
  
    if(rcSwitchState == 0) {
      if(isCurrent  && powerState == OFF) {
        powerOn();
        powerState = ON;
        lcdPrint("Power ON via:", "current sensor " + String(sensorTripped + 1));
        delay(powerOnSmoothingDelay);
      }
      if(!isCurrent && powerState == ON) {
        int delayVal = storage.menuState[delay1State + sensorTripped] * 1000;
        lcdPrint("Power OFF",  "delay: " + String(delayVal / 1000) + " via:s" + String(sensorTripped + 1));
        // build the delay based on values from the config. Base the count off the first delay value
        delay(delayVal);
        powerOff();
        powerState = OFF;
      } 
    }
  }
}

void powerOn() {
  digitalWrite(relayCtrlPin, HIGH);
}

void powerOff() {
  digitalWrite(relayCtrlPin, LOW);
}

void handleRcReceiverSignal(){
  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      Serial.print("Unknown encoding");
    } else {
      if((millis() - lastDebounceTime) > debounceDelay && storage.menuState[manualPowerState] == POWER_AUTO && storage.menuState[rcState] == ON) {
        lastDebounceTime = millis();
        if(rcSwitchState == OFF) {
          powerOn();
          rcSwitchState = ON;
          powerState = ON;
          lcdPrint("Power ON via:", "RC Control");
        } else {
          powerOff();
          rcSwitchState = OFF;
          powerState = OFF;
          lcdPrint("Power OFF via:", "RC Control");
        }
      }
      consoleLog("Received " + String(mySwitch.getReceivedValue()) + " / " + mySwitch.getReceivedBitlength() + "bit Protocol: " + mySwitch.getReceivedProtocol());
    }

    if(storage.menuState[manualPowerState] != POWER_AUTO) {
      lcdPrint("Power: Auto is", "required for RC");
    } else if(storage.menuState[rcState] == OFF) {
      lcdPrint("RC Mode", "is off");
    }
    
    mySwitch.resetAvailable();
  }
}

String buttonNameForPin(int b) {
  if(b == selectBtn) {
    return "Select"; 
  }

  if(b == leftBtn) {
    return "Left";  
  }

  if(b == rightBtn) {
    return "Right"; 
  }

  if(b == toggleBtn) {
    return "Toggle";  
  }
}

void callButtonFunc(int button) {
  if(button == selectBtn) {
    if(!screenAwake) {
      lcd.backlight();
      screenAwake = true;  
    }
    callSelectBtn();
  } else if(button == leftBtn && screenAwake) {
    callLeftBtn();
  } else if(button == rightBtn && screenAwake) {
    callRightBtn();  
  } else if(button == toggleBtn && screenAwake) {
    callToggleBtn();
  }
}

// Save/load config from EEPROM so it doesn't reset when power is off
////////////////////////////////////////////////////////////////////
/* LoadAndSaveSettings
 * Joghurt 2010
 * Demonstrates how to load and save settings to the EEPROM
 */

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    for (unsigned int t=0; t<sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
}

/////////////////////////////////////////////////////////////////////

void callSelectBtn(){
  currentMenuState = currentMenuState + 1;

  if(currentMenuState == 7) {
    currentMenuState = 1;
  }
}

void toggleMenuPower(boolean reverse=false){
  if(reverse){
    storage.menuState[manualPowerState] = storage.menuState[manualPowerState] - 1;
  } else {
    storage.menuState[manualPowerState] = storage.menuState[manualPowerState] + 1;  
  }

  if(storage.menuState[manualPowerState] == 3) {
    storage.menuState[manualPowerState] = 0;
  } else if(storage.menuState[manualPowerState] == -1) {
    storage.menuState[manualPowerState] = 2;
  }

  // Reset the RC switch to off whenever changing any of the manual power settings
  if(rcSwitchState == ON) {
    rcSwitchState = OFF;
    powerState = OFF; 
  }

  if(storage.menuState[manualPowerState] == POWER_ON) {
    powerOn();
  } else if(storage.menuState[manualPowerState] == POWER_OFF) {
    powerOff();
  } else if(storage.menuState[manualPowerState] == POWER_AUTO) {
    if(powerState == ON) {
      powerOn();
    } else {
      powerOff();
    }
  }

  refreshMenu();
  saveConfig();
}

void toggleSensorDelay(boolean reverse=false) {
  if(reverse) {
    storage.menuState[currentMenuState] = storage.menuState[currentMenuState] - 1;
  } else {
    storage.menuState[currentMenuState] = storage.menuState[currentMenuState] + 1;
  }

  if(storage.menuState[currentMenuState] < 0) {
    storage.menuState[currentMenuState] = 0;  
  }

  refreshMenu();
  saveConfig();
}

void toggleRcValue(boolean reverse=false){
  if(storage.menuState[currentMenuState] == OFF) {
    storage.menuState[currentMenuState] = ON; 
  } else {
    storage.menuState[currentMenuState] = OFF;

    if(rcSwitchState == 1) {
      rcSwitchState = OFF;
      powerState = OFF;
      powerOff(); 
    }
  }
 
  refreshMenu();
  saveConfig();
}

void callLeftBtn(){
  // Manual power state
  if(currentMenuState == manualPowerState) {
    toggleMenuPower(true);

  // current sensors off delay
  } else if(currentMenuState == delay1State || currentMenuState == delay2State || currentMenuState == delay3State || currentMenuState == delay4State) {
    toggleSensorDelay(true);

  // RC power toggle
  } else if(currentMenuState == rcState){
    toggleRcValue(true);  
  }
}

void callRightBtn(){
  // Manual power state
  if(currentMenuState == manualPowerState) {
    toggleMenuPower();

  // current sensors off delay
  } else if(currentMenuState == delay1State || currentMenuState == delay2State || currentMenuState == delay3State || currentMenuState == delay4State) {
    toggleSensorDelay();

  // RC power toggle
  } else if(currentMenuState == rcState){
    toggleRcValue();
  }
}

void callToggleBtn(){
  Serial.println("TOGGLEING");
}

void refreshMenu() {
  // Manual power menu item
  if(currentMenuState == manualPowerState) {
    if(storage.menuState[currentMenuState] == POWER_AUTO) {
      lcdPrint(stateNames[currentMenuState], " on [auto] off ");
    } else if(storage.menuState[currentMenuState] == POWER_ON) {
      lcdPrint(stateNames[currentMenuState], "[on] auto  off "); 
    } else {
      lcdPrint(stateNames[currentMenuState], " on  auto [off]"); 
    }
  // Sensors off Delay
  } else if(currentMenuState == delay1State || currentMenuState == delay2State || currentMenuState == delay3State || currentMenuState == delay4State) {
    String output = String(storage.menuState[currentMenuState]);
    lcdPrint(stateNames[currentMenuState], "< " + output + " > " + pluralSeconds(storage.menuState[currentMenuState]));
  // RC receiver toggle
  } else if(currentMenuState == rcState) {
    if(storage.menuState[currentMenuState] == OFF) {
      lcdPrint(stateNames[currentMenuState], " on [off]");
    } else {
      lcdPrint(stateNames[currentMenuState], "[on] off "); 
    }
  }
  
}

String pluralSeconds(int val) {
  if(val == 1) {
    return "second"; 
  } else {
    return "seconds";  
  }
}

void displayMenuState() {
  if(displayedMenuState != currentMenuState){
    refreshMenu();
    
    displayedMenuState = currentMenuState;
  }
}

void handlePushButtons(){
  int button;
  int i;
  int l = arrayLength(btns);

  for(i=0; i < l; i=i+1){
    button = btns[i];
    if (digitalRead(button) == LOW && buttonPressed != button) {
      buttonPressed = button;
    } else if(digitalRead(button) == HIGH && buttonPressed == button){
      callButtonFunc(button);
      buttonPressed = -1;
    }
  }
}

void loop()
{ 
  // we cant add the rc receiver to this since it buffers signals, we want to disable it completely
  if(storage.menuState[manualPowerState] == POWER_AUTO) {
    handleCurrentSensors();
  }
  
  handleRcReceiverSignal();
  handlePushButtons();
  displayMenuState();

  if((millis() - lastScreenSleep) > screenSleepDelay) {
    lastScreenSleep = millis();
    screenAwake = false;
    lcd.noBacklight();
    lcd.clear();
    currentMenuState = 0;
  }
  
}

