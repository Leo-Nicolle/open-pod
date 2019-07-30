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
        char audio_file_path[256];
        char data_file_path[256];
        char title[64];
        char lines[NUM_LINES][64];
        int line_index_file=0;
        int position_first_line=0;
        int line_index=0;

        int battery=100;

        void readDataLines();
        void setDataFilePath(char* path);
        void incrementLine();
        void decrementLine();


};

#endif
