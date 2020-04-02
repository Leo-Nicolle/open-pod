#ifndef _MP3_DISPLAY_H
#define _MP3_DISPLAY_H



#include <SPI.h>
#include "Ucglib.h"

#include  "state.h"
#define DISPLAY_CS 6
#define TFT_CS 11
#define TFT_RST 10
#define TFT_DC 13

#ifndef _SCREEN_TEXT
#define _SCREEN_TEXT

typedef struct screenText
{
   char text[128];
   char color[3];
};

#endif



#define NUM_TEXTS 5
class MP3Display {
    private:
    Ucglib_ST7735_18x128x160_HWSPI display = Ucglib_ST7735_18x128x160_HWSPI(TFT_DC, TFT_CS, TFT_RST);
    screenText texts[NUM_TEXTS]; 
    int linesY=0;
    public:
    MP3Display(State* state);
    void draw();
    void drawLines();
    float checkBattery();
    void drawBattery();

    void drawText(char* text,int line);
    void update();
    State* state;
};

#endif
