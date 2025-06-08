
#include "player.h"
void PodPlayer::setup() {
  if (!vs1053.begin()) {
    Serial.println("Couldn't find VS1053, check pin definitions");
    return;
  }
  Serial.println("VS1053 found");

  if (!SD.begin(CARDCS)) {
    Serial.println("SD failed or not present");
    return;
  }

  vs1053.setVolume(20, 20);
  attachInterrupt(
      digitalPinToInterrupt(MP3DREQ), [this]() { this->vs1053.feedBuffer(); },
      RISING);
  vs1053.startPlayingFile("/track-1.mp3");
}

void PodPlayer::loop() {
  // Add your loop logic here
}
