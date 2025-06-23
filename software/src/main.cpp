/*!
 * @file main.cpp
 * FIXED: Smooth VS1053 audio player for STM32 - no more glitches
 */

#include <Arduino.h>
#include "sound/player.h"

PodPlayer player;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== FIXED VS1053 Audio Player ===");
    Serial.println("Initializing...");
    player.setup();
    player.audioPlayer.startPlayingEnhanced("track-3.flac");
}

void loop() {
    player.loop();
    delay(1);  // Small delay to prevent overwhelming the system
}