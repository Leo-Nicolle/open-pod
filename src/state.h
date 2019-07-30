#ifndef _STATE_PLAYER_H
#define _STATE_PLAYER_H
#include <string.h>
#include <Arduino.h>
#include <SD.h>

#ifndef _MENU_STATE
#define _MENU_STATE
typedef struct menuState
{
   char path[256];
   char title[16];
   int line;
};
#endif


#define NUM_LINES 18
class State {

    public:
        State();
        File data_file;
        char audio_file_path[256];
        char lines[NUM_LINES][64];
        char root_data_path[64];

        int line_index_file = 0;
        int battery = 100;
        int max_lines = 0;

        menuState menuStates[4];
        int menuIndex = 0;

        void init();
        void readDataLines();
        void setDataFilePath(char* path);
        void incrementLine();
        void decrementLine();
        int forward();
        void backward();
        menuState getMenuState();

};

#endif
