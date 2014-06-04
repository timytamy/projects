/*
  serial_i2c_converter
  
  Converts serial bytes to i2c bytes
  
  Created 20131224
  By Tim
  
  https://github.com/timytamy/temp-projects

*/

#include <Wire.h>
#include <stdint.h>

#define I2C_ADDRESS_RGBLED (0b10110001)
#define DATAGRAM_SIZE 4

void setup(){
  Wire.begin();
  Serial.begin(9600);  
}

void loop(){
  //Check to see if there is enough data
  if (Serial.available() > DATAGRAM_SIZE){
    //Parse serial input into data array
    uint8_t data[DATAGRAM_SIZE];
    for (int i = 0; i < DATAGRAM_SIZE; i++){
      data[i] = Wire.read();
    }
    //Transmit I2C
    Wire.beginTransmission(I2C_ADDRESS_RGBLED);
    for (int i = 0; i < DATAGRAM_SIZE; i++) {
      Wire.write(data[i]);
    }
    Wire.endTransmission();
    Serial.write('ACK'); //ACK
  }
}
