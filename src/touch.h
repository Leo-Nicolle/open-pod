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
    public:
    Touch();
    void update();
    State* state;
};

#endif
