#ifndef _MP3_MUSIC_SHIELD_H
#define _MP3_MUSIC_SHIELD_H

#include  "state.h"
#include  "utils.h"
#include <SPI.h>
#include <VS1053.h>
#include <SD.h>

// enum
// {
//   BREAKOUT_CS = 17,
//   BREAKOUT_DCS = 33,
//   CARDCS = 5,
//   DREQ = 9
// };

class MP3MusicShield{
private :
  VS1053 musicPlayer(17, 33, 9);

  // Adafruit_VS1053_FilePlayer musicPlayer =
  // Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
public :
  MP3MusicShield(State * state);
  State * state;
  void playFile();
};

#endif
