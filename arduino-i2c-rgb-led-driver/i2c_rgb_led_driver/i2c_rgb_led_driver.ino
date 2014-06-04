/*
  i2c_rgb_led_driver

  Drives a RGB LED strip with MOSFETS, taking comands
  from a I2C master
  
  Created 20131224
  By Tim
  
  https://github.com/timytamy/temp-projects

*/

#include <Wire.h>

#define PIN_RED 9
#define PIN_GREEN 3
#define PIN_BLUE 10
#define I2C_ADDRESS (0b10110001)
#define PIN_LED 13

void setup() {
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(handlerRecieve);
  //Wire.onRequest(handlerRequest);
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
}

void setColour(int pin, uint8_t value) {
  analogWrite(pin, value);
}

void setColourRGB(uint8_t red, uint8_t green, uint8_t blue) {
  setColour(PIN_RED, red);
  setColour(PIN_GREEN, green);
  setColour(PIN_BLUE, blue);
}

void handlerRecieve(int numBytes) {
  digitalWrite(PIN_LED, HIGH); //D13 LED is flashed on when data is being recieved
  //Parse input into bytesIn array
  uint8_t bytesIn[numBytes];
  for (int i = 0; i < numBytes; i++) {
    bytesIn[i] = Wire.read();
  }
  //Set Colours
  if (numBytes >= 3){
    setColourRGB(bytesIn[0], bytesIn[1], bytesIn[2]);
  }
  digitalWrite(PIN_LED, LOW); //Turning D13 LED off again
}

//void handlerRequest() {} //Not implemented and not needed


