#pragma once
#include <Adafruit_VS1053.h>
#include <SD.h>

// Pin definitions
#define CARDCS     PA4  // Card chip select pin
#define MP3CS      PA8  // VS1053 chip select pin
#define MP3DREQ    PB3  // VS1053 data request pin
#define MP3XDCS    PA9  // VS1053 data/command select pin
#define MP3_RESET  PA10 // VS1053 reset pin

class PodPlayer {
public:
    Adafruit_VS1053_FilePlayer vs1053;
    
    PodPlayer() : vs1053(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS) {}
    
    void setup();
    void loop();
    void printDirectory(File dir, int numTabs);
    void onDataRequest();
};