#pragma once
#include "AudioPlayer.h"

#define CARDCS     PA4  // Card chip select pin
#define MP3CS      PA11 // VS1053 chip select pin
#define MP3DREQ    PB3 // VS1053 data request pin
#define MP3XDCS    PA9  // VS1053 data/command select pin
#define MP3_RESET  PA10 // VS1053 reset pin

class PodPlayer {
public:
    AudioPlayer audioPlayer;
    
    PodPlayer() : audioPlayer(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS) {}
    
    void setup();
    void loop();
    void printDirectory(File dir, int numTabs);
    void onDataRequest();
};