/*
  nicta_board_slave
  
  Turns the NICA EDU1 board into a serial slave
  to utilse the sensors/IO
  
  Created 20131231
  By Tim
  
  https://github.com/timytamy/temp-projects

*/

#include <Wire.h>
#include <LiquidCrystal.h>

#define POT     (3)
#define LIGHT   (6)
#define TEMP    (7)
#define PIEZO   (9)
#define LATCH  (11)
#define DATA   (12)
#define CLOCK  (10)
#define BLEFT  (14)
#define BRIGHT (15)

void special ();
void handlerI2C(uint8_t bytesIn[]);
void handlerLCD(uint8_t bytesIn[]);
void handlerPeripherals(uint8_t bytesIn[]);

LiquidCrystal lcd(6, 7, 8, 2, 3, 4, 5);

void setup () {
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(16, 2);
  pinMode(PIEZO, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(BLEFT, INPUT);
  pinMode(BRIGHT, INPUT);
  pinMode(13, OUTPUT);
}

void loop(){
  
}

void SerialEvent () {
  int bytesInLength = Serial.read(); //first byte is always length
  uint8_t bytesIn[bytesInLength];
  for (int i = 0; i < bytesInLength; i++) {
    bytesIn[i] = Serial.read();
  }
  if (bytesIn[0] == 'I') {         // 'I' for I2C
    handlerI2C(&bytesIn[0]);
  } else if (bytesIn[0] == 'L') {  // 'L' for LCD
    handlerLCD(&bytesIn[0]);
  } else if (bytesIn[0] == 'P') {  // 'P' for Peripherals (Pot, Light, Temp, Buttons, Piezo, LEDs)
    hanlderPeripherals(&bytesIn[0]);
  } 
}

void handlerI2C (uint8_t bytesIn[]) {
  //read from slave
  if (bytesIn[4] & 0b00000001) {
    int numBytes = Wire.requestFrom((bytesIn[4] >> 1), int(bytesIn[5]), true);
    uint8_t bytesOut[numBytes + 4];
    for (int i = 4; i < (numBytes + 4); i++) {
      bytesOut[i] = Wire.read();
    }
    //something to send stuff back
  //write to a slave
  } else if (bytesIn[4] && 0b00000000) {
    Wire.beginTransmission(bytesIn[4] >> 1);
    for (int i = 6; i < bytesIn[5]; i++){
      Wire.write(bytesIn[i]);
    }
    Wire.endTransmission();
  }
}

void handlerLCD (uint8_t bytesIn[]) {
  lcd.setCursor(0,0);
  for (int i = 6; i < bytesIn[5]; i++) {
    lcd.write(bytesIn[i]);
  }
}

void hanlderLED (uint8_t bytesIn[]) {

}

void hanlderPeripherals (uint8_t bytesIn[]) {
  //LED Shift Register
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, bytesIn[5]);
  digitalWrite(LATCH, HIGH);
  //Sensors to output
  uint8_t bytesOut[12];
  bytesOut[6] = analogRead(POT)/4;
  bytesOut[7] = analogRead(LIGHT)/4;
  bytesOut[8] = analogRead(TEMP)/4;
  bytesOut[9] = digitalRead(BLEFT);
  bytesOut[10] = digitalRead(BRIGHT);
  bytesOut[11] = bytesIn[9];
  bytesOut[12] = bytesIn[10];
  //Something to return bytesOut
  Serial.write(sizeof(bytesOut));
  for (int i = 0; i < sizeof(bytesOut); i++) {
    Serial.write(bytesOut[i]);
  }
}
/*
 Header
  [0]  interface (I2C, LCD, {Peripherals)
  [1]  length of body
  [2]  comand/error code
  [3]  reserved

 Body (starting at [4]
  I2C:
  [4] address + read/write state (address can be found with byteIn[4] >> 1)
  [5] length to be returned for read, or length of write
  [6] start of messege for write
  
  LCD:
  [4] reserved
  [5] length of messege
  [6] start of text
  
  Peripherals
  [4] reserved
  [5] reserved
  [6] Pot
  [7] Light
  [8] Temp
  [9] ButtonLeft
  [10] ButtonRight
  [11] Piezo
  [12] LED array
*/
