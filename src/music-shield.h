#ifndef _MP3_MUSIC_SHIELD_H
#define _MP3_MUSIC_SHIELD_H

#include  "state.h"
#include  "utils.h"
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// These are the pins used for the breakout example
#define BREAKOUT_RESET  1000      // VS1053 reset pin (output)
#define BREAKOUT_CS     6     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    10      // VS1053 Data/command select pin (output)
// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     6      // VS1053 chip select pin (output)
#define SHIELD_DCS    10      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 5     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 9       // VS1053 Data request, ideally an Interrupt pin




class MP3MusicShield {
    private:
      Adafruit_VS1053_FilePlayer musicPlayer =
      // create breakout-example object!
      Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
      // create shield-example object!
    // Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);
    public:
    MP3MusicShield(State* state);
    State* state;
    void playFile();
};

#endif
