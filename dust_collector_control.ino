#include <RCSwitch.h> // https://github.com/sui77/rc-switch
#include "EmonLib.h"

EnergyMonitor emon1;
EnergyMonitor emon2;
EnergyMonitor emon3;
EnergyMonitor emon4;
EnergyMonitor emon5;
RCSwitch mySwitch = RCSwitch();

double Irms1;
double Irms2;
double Irms3;
double Irms4;
double Irms5;

int data = 0; // from RC receiver
int switchState = 0; // RC switch on/off
int powerState = 0; // actual power state
int initialized = 0; // wait to start the loop until a value of 0.00 is found on the current sensors
unsigned long lastDebounceTime = 0;

// Settings variables
int currentSampleRate = 300; // Sample rate to measure current from sensors
int powerOffDelay = 5000; // Delay before switching off after current is cut
int powerOnSmoothingDelay = 1000; // Delay after turning on so false negatives are weeded out
int relayCtrlPin = 7; // control pin for the beef cake relay
unsigned long debounceDelay = 1000;
float currentOffset = 0.02;
boolean isCurrent = false;

void setup()
{
  Serial.begin(9600);
  mySwitch.enableReceive(0);
  emon1.current(A0, 111.1);             // Current: input pin, calibration.
  emon2.current(A1, 111.1);
  emon3.current(A2, 111.1);
  emon4.current(A3, 111.1);
  emon5.current(A4, 111.1);

  pinMode(relayCtrlPin, OUTPUT);
}

void loop()
{
  Irms1 = emon1.calcIrms(currentSampleRate);
  Irms2 = emon1.calcIrms(currentSampleRate);
  Irms3 = emon1.calcIrms(currentSampleRate);
  Irms4 = emon1.calcIrms(currentSampleRate);
  Irms5 = emon1.calcIrms(currentSampleRate);
  isCurrent = Irms1 > currentOffset || Irms2 > currentOffset || Irms3 > currentOffset || Irms4 > currentOffset || Irms5 > currentOffset;


  if(Irms1 < 0.001 && !initialized) {
    initialized = 1;
    Serial.println("initialized!");
  }
  Serial.println(Irms1);
  if(initialized == 1) {
    Serial.print(Irms1*230.0);           // Apparent power
    Serial.print(" ");

    if(switchState == 0) {
      if(isCurrent  && powerState == 0) {
        digitalWrite(relayCtrlPin, HIGH);
        powerState = 1;
        delay(powerOnSmoothingDelay);
      }
      if(!isCurrent && powerState == 1) {
        delay(powerOffDelay);
        digitalWrite(relayCtrlPin, LOW);
        powerState = 0;
      }
    }

  if (mySwitch.available()) {
      int value = mySwitch.getReceivedValue();

      if (value == 0) {
        Serial.print("Unknown encoding");
      } else {
        if((millis() - lastDebounceTime) > debounceDelay) {
          lastDebounceTime = millis();
          if(switchState == 0) {
            digitalWrite(relayCtrlPin, HIGH);
            switchState = 1;
            powerState = 1;
          } else {
            digitalWrite(relayCtrlPin, LOW);
            switchState = 0;
            powerState = 0;
            initialized = 0;
          }
        }

        Serial.print("Received ");
        Serial.print( mySwitch.getReceivedValue() );
        Serial.print(" / ");
        Serial.print( mySwitch.getReceivedBitlength() );
        Serial.print("bit ");
        Serial.print("Protocol: ");
        Serial.println( mySwitch.getReceivedProtocol() );
      }

      mySwitch.resetAvailable();
    }
  }
}
