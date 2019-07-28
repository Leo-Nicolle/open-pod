#ifndef _MP3_PLAYER_H
#define _MP3_PLAYER_H

#include <Arduino.h>
class Player {

    public:
        Player();
        char audio_file[256];
        char data_file[256];
        int line=0;
        int battery=100;
};

#endif
