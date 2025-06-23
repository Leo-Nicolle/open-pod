#include "player.h"

void PodPlayer::setup() {
  Serial.println("=== OpenPod Audio System Initialization ===");

  if (!audioPlayer.begin()) {
    Serial.println("Couldn't find VS1053, check pin definitions");
    return;
  }
  // Set volume (0-255, lower = louder)
  audioPlayer.setVolume(20, 20);
  Serial.println("\n=== VS1053 Register Status ===");
  audioPlayer.dumpRegisters();
  audioPlayer.sineTest(0x44, 1000); // 1 second test tone
}

void PodPlayer::loop() {
  // Add your loop logic here
  audioPlayer.loop();
}
