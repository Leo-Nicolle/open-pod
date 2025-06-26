/*!
 * @file main.cpp
 * FIXED: Smooth VS1053 audio player for STM32 - no more glitches
 */

#include "sound/player.h"
#include "storage/PSRAM_controller.hpp"
#include "storage/PSRAM_test.hpp"
#include "sound/test_file_transfert.hpp"
#include "sound/pinout.h"
#include <Arduino.h>

// PodPlayer player;
// SDToPSRAMTest psramTest(CARDCS, "track-3.mp3"); // Test SD to PSRAM transfer speed
PSRAM_test psramTest;
void setup() {
  Serial.begin(115200);
  delay(1000);
// Enable DMA clock (example for STM32F4)
__HAL_RCC_DMA1_CLK_ENABLE();
__HAL_RCC_DMA2_CLK_ENABLE();
  Serial.println("\n=== FIXED VS1053 Audio Player ===");
  Serial.println("Initializing...");
  // player.setup();
  // psramTest.testTransferSpeed(); // Test SD to PSRAM transfer speed
  psramTest.testDMAPerformance();

  delay(1000);
  Serial.println("Starting playback...");
  // player.audioPlayer.playFile("track-3.mp3");
  // while (player.audioPlayer.isPlaying()) {
  //   player.audioPlayer.loop(); // This now uses optimized feeding
  //   delayMicroseconds(100);               // Reduced delay for faster feeding
  // }

  // // Or for automatic optimization:
  // if (player.audioPlayer.isFLACFile("track-3.flac")) {
  //   player.audioPlayer.startPlayingEnhanced(
  //       "track-3.flac"); // Uses FLAC optimizations
  // } else {
  //   player.audioPlayer.startPlayingSimple(
  //       "track-3.mp3"); // Uses standard optimizations
  // }

  // player.audioPlayer.startPlayingEnhanced("track-3-cd.flac");
}

void loop() {
  // player.loop();
  delayMicroseconds(10); // Small delay to prevent overwhelming the system
}