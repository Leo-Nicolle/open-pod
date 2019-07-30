#ifndef _STATE_PLAYER_H
#define _STATE_PLAYER_H
#include <string.h>
#include <Arduino.h>
#include <SD.h>


#define NUM_LINES 18
class State {

    public:
        State();
        File data_file;
        char audio_file[256];
        char data_file_path[256];
        char lines[NUM_LINES][256];
        int line_in_file=0;
        int line_in_lines=0;
        int battery=100;

        void readDataLines();
        void setDataFilePath(char* path);
        void incrementLine();
        void decrementLine();


};

#endif
