#ifndef _STATE_PLAYER_H
#define _STATE_PLAYER_H

#include <Arduino.h>
class State {

    public:
        State();
        char audio_file[256];
        char data_file[256];
        int line=0;
        int battery=100;
};

#endif
