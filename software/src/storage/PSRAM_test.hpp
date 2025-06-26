#pragma once
#include "PSRAM_controller.hpp"
#include <Arduino.h>

class PSRAM_test {

private:
public:
  PSRAM_test() {}

  // Simplified test function for debugging
  bool testPSRAM() {
    Serial.println("=== PSRAM TEST BEGIN ===");
    delay(50);

    const uint32_t capacity = psram.getCapacity();
    const uint32_t pageSize = psram.getPageSize();
    Serial.print("PSRAM Capacity: ");
    Serial.print(capacity / 1024);
    Serial.println(" KB");
    Serial.print("Page Size: ");
    Serial.print(pageSize);
    Serial.println(" bytes");

    const uint8_t testPattern = 0xAA;
    const uint32_t testAddr = 0x100000;
    const int testSize = 256;

    // Write test pattern
    uint8_t writeBuffer[testSize];
    for (int i = 0; i < testSize; i++) {
      writeBuffer[i] = testPattern;
    }

    psram.writeData(testAddr, writeBuffer, testSize);
    delay(10);

    // Read back and verify
    uint8_t readBuffer[testSize];
    psram.readData(testAddr, readBuffer, testSize);

    bool success = true;
    for (int i = 0; i < testSize; i++) {
      if (readBuffer[i] != testPattern) {
        Serial.print("Test failed at byte ");
        Serial.print(i);
        Serial.print(": expected 0x");
        Serial.print(testPattern, HEX);
        Serial.print(", got 0x");
        Serial.println(readBuffer[i], HEX);
        success = false;
        break;
      }
    }

    if (success) {
      Serial.println("PSRAM test PASSED");
    } else {
      Serial.println("PSRAM test FAILED");
    }

    Serial.println("=== PSRAM TEST END ===");
    return success;
  }

  bool testExtended() {
    if (!psram.init()) {
      Serial.println("❌ PSRAM initialization failed");
      return false;
    }

    const uint32_t capacity = psram.getCapacity();
    const uint32_t pageSize = psram.getPageSize();

    Serial.println("=== EXTENDED PSRAM TEST ===");
    Serial.print("Testing ");
    Serial.print(capacity / 1024);
    Serial.println(" KB capacity");

    const uint32_t testSize =
        (capacity < 64 * 1024U) ? capacity : 64 * 1024U; // Test up to 64KB
    const uint32_t chunkSize = 1024;

    for (uint32_t addr = 0; addr < testSize; addr += chunkSize) {
      uint32_t currentChunk =
          (chunkSize < (testSize - addr)) ? chunkSize : (testSize - addr);

      // Create test pattern
      uint8_t testData[1024];
      for (uint32_t i = 0; i < currentChunk; i++) {
        testData[i] = (uint8_t)((addr + i) & 0xFF);
      }

      // Write chunk
      psram.writeData(addr, testData, currentChunk);

      // Read back
      uint8_t readBuffer[1024];
      psram.readData(addr, readBuffer, currentChunk);

      // Verify
      for (uint32_t i = 0; i < currentChunk; i++) {
        if (testData[i] != readBuffer[i]) {
          Serial.print("Extended test failed at address 0x");
          Serial.print(addr + i, HEX);
          Serial.print(": expected 0x");
          Serial.print(testData[i], HEX);
          Serial.print(", got 0x");
          Serial.println(readBuffer[i], HEX);
          return false;
        }
      }

      if ((addr % (16 * 1024)) == 0) {
        Serial.print("Tested ");
        Serial.print(addr / 1024);
        Serial.println(" KB");
      }
    }

    Serial.println("Extended PSRAM test PASSED");
    return true;
  }
  // 🧪 Enhanced performance test
  bool testSpeed() {
    if (!psram.init()) {
      Serial.println("❌ PSRAM initialization failed");
      return false;
    }
    Serial.println("=== PSRAM PERFORMANCE TEST ===");
    delay(50);

    // Test with reasonable buffer size
    const uint32_t TEST_SIZE = 4096; // 4KB test
    static uint8_t testBuffer[4096];
    static uint8_t readBuffer[4096];

    Serial.printf("📊 Test Size: %d bytes\n", TEST_SIZE);

    // Fill test buffer
    for (uint32_t i = 0; i < TEST_SIZE; i++) {
      testBuffer[i] = i % 256; // Simple pattern for testing
    }

    // Write test
    Serial.println("🚀 Testing Write Performance...");
    uint32_t startTime = micros();

    psram.writeData(0x200000, testBuffer, TEST_SIZE);

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
    psram.readData(0x200000, readBuffer, TEST_SIZE);
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
      Serial.println("🎯 Mode: SPI Transfer (42MHz)");
      Serial.println("🎉 === PERFORMANCE TEST PASSED ===");
      return true;
    } else {
      Serial.println("❌ Data integrity failed");
      return false;
    }
    Serial.flush();
  }

  // Test basic PSRAM functionality
  bool testBasic() {
    if (!psram.init()) {
      Serial.println("❌ PSRAM initialization failed");
      return false;
    }
        Serial.println("=== PSRAM TEST BEGIN ===");
    delay(50);
    
    const uint32_t capacity = psram.getCapacity();
    const uint32_t pageSize = psram.getPageSize();
    Serial.print("PSRAM Capacity: ");
    Serial.print(capacity / 1024);
    Serial.println(" KB");
    Serial.print("Page Size: ");
    Serial.print(pageSize);
    Serial.println(" bytes");
    
    const uint8_t testPattern = 0xAA;
    const uint32_t testAddr = 0x100000;
    const int testSize = 256;
    
    // Write test pattern
    uint8_t writeBuffer[testSize];
    for (int i = 0; i < testSize; i++) {
      writeBuffer[i] = testPattern;
    }
    
    psram.writeData(testAddr, writeBuffer, testSize);
    delay(10);
    
    // Read back and verify
    uint8_t readBuffer[testSize];
    psram.readData(testAddr, readBuffer, testSize);
    
    bool success = true;
    for (int i = 0; i < testSize; i++) {
      if (readBuffer[i] != testPattern) {
        Serial.print("Test failed at byte ");
        Serial.print(i);
        Serial.print(": expected 0x");
        Serial.print(testPattern, HEX);
        Serial.print(", got 0x");
        Serial.println(readBuffer[i], HEX);
        success = false;
        break;
      }
    }
    
    if (success) {
      Serial.println("PSRAM test PASSED");
    } else {
      Serial.println("PSRAM test FAILED");
    }
    
    Serial.println("=== PSRAM TEST END ===");
    return success;
  }
};