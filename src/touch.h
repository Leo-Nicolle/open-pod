#ifndef _MP3_TOUCH_H
#define _MP3_TOUCH_H


#include <Wire.h>
#include "Adafruit_MPR121.h"

#include  "state.h"

class Touch {
    private:
      Adafruit_MPR121 cap = Adafruit_MPR121();
      uint16_t lasttouched = 0;
      uint16_t currtouched = 0;
      State* state;
      short lastTouchIndex =-1;
      short touchIndex =-1;
      void setTouchIndex(short touchIndex);
    public:
    Touch(State*state);
    void update();
    void debug();


};

#endif
