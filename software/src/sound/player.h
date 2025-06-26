#pragma once
#include "AudioPlayer.h"
#include "../pinout.h"

class PodPlayer {
public:
    AudioPlayer audioPlayer;
    
    PodPlayer() : audioPlayer(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS) {}
    
    void setup();
    void loop();
    void printDirectory(File dir, int numTabs);
    void onDataRequest();
};