/*
  i2c_rgb_led_driver

  Drives a Rgb LED strip with MOSFETS, taking comands
  from a I2C master

  msg spec
  [INDEX]: [FUNCTION]
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

// Pin mapings
#define PIN_R 5
#define PIN_G 6
#define PIN_B 3

#define SIMPLE_MSGS true
#define MAX_RBOW_FREQ 1
#define MAX_STROBE_FREQ 10

void readMsg (uint8_t* msg, int lenMsg);
void doStrobe (float freq);
void doRainbow (float freq);
void setColourHue (int angle);
uint8_t transformHue (signed int theta);
void setColourRgb (uint8_t r, uint8_t g, uint8_t b);
void render ();

static uint8_t analogVals[NUM_DIGITAL_PINS];
static float rBowFreq = 0;
static float strobeFreq = 0;

void init () {
  for (int i = 0; i < NUM_DIGITAL_PINS - 1; i++) {
    analogVals[i] = 0;
  }
  render();
}

void update () {
  if (rBowFreq > 0) {
    doRainbow(rBowFreq);
  }
  
  if (strobeFreq > 0) {
    doStrobe(strobeFreq);
  } else {
    render();
  }
}

void readMsg (uint8_t* msg, int lenMsg) {
  rBowFreq = 0; strobeFreq = 0;

  // Proccess input
  if (!SIMPLE_MSGS && (lenMsg >= 5) && (msg[4] != 0)) {
    rBowFreq = map(msg[4], 1, 255, 0, (MAX_RBOW_FREQ*1000)/1000.0;
  } else if (!SIMPLE_MSGS && (lenMsg >= 4) && (msg[3] != 0)) {
    setColourHue(map(msg[3], 1, 255, 0, 255)); //hehe, so effective!
  } else if (lenMsg >= 3) {
    setColourRgb(msg[0], msg[1], msg[2]);
  }
  
  // Deal with strobe case
  if ((!SIMPLE_MSGS && lenMsg >= 6) && (msg[5] != 0)) {
    strobeFreq = map(msg[5], 1, 255, 0, (MAX_STROBE_FREQ*1000)/1000.0;
  }
}

void doStrobe (float freq) {
  static unsigned long timePrev = 0;
  static bool strobeOn = true;

  //check time
  unsigned long timeCurr = millis();
  unsigned int strobeDelay = 1000/(2*freq);
  if (abs(timeCurr - timePrev) < strobeDelay) return;
  
  //set strobe
  if (strobeOn) {
    uint8_t tempR = analogVals[PIN_R];
    uint8_t tempG = analogVals[PIN_G];
    uint8_t tempB = analogVals[PIN_B];

    setColourRgb(0, 0, 0);
    render();
    setColourRgb(tempR, tempG, tempB);

    strobeOn = false;
  } else {
    render();
    strobeOn = true;
  }
  
  //set time
  timePrev = timeCurr;
}

void doRainbow (float freq) {
  static unsigned long timePrev = 0;
  static int i = 0;

  //check time
  unsigned long timeCurr = millis();
  unsigned int rBowDelay = 1000/(255*freq);
  if (abs(timeCurr - timePrev) < rBowDelay) return;
  
  //set colour
  if (i >= uint8_t) i = 0;
  setColourHue(i);
  i += min(1, abs(timeCurr - timePrev)/rBowDelay);
  
  //update time
  timePrev +=rBowDelay;
}

void setColourHue (uint8_t angle) {
  /*float hue = (1.0*angle)/255; 
  uint8_t r = 255 * (sin(2*PI*(hue + 0/3.0))+1)/2;
  uint8_t g = 255 * (sin(2*PI*(hue + 1/3.0))+1)/2;
  uint8_t b = 255 * (sin(2*PI*(hue + 2/3.0))+1)/2;*/

  signed int hue = map(angle, 0, 255, 0, 360);
  uint8_t r = transformHue(hue + 0);
  uint8_t g = transformHue(hue + 120);
  uint8_t b = transformHue(hue + 240);
  setColourRgb(r, g, b);
}

uint8_t transformHue (signed int theta) {
  //Bind theta to 0 <= theta < 360
  while (theta < 0) theta += 360;
  if (theta > 360) theta %= 360;

  //Funny pseudo square trinagle transformation
  if (theta > 240) return 0;
  else if (theta > 180) return 255-(255*(theta-120))/60;
  else if (theta > 60) return 255
  else return (255*theta)/60;
}

void setColourRgb (uint8_t r, uint8_t g, uint8_t b) {
  analogVals[PIN_R] = r;
  analogVals[PIN_G] = g;
  analogVals[PIN_B] = b;
}

void render () {
  analogWrite(PIN_R, analogVals[PIN_R]);
  analogWrite(PIN_G, analogVals[PIN_G]);
  analogWrite(PIN_B, analogVals[PIN_B]);
}

//Hack from http://forum.arduino.cc/index.php?topic=46546#msg336451
long myMap (long x, long in_min, long in_max, long out_min, long out_max) {
  long y = (x - in_min) * (out_max - out_min + 1) / (in_max - in_min + 1) + out_min;
  return constrain(y, out_min, out_max);
}
