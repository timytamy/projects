/*
  pwm_tester

  Tests continuity and isolation of between pins to verify
  PWM cables.
  
  Created 20140202
  By Tim
  
  https://github.com/timytamy/temp-projects

*/

#define SIG_A 7
#define PWR_A 6
#define GND_A 5
#define SIG_B 8
#define PWR_B 9
#define GND_B 10
#define NUM_PINS 6
#define PIN_LED 13
#define INTERVAL_GOOD 1000
#define INTERVAL_BAD 100

int pinArray[] = {SIG_A, PWR_A, GND_A, SIG_B, PWR_B, GND_B};
//int faultFlag = false;

void setup() {
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  int faultFlag = false;
  if (testPin(SIG_A, SIG_B)) {
    faultFlag = true;
  }
  if (testPin(PWR_A, PWR_B)) {
    faultFlag = true;
  }
  if (testPin(GND_A, GND_B)) {
    faultFlag = true;
  }
  blinkFault(faultFlag);
}

void setPinIO(int testPin) {
  for (int i = 0; i < NUM_PINS; i++) {
    pinMode(pinArray[i], INPUT_PULLUP);
  }
  pinMode(testPin, OUTPUT);
}

int checkPinIsolation(int pinOut, int pinIn){
  for (int i = 0; i < NUM_PINS; i++) {
    if ((pinArray[i] != pinOut) && (pinArray[i] != pinIn)) {
      if (digitalRead(pinArray[i]) == LOW) {
        return(true);
      }
    }
  }
  return(false);
}

int testPin(int pinOut, int pinIn){
 int faultFlag;
 setPinIO(pinOut);
 digitalWrite(pinOut, LOW);
 faultFlag = checkPinIsolation(pinOut, pinIn);
 if (digitalRead(pinIn) != LOW) {
   faultFlag = true;
 }
 return faultFlag;
}

void blinkFault(int faultFlag) {
  int interval = INTERVAL_GOOD;
  if(faultFlag) {
    interval = INTERVAL_BAD;
  }
  digitalWrite(PIN_LED, HIGH);
  delay(interval);
  digitalWrite(PIN_LED, LOW);
  delay(interval);
}
