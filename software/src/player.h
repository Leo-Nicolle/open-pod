#pragma once
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
// MP3 and SD card
#define BREAKOUT_RESET  9
#define BREAKOUT_CS     4     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    8      // VS1053 Data/command select pin (output)
#define CARDCS 2     // Card chip select pin
#define DREQ PB3  

class PodPlayer{
  Adafruit_VS1053_FilePlayer vs1053 = 
  // create breakout-example object!
  Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  void printDirectory(File dir, int numTabs);
  void onDataRequest();
  public:
  void setup();
  void loop();
};
