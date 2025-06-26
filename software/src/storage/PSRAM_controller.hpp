#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "../pinout.h"

#define PSRAM_SIZE                (64 * 1024 * 1024) // 64 Mb PSRAM chip size
#define AUDIO_BUFFER_BASE_ADDRESS 0x000
#define AUDIO_BUFFER_SIZE         (6 * 1024 * 1024) // 6 Mb
#define MUSIC_INDEX_BASE_ADDRESS  (AUDIO_BUFFER_BASE_ADDRESS + AUDIO_BUFFER_SIZE)
#define MUSIC_INDEX_SIZE           PSRAM_SIZE - AUDIO_BUFFER_SIZE // Remaining PSRAM size for music index

// SPI PSRAM interface for APS6404L-3SQR-SN using Arduino SPI library
class SPI_PSRAM {
private:
  SPIClass *spi;
  SPISettings spiSettings;
  bool initialized = false;
  // APS6404L Commands
  static const uint8_t CMD_RESET_ENABLE = 0x66;
  static const uint8_t CMD_RESET = 0x99;
  static const uint8_t CMD_READ_ID = 0x9F;
  static const uint8_t CMD_READ = 0x03;
  static const uint8_t CMD_FAST_READ = 0x0B;
  static const uint8_t CMD_WRITE = 0x02;
  static const uint8_t CMD_ENTER_QUAD = 0x35;
  static const uint8_t CMD_EXIT_QUAD = 0xF5;
  static const uint8_t CMD_FAST_READ_QUAD = 0xEB;
  static const uint8_t CMD_WRITE_QUAD = 0x38;
  static const uint8_t CMD_WRAP_TOGGLE = 0xC0;

  void setupPins() {
    pinMode(PSRAM_CS, OUTPUT);
    digitalWrite(PSRAM_CS, HIGH);
    spi = new SPIClass(PSRAM_MOSI, PSRAM_MISO, PSRAM_CLK);
    spi->begin();
  }

  inline void chipSelect(bool active) {
    if (active) {
      digitalWrite(PSRAM_CS, LOW);
    } else {
      digitalWrite(PSRAM_CS, HIGH);
    }
  }

public:
  SPI_PSRAM() : spi(nullptr) {
    spiSettings = SPISettings(42000000, MSBFIRST, SPI_MODE0);
  }

  ~SPI_PSRAM() {
    if (spi) {
      spi->end();
      delete spi;
    }
  }

  bool init() {
    if(initialized)return true;
    Serial.println("=== PSRAM INIT START ===");
    Serial.flush();

    setupPins();
    delayMicroseconds(150); // T_POWER_UP
    resetDevice();

    uint8_t id[2] = {0, 0};
    if (!readDeviceID(id)) {
      Serial.println("Failed to read device ID");
      return false;
    }

    Serial.print("Read ID: 0x");
    Serial.print(id[0], HEX);
    Serial.print(" 0x");
    Serial.println(id[1], HEX);

    if (id[0] != 0x0D || id[1] != 0x5D) {
      Serial.print("Unexpected ID: 0x");
      Serial.print(id[0], HEX);
      Serial.print(" 0x");
      Serial.println(id[1], HEX);
      return false;
    }

    Serial.println("PSRAM OK");
    Serial.flush();

    // Test basic communication
    return true;
  }

  void resetDevice() {
    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET_ENABLE);
    chipSelect(false);
    spi->endTransaction();

    delayMicroseconds(1);

    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET);
    chipSelect(false);
    spi->endTransaction();

    delayMicroseconds(100); // T_RESET
    Serial.println("PSRAM reset complete");
  }

  bool readDeviceID(uint8_t *id) {
    spi->beginTransaction(spiSettings);
    chipSelect(true);

    spi->transfer(CMD_READ_ID);
    spi->transfer(0x00);
    spi->transfer(0x00);
    spi->transfer(0x00);

    id[0] = spi->transfer(0x00);
    id[1] = spi->transfer(0x00);

    chipSelect(false);
    spi->endTransaction();
    return true;
  }

  void writeData(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0) return;

    spi->beginTransaction(spiSettings);
    chipSelect(true);

    spi->transfer(CMD_WRITE);
    spi->transfer((uint8_t)(address >> 16));
    spi->transfer((uint8_t)(address >> 8));
    spi->transfer((uint8_t)(address));

    for (uint32_t i = 0; i < size; i++) {
      spi->transfer(data[i]);
    }

    chipSelect(false);
    spi->endTransaction();
  }

  void readData(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0) return;

    spi->beginTransaction(spiSettings);
    chipSelect(true);

    spi->transfer(CMD_FAST_READ);
    spi->transfer((uint8_t)(address >> 16));
    spi->transfer((uint8_t)(address >> 8));
    spi->transfer((uint8_t)(address));
    spi->transfer(0x00); // dummy byte

    for (uint32_t i = 0; i < size; i++) {
      data[i] = spi->transfer(0x00);
    }

    chipSelect(false);
    spi->endTransaction();
  }

  uint32_t getCapacity() {
    return 8 * 1024 * 1024; // 8MB
  }

  uint32_t getPageSize() {
    return 1024; // 1KB pages
  }
};
extern SPI_PSRAM psram;