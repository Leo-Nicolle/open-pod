#ifndef _MP3_MUSIC_SHIELD_H
#define _MP3_MUSIC_SHIELD_H

#include  "state.h"
#include  "utils.h"
#include <SPI.h>
#include <SD.h>

// enum
// {
//   BREAKOUT_CS = 17,
//   BREAKOUT_DCS = 33,
//   CARDCS = 5,
//   DREQ = 9
// };
#include <Adafruit_VS1053.h>

class MP3MusicShield{
private :
  Adafruit_VS1053_FilePlayer* musicPlayer;
  static const int CARDCS = 5;

  // Adafruit* musicPlayer;

  // Adafruit_VS1053_FilePlayer musicPlayer =
  // Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
public :
  MP3MusicShield(State * state);
  State * state;
  void playFile();
};

#endif
