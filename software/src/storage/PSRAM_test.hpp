#pragma once
#include <Arduino.h>
#include "PSRAM_controller.hpp"

class PSRAM_test {

  private:
  public:
  PSRAM_test()  {}

  // 🧪 Enhanced performance test
  bool testDMAPerformance() {
    if(!psram.init()){
      Serial.println("❌ PSRAM initialization failed");
      return false;
    }
    Serial.println("=== DMA PERFORMANCE TEST (Large Buffers) ===");
    delay(50);

    // Test with larger buffer size to demonstrate DMA efficiency
    const uint32_t TEST_SIZE = DMA_BUFFER_SIZE * 2; // 8KB test
    static uint8_t testBuffer[DMA_BUFFER_SIZE * 2];
    static uint8_t readBuffer[DMA_BUFFER_SIZE * 2];
    
    Serial.printf("🔧 DMA Buffer Size: %d bytes\n", DMA_BUFFER_SIZE);
    Serial.printf("📊 Test Size: %d bytes\n", TEST_SIZE);

    // Fill test buffer
    for (uint32_t i = 0; i < TEST_SIZE; i++) {
      testBuffer[i] = (i ^ (i >> 8) ^ (i >> 4)) & 0xFF;
    }

    // Write test
    Serial.println("🚀 Testing Write Performance...");
    uint32_t startTime = micros();

    if (!psram.writeData(0x200000, testBuffer, TEST_SIZE, true)) {
      Serial.println("❌ Write test failed");
      return false;
    }

    uint32_t writeTime = micros() - startTime;
    float writeSpeedMBs = ((float)TEST_SIZE * 1e6f) / (writeTime * 1048576.0f);
    char writeSpeedStr[10];
    dtostrf(writeSpeedMBs, 6, 2, writeSpeedStr);

    Serial.printf("📈 Write: %lu bytes in %lu µs (%s MB/s)\n", TEST_SIZE,
                  writeTime, writeSpeedStr);

    delay(10);

    // Read test
    Serial.println("🚀 Testing Read Performance...");
    memset(readBuffer, 0, TEST_SIZE);

    startTime = micros();
    if (!psram.readData(0x200000, readBuffer, TEST_SIZE)) {
      Serial.println("❌ Read test failed");
      return false;
    }
    uint32_t readTime = micros() - startTime;
    float readSpeedMBs = ((float)TEST_SIZE * 1e6f) / (readTime * 1048576.0f);
    char readSpeedStr[10];
    dtostrf(readSpeedMBs, 6, 2, readSpeedStr);

    Serial.printf("📈 Read: %lu bytes in %lu µs (%s MB/s)\n", TEST_SIZE,
                  readTime, readSpeedStr);

    // Verify data
    bool dataOK = true;
    for (uint32_t i = 0; i < TEST_SIZE; i++) {
      if (testBuffer[i] != readBuffer[i]) {
        Serial.printf("❌ Data error at %lu: %02X != %02X\n", i, testBuffer[i],
                      readBuffer[i]);
        dataOK = false;
        break;
      }
    }

    if (dataOK) {
      Serial.println("✅ Data integrity: PERFECT");
      Serial.printf("🎯 Mode: %s\n",
                    psram.isDMAConfigured() ? "DMA" : "Optimized Blocking");
      Serial.println("🎉 === PERFORMANCE TEST PASSED ===");
      return true;
    } else {
      Serial.println("❌ Data integrity failed");
      return false;
    }
  }
};