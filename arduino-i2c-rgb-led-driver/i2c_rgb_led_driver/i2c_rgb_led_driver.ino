/*
  i2c_rgb_led_driver

  Drives a Rgb LED strip with MOSFETS, taking comands
  from a I2C master
  
  i2c packet spec:
  0: red value
  1: green value
  2: blue value
  3: hue macro value
  4: rainbow macro frequency value
  5: strobe frequency
  
  any value 3 > 0 overides values of 0, 1 and 2
  similarly any value 4 > 0 overides values 0-3
  
  any value of 5 > 0 produces a strobing effect
  
  Created 20131224
  By Tim
  
  https://github.com/timytamy/projects

*/

#include <stdbool.h>
#include <Arduino.h>
#include <Wire.h>

// Pin m
#define PIN_R 5
#define PIN_G 6
#define PIN_B 3
#define I2C_ADDR (B10110001)

#define RBOW_STEPS (1000)

#define SIMPLE_MSGS true

uint8_t analogWriteVals[NUM_DIGITAL_PINS];
float rBowFreq = 0;
float strobeFreq = 0;

void setup () {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  Wire.begin(I2C_ADDR);
  Wire.onReceive(handlerRecieve);
  Wire.onRequest(handlerRequest);
  
  setColourRgb(0, 0, 0);
  rBowFreq = 0.25;
}

void loop () {
  if (rBowFreq > 0) {
    doRainbow(rBowFreq);
  }
  
  if (strobeFreq > 0) {
    doStrobe(strobeFreq, false);
  } else {
    doStrobe(strobeFreq, true);
  }
}

void handlerRecieve (int numBytesIn) {
  
  // Turn on led on i2c recieve
  digitalWrite(LED_BUILTIN, HIGH);

  // Parse input into bytesIn array
  uint8_t bytesIn[numBytesIn];
  for (int i = 0; i < numBytesIn; i++) {
    bytesIn[i] = Wire.read();
  }
  
  doColour(bytesIn, numBytesIn);
  
  digitalWrite(LED_BUILTIN, LOW);
}

void handlerRequest () {
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BUILTIN, LOW);
}

void doColour (uint8_t* msg, int lenMsg) {
  
  // Proccess input
  if ((lenMsg >= 5) && (msg[4] != 0) && !SIMPLE_MSGS) {
    // Frequency ranges between 0 and 2 Hz
    rBowFreq = map(msg[4], 1, 255, 0, 2*1000)/1000.0;
  } else if (lenMsg >= 4 && !SIMPLE_MSGS) {
    rBowFreq = 0;
    setColourHue(msg[3], 255);
  } else if (lenMsg >= 3) {
    rBowFreq = 0;
    setColourRgb(msg[0], msg[1], msg[2]);
  }
  
  // Deal with strobe case
  if ((lenMsg >= 6) && (msg[5] != 0) && !SIMPLE_MSGS) {
    // Frequency ranges between 0 and 10 Hz
    strobeFreq = map(msg[5], 1, 255, 0, 10*1000)/1000.0;
  } else {
    strobeFreq = 0; 
  }
  
}

void doStrobe (float freq, bool reset) {
  static unsigned long timePrev = millis();
  static bool strobeOn = true;
  static uint8_t strobeVals[] = {0, 0, 0};

  if (reset) {
    strobeOn = true;
    return;
  }

  unsigned long timeCurr = millis();
  unsigned int strobeDelay = (1000/freq)/2;
  if (abs(timeCurr - timePrev) < strobeDelay) return;
  
  if (strobeOn) {
    strobeVals[0] = readColour(PIN_R);
    strobeVals[1] = readColour(PIN_G);
    strobeVals[2] = readColour(PIN_B);
    setColourRgb[0, 0, 0];
    strobeOn = false;
  } else {
    setColourRgb(strobeVals[0], strobeVals[1], strobeVals[2]);
    strobeOn = true;
  }
  
  timePrev = timeCurr;
}

void doRainbow (float freq) {
  static int i = 0;
  static unsigned long timePrev = millis();
  
  if (false) {
    i = 0;
    return;
  }

  unsigned long timeCurr = millis();
  unsigned int rBowDelay = (1000/freq)/RBOW_STEPS;
  if (abs(timeCurr - timePrev) < rBowDelay) return;
  
  if (i >= RBOW_STEPS) i = 0;
  setColourHue(i, RBOW_STEPS-1);
  i++;
  
  timePrev = timeCurr;
}

void setColourHue (int angle, int total) {
  float hue = (1.0*angle)/total;
  uint8_t r = 255 * (sin(2*PI*(hue + 0/3.0))+1)/2;
  uint8_t g = 255 * (sin(2*PI*(hue + 1/3.0))+1)/2;
  uint8_t b = 255 * (sin(2*PI*(hue + 2/3.0))+1)/2;
  setColourRgb(r, g, b);
}

void setColourRgb (uint8_t r, uint8_t g, uint8_t b) {
  myAnalogWrite(PIN_R, r);
  myAnalogWrite(PIN_G, g);
  myAnalogWrite(PIN_B, b);
}

uint8_t readColour (uint8_t pin) {
  return analogWriteVals[pin];
}

void myAnalogWrite (uint8_t pin, int val) {
  if (val < 0) val = 0;
  if (val > 255) val = 255;
  analogWriteVals[pin] = val;
  analogWrite(pin, val);
}
