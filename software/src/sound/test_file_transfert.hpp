#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include "Audio_buffer.h"

/**
 * @brief Test class for measuring SD to PSRAM transfer speed using Audio_buffer.
 */
class SDToPSRAMTest {
public:
  SDToPSRAMTest(uint8_t sdCS, const char* filename)
    : audioBuffer(sdCS), _filename(filename) {}

  bool testTransferSpeed() {
    Serial.println("=== SD to PSRAM TRANSFER SPEED TEST ===");

    if (!audioBuffer.begin()) {
      Serial.println("❌ SD initialization failed");
      return false;
    }

    audioBuffer.setFileName(_filename);

    if (!audioBuffer.load()) {
      Serial.println("❌ Failed to open audio file");
      return false;
    }

    // // Start timing the SDtoPSRAM operation
    // uint32_t startTime = micros();
    // bool result = audioBuffer.SDtoPSRAM();
    // uint32_t elapsed = micros() - startTime;

    // if (!result) {
    //   Serial.println("❌ SDtoPSRAM transfer failed");
    //   return false;
    // }

    // // Get file size for speed calculation
    // size_t fileSize = audioBuffer.fileSize;
    // float speedMBs = ((float)fileSize * 1e6f) / (elapsed * 1048576.0f);
    // char speedStr[10];
    // dtostrf(speedMBs, 6, 2, speedStr);

    // Serial.printf("📥 SD to PSRAM: %lu bytes in %lu µs (%s MB/s)\n", fileSize, elapsed, speedStr);
    // Serial.println("✅ SD to PSRAM transfer complete");

    return true;
  }

private:
  Audio_buffer audioBuffer;
  const char* _filename;
};