#ifndef _MP3_DISPLAY_H
#define _MP3_DISPLAY_H



#include <SPI.h>
#include "Ucglib.h"

#include  "state.h"
#define TFT_CS 11
#define TFT_RST 10
#define TFT_DC 13

class MP3Display {
    private:
    const char X_FIRST_LINE = 2;
    const char Y_FIRST_LINE = 160-2;

    const char LINE_LINEHEIGHT = 10;
    const char NUM_DISPLAY_LINES = 6;


    const char LINE_FONT_HEIGHT = 5;

    Ucglib_ST7735_18x128x160_HWSPI display = Ucglib_ST7735_18x128x160_HWSPI(TFT_DC, TFT_CS, TFT_RST);
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
