#include "player.h"

void PodPlayer::setup() {
  Serial.println("Starting VS1053 setup...");
  if (!vs1053.begin()) { 
    Serial.println("VS1053 begin failed!");
    return;
  }
  Serial.println("VS1053 begin succeeded!");
  
  Serial.println("Checking SD card...");
  if (!SD.begin(CARDCS)) {
    Serial.println("SD begin failed!");
    return;
  }
  Serial.println("SD begin succeeded!");
  vs1053.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
  Serial.println("VS1053 found");
  vs1053.setVolume(20, 20);
  
  attachInterrupt(
      digitalPinToInterrupt(MP3DREQ), [this]() { this->vs1053.feedBuffer(); },
      CHANGE);

  vs1053.startPlayingFile("/track-1.mp3");
}

void PodPlayer::loop() {}