/*
  i2c_rgb_led_driver

  Drives a Rgb LED strip with MOSFETS, taking comands
  from a I2C master
  
  Created 20131224
  By Tim
  
  https://github.com/timytamy/projects

*/

#include <Wire.h>

#define PIN_LED 13
#define PIN_RED 5
#define PIN_GREEN 6
#define PIN_BLUE 3
#define I2C_ADDRESS (0b10110001)

#define LED_TEST false

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(handlerRecieve);
  Wire.onRequest(handlerRequest);

  // Start off with green
  setColourRgb(0, 255, 0);
}

void loop() {
  if (LED_TEST == true) {
    uint8_t rgbMix[3];
    for (int i = 0; i < 3; i++) rgbMix[i] = 0;
    for (int i = 0; i < 3; i++) {
      for (rgbMix[i] = 0; rgbMix[i] < 255; rgbMix[i]++) {
        setColourRgb(rgbMix[0], rgbMix[1], rgbMix[2]);
        delay(10);
      }
      rgbMix[i] = 0;
    }
  }
}

void setColour(int pin, uint8_t value) {
  analogWrite(pin, value);
}

void setColourRgb(uint8_t red, uint8_t green, uint8_t blue) {
  setColour(PIN_RED, red);
  setColour(PIN_GREEN, green);
  setColour(PIN_BLUE, blue);
}

void handlerRecieve(int numBytesIn) {
  // Turn on led on i2c recieve
  digitalWrite(PIN_LED, HIGH);

  // Parse input into bytesIn array
  uint8_t bytesIn[numBytesIn];
  for (int i = 0; i < numBytesIn; i++) {
    bytesIn[i] = Wire.read();
  }

  // Set Colours
  if (numBytesIn >= 3){
    setColourRgb(bytesIn[0], bytesIn[1], bytesIn[2]);
  }
  
  digitalWrite(PIN_LED, LOW);
}

void handlerRequest() {} //Not implemented and not needed


