#ifndef _MP3_DISPLAY_H
#define _MP3_DISPLAY_H


#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#include  "state.h"
#define DISPLAY_CS 6

#define TFT_CS         11
#define TFT_RST        10
#define TFT_DC         13
class MP3Display {
    private:
    Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
    int linesY=0;
    public:
    MP3Display(State* state);
    void draw();
    void drawLines();
    float checkBattery();
    void drawBattery();

    void update();
    State* state;
};

#endif
