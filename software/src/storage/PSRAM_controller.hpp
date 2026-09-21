#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "../pinout.h"

// APS6404L-3SQR-SN is a 64-Megabit chip, i.e. 8 MB - not 64 MB. This used to
// be defined as (64 * 1024 * 1024) (confusing Mbit with MB), which made
// MUSIC_INDEX_SIZE below claim ~58 MB of address space that doesn't
// physically exist: any write past the real 8 MB boundary would wrap around
// and silently corrupt AUDIO_BUFFER_SIZE's region at address 0. Keep this in
// sync with SPI_PSRAM::getCapacity() below, which was already correct.
#define PSRAM_SIZE                (8 * 1024 * 1024) // 8 MB PSRAM chip size
#define AUDIO_BUFFER_BASE_ADDRESS 0x000
// Current-track ring buffer. See memory-improvements.md for the full 8 MB
// budget (4 MB current-track + 1 MB cover buffer + 1 MB next-track prefetch
// + 2 MB music index). 1 MB was reallocated from this buffer (was 5 MB) to
// COVER_BUFFER_SIZE below for the on-device QOI-vs-raw565 cover benchmark.
#define AUDIO_BUFFER_SIZE         (4 * 1024 * 1024) // 4 MB
// Scratch region for album-art decode/cache experiments (see
// software/test/onboard/'s cover format benchmark). Not yet claimed by the
// production cover-fetch path - that's decided by the benchmark's results.
#define COVER_BUFFER_BASE_ADDRESS (AUDIO_BUFFER_BASE_ADDRESS + AUDIO_BUFFER_SIZE)
#define COVER_BUFFER_SIZE         (1 * 1024 * 1024) // 1 MB
// Linear (non-ring) scratch region used to prefetch the start of the next
// track while the current one is still playing, so a track change usually
// needs zero fresh SD activity - see Audio_buffer::prepareNextTrack().
#define NEXT_TRACK_BUFFER_BASE_ADDRESS (COVER_BUFFER_BASE_ADDRESS + COVER_BUFFER_SIZE)
#define NEXT_TRACK_BUFFER_SIZE    (1 * 1024 * 1024) // 1 Mb
#define MUSIC_INDEX_BASE_ADDRESS  (NEXT_TRACK_BUFFER_BASE_ADDRESS + NEXT_TRACK_BUFFER_SIZE)
#define MUSIC_INDEX_SIZE          (PSRAM_SIZE - AUDIO_BUFFER_SIZE - COVER_BUFFER_SIZE - NEXT_TRACK_BUFFER_SIZE) // Remaining PSRAM size for music index

static_assert(AUDIO_BUFFER_SIZE + COVER_BUFFER_SIZE + NEXT_TRACK_BUFFER_SIZE < PSRAM_SIZE,
             "Audio + cover buffer + next-track prefetch regions must leave room in PSRAM for the music index");

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
    // APS6404L is rated up to 33 MHz (SPI), but over long/breadboard wiring a
    // slower bus is more reliable (this fixes intermittent garbage ID reads
    // like 0xFF/0x1A during init). 16 MHz turned out to hang the reads on this
    // breadboard (the ring buffer dropped to 0 and stayed there), so keep 8 MHz
    // until quadSPI/QPI is wired up.
    spiSettings = SPISettings(8000000, MSBFIRST, SPI_MODE0);
  }

  ~SPI_PSRAM() {
    if (spi) {
      spi->end();
      delete spi;
    }
  }

  bool init() {
    if (initialized) return true;
    Serial.println("=== PSRAM INIT START ===");
    Serial.flush();

    setupPins();
    delay(1); // T_POWER_UP

    // Reset + read the ID a few times; the chip is unreliable right after
    // power-up, so do a full reset (not just a re-read) on each attempt.
    for (int attempt = 0; attempt < 5; attempt++) {
      resetDevice();
      delay(1); // reset recovery

      uint8_t id[2] = {0, 0};
      readDeviceID(id);
      Serial.print("PSRAM ID attempt ");
      Serial.print(attempt);
      Serial.print(": 0x");
      Serial.print(id[0], HEX);
      Serial.print(" 0x");
      Serial.println(id[1], HEX);

      if (id[0] == 0x0D && id[1] == 0x5D) {
        initialized = true;
        Serial.println("PSRAM OK");
        Serial.flush();
        return true;
      }
      delay(5);
    }

    Serial.println("PSRAM init failed");
    return false;
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
    delay(1); // extra margin: reset recovery
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