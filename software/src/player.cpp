#include "player.h"

void PodPlayer::setup() {
  if (!vs1053.begin()) { // initialise the music player
    Serial.println(
        F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1)
      ;
  }
  Serial.println(F("VS1053 found"));
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1)
      ; // don't do anything more
  }
  // list files
  // printDirectory(SD.open("/"), 0);
  vs1053.setVolume(20, 20);
  attachInterrupt(digitalPinToInterrupt(PB3),[this]() { this->vs1053.feedBuffer(); }, CHANGE);

  vs1053.startPlayingFile("/track-1.mp3");
}

void PodPlayer::loop() {}