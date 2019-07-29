#ifndef _STATE_PLAYER_H
#define _STATE_PLAYER_H
#include <string.h>
#include <Arduino.h>
class State {

    public:
        State();
        char audio_file[256];
        char data_file[256];
        char lines[1024];
        int audio_line=0;
        int battery=100;
};

#endif
