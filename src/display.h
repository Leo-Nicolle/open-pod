#ifndef _MP3_DISPLAY_H
#define _MP3_DISPLAY_H

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include  "player.h"

class MP3Display {
    private:
    Adafruit_PCD8544 screen = Adafruit_PCD8544(24, 23, 11, 12, 13);

    public:
    MP3Display(Player player);
    void draw();
    Player player;
};

#endif
