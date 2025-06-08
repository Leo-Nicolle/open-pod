#pragma once
#include <Adafruit_VS1053.h>
#include <SD.h>
#define CARDCS PA4 // Card chip select pin
#define MP3CS PA8  // VS1053 chip select pin (output)
#define MP3DREQ PB3
#define MP3XDCS PA9 // VS1053 Data/command select pin (output)
#define MP3_RESET PA10

class PodPlayer {
  Adafruit_VS1053_FilePlayer vs1053 =
  Adafruit_VS1053_FilePlayer(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ, CARDCS);
  void printDirectory(File dir, int numTabs);
  void onDataRequest();

public:
  void setup();
  void loop();
};
