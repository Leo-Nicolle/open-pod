/*********************************************************
This is a library for the MPR121 12-channel Capacitive touch sensor

Designed specifically to work with the MPR121 Breakout in the Adafruit shop
  ----> https://www.adafruit.com/products/

These sensors use I2C communicate, at least 2 pins are required
to interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.
BSD license, all text above must be included in any redistribution
**********************************************************/

#include <Wire.h>
#include "Adafruit_MPR121.h"

#include "touch.h"
#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif


Touch::Touch(State* state){
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  this->state = state;
  if (!cap.begin(0x5A, &Wire,20 )) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

}
void Touch::debug(){
   currtouched = cap.touched();

  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i))
     && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" touched");
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" released");
    }
  }

  // reset our state
  lasttouched = currtouched;

  // // comment out this line for detailed data from the sensor!
  // return;

  // // debugging info, what
  Serial.print("\t\t\t\t\t\t\t\t\t\t\t\t\t 0x"); Serial.println(cap.touched(), HEX);
  Serial.print("Filt: ");
  for (uint8_t i=0; i<12; i++) {
    Serial.print(cap.filteredData(i)); Serial.print("\t");
  }
  Serial.println();
  Serial.print("Base: ");
  for (uint8_t i=0; i<12; i++) {
    Serial.print(cap.baselineData(i)); Serial.print("\t");
  }
  Serial.println();
}

void Touch::update(){
  const short top = 2;
  const short right = 1;
  const short bottom = 3;
  const short left = 4;
  int wheel[4];
  wheel[0]= top;
  wheel[1]= right;
  wheel[2]= bottom;
  wheel[3]= left;

  int filtered[4];
  int baseline[4];


  // read the values
  for (uint8_t i=0; i<4; i++) {
      filtered[i] = cap.filteredData(wheel[i]);
      baseline[i] = cap.filteredData(wheel[i]);
  }
  int maxFiltered = 0;
  for (uint8_t i=0; i<4; i++) {
    maxFiltered = max(maxFiltered, baseline[i]);
  }
  int thresholdFiltered = 30;
  int minFiltered = 95;
  int indexA = -1;
  int minValue = 0;

  int touched[4];

  for (uint8_t i=0; i<4; i++) {
    if(filtered[i] + thresholdFiltered >= maxFiltered) continue;
    touched[i] = true;
    int diff = maxFiltered - filtered[i];
    if(diff < minValue) continue;
    minValue = diff;
    indexA = i;
  }
  if(indexA<0){
    setTouchIndex(-1);
    return;
  }
  // finds who is touched:
  int valueA = filtered[indexA];
  int nextIndex  = (indexA+1)% 4;
  int prevIndex  = (indexA+3)% 4;
  int indexB;
  if(touched[nextIndex] && touched[prevIndex]){
    indexB = filtered[nextIndex]<filtered[prevIndex] 
      ? nextIndex
      : prevIndex; 
  }else if(touched[nextIndex]){
    indexB = nextIndex; 
  } else if(touched[nextIndex]){
    indexB = prevIndex; 
  }else{
    indexB = indexA; 
  }
  int valueB = filtered[indexB];
  int index;
    if(indexA == indexB){
      index = 2*indexA;
    }else if(abs(valueA -valueB) < 30 ){
      index = min(indexA,indexB) == 0 && max(indexA,indexB) == 3 
      ? 7
      : 2* min(indexA,indexB) + 1;
    }else{
      index = 2*indexA;
    }
    setTouchIndex(index);
}

void Touch::setTouchIndex(short touchIndex){
  lastTouchIndex = this->touchIndex;
  this->touchIndex = touchIndex;
  if(lastTouchIndex <0 || touchIndex <0){
    return;
  }
  short delta =touchIndex - lastTouchIndex ; 
  if(delta< 0){
    for(short i=0;i<delta ; i++ )
      state->decrementLine();
  }else{
     for(short i=0;i<delta ; i++ )
      state->incrementLine();
  }
  Serial.print("delta ");
  Serial.println(delta);
  Serial.print("state line ");
  Serial.println(state->getMenuStatePointer()->line);

}

