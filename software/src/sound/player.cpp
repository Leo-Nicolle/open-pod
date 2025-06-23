#include "player.h"

void PodPlayer::setup() {
  Serial.println("=== OpenPod Audio System Initialization ===");

  if (!audioPlayer.begin()) {
    Serial.println("Couldn't find VS1053, check pin definitions");
    return;
  }
  Serial.println("VS1053 found and configured with enhanced clock settings");

  // Check PSRAM availability
  Serial.println("STM32 PSRAM controller available - testing...");
  if (audioPlayer.enablePSRAM(true)) {
    Serial.println("STM32 PSRAM enabled - 8MB external PSRAM ready!");
    Serial.print("Buffer size: ");
    Serial.print(AUDIO_PSRAM_BUFFER_SIZE / 1024);
    Serial.println(" KB");
  } else {
    Serial.println("STM32 PSRAM initialization failed - using SD-only mode");
  }

  // Set volume (0-255, lower = louder)
  audioPlayer.setVolume(20, 20);

  // Display enhanced audio capabilities
  Serial.println("\n=== Audio System Capabilities ===");
  Serial.printf("Maximum sample rate: %lu Hz\n",
                audioPlayer.getDriver().getMaxSampleRate());
  Serial.println("Supported formats: MP3, WAV, MIDI, Ogg Vorbis");
  Serial.println("Enhanced features:");
  Serial.println("  - Optimized clock configuration for 48kHz+ audio");
  Serial.println("  - Dynamic SPI speed adjustment");
  Serial.println("  - 2KB internal FIFO with 32-byte burst feeding");
  Serial.println("  - PSRAM preloading for gapless playback");

  // Dump VS1053 registers for debugging
  Serial.println("\n=== VS1053 Register Status ===");
  audioPlayer.dumpRegisters();

  // Test sine wave generation with enhanced clock
  Serial.println("\nPlaying test tone with enhanced audio quality...");
  audioPlayer.sineTest(0x44, 1000); // 1 second test tone

  Serial.println("=== Audio System Ready ===\n");
}

void PodPlayer::loop() {
  // Add your loop logic here
  audioPlayer.processDeferred();
}
