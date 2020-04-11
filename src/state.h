#ifndef _STATE_PLAYER_H
#define _STATE_PLAYER_H
#include <string.h>
#include <Arduino.h>
#include <SD.h>
#include <string.h>

#ifndef _MENU_STATE
#define _MENU_STATE
typedef struct menuState
{
   char path[256];
   char title[16];
    // current line index into the data file
   int line=0;
   int selectedLine=0;
};
#endif
#define NUM_MENUS 4
#define NUM_LINES 18
class State {

    public:
        State();
        File data_file;
        // current file path (being played)
        char audio_file_path[256];
        // current lines displayed on screen
        char lines[NUM_LINES][64];
        // SD card root file path (should not change during runtime)
        char root_data_path[64];
        // SD card root audio path (should not change during runtime)
        char root_music_path[64];

        // number of lines into the data file
        int max_lines = 0;

        // battery power
        int battery = 100;

        // saves the menu state (the line number into this menu)
        menuState menuStates[NUM_MENUS];
        // current menu index
        int menuIndex = 0;

        void init();
        // read the data to display from SD card
        void readDataLines();
        // change the date file path (depends on the menu)
        void setDataFilePath(char* path);
        // go down of one line into a menu
        void incrementLine();
        // go up of one line into a menu
        void decrementLine();

        // enter the next menu
        int forward();
        // go to previous menu
        void backward();
        // get current menu state
        menuState getMenuState();
        menuState* getMenuStatePointer();



        void getDescription(char* line, char* result);


};

#endif
