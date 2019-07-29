#ifndef _MP3_DISPLAY_H
#define _MP3_DISPLAY_H

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include  "state.h"

class MP3Display {
    private:
    Adafruit_PCD8544 display = Adafruit_PCD8544(24, 23, 11, 12, 13);
    int linesY=0;
    public:
    MP3Display(State* state);
    void draw();
    void drawLines();
    State* state;
};

#endif
