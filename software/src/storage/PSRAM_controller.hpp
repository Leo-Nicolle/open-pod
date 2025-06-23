#pragma once
#include <Arduino.h>
#include <SPI.h>
/*
#### SPI1

| Board Pin | Function |
| --------- | -------- |
| PA4       | NSS      |
| PA5       | SCK      |
| PA6       | MISO     |
| PA7       | MOSI     |

#### SPI2

| Board Pin | Function |
| --------- | -------- |
| PB12      | NSS      |
| PB13      | SCK      |
| PB14      | MISO     |
| PB15      | MOSI     |
*/
// // SPI Pin Definitions for PSRAM - Updated pinout
#define PSRAM_CS PB6
#define PSRAM_CLK PB13
#define PSRAM_MISO PB14
#define PSRAM_MOSI PB15

#define PSRAM_SIZE (64 * 1024 * 1024) // 64 Mb PSRAM chip size
#define AUDIO_BUFFER_BASE_ADDRESS 0x000
#define AUDIO_BUFFER_SIZE (6 * 1024 * 1024) // 6 Mb
#define MUSIC_INDEX_BASE_ADDRESS (AUDIO_BUFFER_BASE_ADDRESS + AUDIO_BUFFER_SIZE)
#define MUSIC_INDEX_SIZE                                                       \
  PSRAM_SIZE - AUDIO_BUFFER_SIZE // Remaining PSRAM size for music index
class SPI_PSRAM {
private:
  SPIClass *spi;
  SPISettings spiSettings;

  /**
   * @brief Size of the PSRAM chip in megabytes
   */
  static const uint8_t CAPACITYMB = 64;
  static uint8_t dmaWriteBuf[256] __attribute__((aligned(4)));
  static uint8_t dmaReadBuf[256] __attribute__((aligned(4)));

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

  // Timing constants (microseconds for delayMicroseconds)
  static const uint32_t T_POWER_UP = 150; // 150µs power-up time
  static const uint32_t T_RESET = 100;    // 100µs after reset
  static const uint32_t T_CS_PULSE = 1;   // CS pulse width
  static const uint32_t T_CS_SETUP = 1;   // CS setup time
  static const uint32_t T_CS_HOLD = 1;    // CS hold time
  SPI_HandleTypeDef *hspi;

  void setupPins() {
    // Configure CS pin as GPIO output
    pinMode(PSRAM_CS, OUTPUT);
    digitalWrite(PSRAM_CS, HIGH); // CS inactive (high)

    // Initialize SPI with custom pins for SPI2
    spi = new SPIClass(PSRAM_MOSI, PSRAM_MISO, PSRAM_CLK);

    // Start SPI with conservative settings
    spi->begin();
    hspi = spi->getHandle();
  }

  inline void chipSelect(bool active) {
    if (active) {
      delayMicroseconds(T_CS_SETUP);
      digitalWrite(PSRAM_CS, LOW); // Active low
      delayMicroseconds(T_CS_SETUP);
    } else {
      delayMicroseconds(T_CS_HOLD);
      digitalWrite(PSRAM_CS, HIGH); // Inactive high
      delayMicroseconds(T_CS_PULSE);
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
    Serial.println();
    Serial.println("=== PSRAM INIT START ===");
    Serial.flush();
    delay(100); // Let serial stabilize

    // Setup pins first
    setupPins();

    Serial.println("Pins configured, starting PSRAM init...");
    Serial.flush();
    delay(10);

    // Power-up initialization sequence per datasheet
    delayMicroseconds(T_POWER_UP);

    Serial.println("Starting reset sequence...");
    Serial.flush();

    // Reset sequence
    resetDevice();

    Serial.println("Reset complete, reading device ID...");
    Serial.flush();

    // Read and verify device ID
    uint8_t id[2] = {0, 0};
    if (!readDeviceID(id)) {
      Serial.println("Failed to read device ID");
      Serial.flush();
      return false;
    }

    Serial.print("Read ID: 0x");
    Serial.print(id[0], HEX);
    Serial.print(" 0x");
    Serial.println(id[1], HEX);
    Serial.flush();

    // Expected: Manufacturer ID = 0x0D, KGD = 0x5D
    if (id[0] != 0x0D || id[1] != 0x5D) {
      Serial.print("Unexpected ID: 0x");
      Serial.print(id[0], HEX);
      Serial.print(" 0x");
      Serial.println(id[1], HEX);
      Serial.println("Expected: 0x0D 0x5D");
      Serial.flush();
      return false;
    }

    Serial.println("PSRAM ID verified: 0x0D 0x5D");
    Serial.flush();

    // Test basic communication
    return true;
  }

  void resetDevice() {
    // Reset Enable command
    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET_ENABLE);
    chipSelect(false);
    spi->endTransaction();

    // Wait between commands
    delayMicroseconds(1);

    // Reset command
    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET);
    chipSelect(false);
    spi->endTransaction();

    // Wait for reset completion
    delayMicroseconds(T_RESET);

    Serial.println("PSRAM reset complete");
    Serial.flush();
  }
  bool spiWriteDMA(uint8_t *data, uint32_t size) {
    SPI_HandleTypeDef *hspi = spi->getHandle();
    HAL_StatusTypeDef res =
        HAL_SPI_Transmit(hspi, data, size, 100); // timeout 100ms
    return res == HAL_OK;
  }

  bool spiReadDMA(uint8_t *out, uint32_t size) {
    SPI_HandleTypeDef *hspi = spi->getHandle();
    HAL_StatusTypeDef res =
        HAL_SPI_Receive(hspi, out, size, 100); // timeout 100ms
    return res == HAL_OK;
  }

  bool readDeviceID(uint8_t *id) {
    spi->beginTransaction(spiSettings);
    chipSelect(true);

    // Send Read ID command
    spi->transfer(CMD_READ_ID);

    // Send 24-bit address (000000h for ID read)
    spi->transfer(0x00);
    spi->transfer(0x00);
    spi->transfer(0x00);

    // Read 2 bytes: Manufacturer ID and KGD
    id[0] = spi->transfer(0x00);
    id[1] = spi->transfer(0x00);

    chipSelect(false);
    spi->endTransaction();

    return true;
  }

  void writeChunkDMA(uint32_t address, uint8_t *data, uint32_t size) {
    uint8_t header[4] = {CMD_WRITE, (uint8_t)(address >> 16),
                         (uint8_t)(address >> 8), (uint8_t)(address)};

    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spiWriteDMA(header, 4);  // envoyer l'entête
    spiWriteDMA(data, size); // envoyer les données
    chipSelect(false);
    spi->endTransaction();
  }

  void readChunkDMA(uint32_t address, uint8_t *data, uint32_t size) {
    uint8_t header[4] = {
        CMD_FAST_READ,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address),
    };

    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spiWriteDMA(header, 4); // envoyer l'entête
    uint8_t dummy = 0x00;
    spiWriteDMA(&dummy, 1); // envoyer dummy pour Fast Read
    spiReadDMA(data, size); // lire les données
    chipSelect(false);
    spi->endTransaction();
  }

  void writeData(uint32_t address, uint8_t *data, uint32_t size) {
    // Split large transfers into page-aligned chunks
    const uint32_t PAGE_SIZE = 1024; // 1KB page size per datasheet

    while (size > 0) {
      // Calculate how much we can write in this page
      uint32_t pageOffset =
          address & (PAGE_SIZE - 1); // Offset within current page
      uint32_t remainingInPage = PAGE_SIZE - pageOffset;
      uint32_t chunkSize = (size > remainingInPage) ? remainingInPage : size;

      // writeChunk(address, data, chunkSize);
      writeChunkDMA(address, data, chunkSize);

      // Update address and data pointers
      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
  }

  void writeChunk(uint32_t address, uint8_t *data, uint32_t size) {
    spi->beginTransaction(spiSettings);
    chipSelect(true);

    // Send write command
    spi->transfer(CMD_WRITE);

    // Send 24-bit address (MSB first)
    spi->transfer((address >> 16) & 0xFF);
    spi->transfer((address >> 8) & 0xFF);
    spi->transfer(address & 0xFF);

    // Send data
    for (uint32_t i = 0; i < size; i++) {
      spi->transfer(data[i]);
    }

    chipSelect(false);
    spi->endTransaction();
  }

  void readData(uint32_t address, uint8_t *data, uint32_t size) {
    // Split large transfers into smaller chunks
    const uint32_t MAX_CHUNK = 256; // Smaller chunks for stability

    while (size > 0) {
      uint32_t chunkSize = (size > MAX_CHUNK) ? MAX_CHUNK : size;

      // readChunk(address, data, chunkSize);
      writeChunkDMA(address, data, chunkSize);

      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
  }

  void readChunk(uint32_t address, uint8_t *data, uint32_t size) {
    spi->beginTransaction(spiSettings);
    chipSelect(true);

    // Use Fast Read command for better performance
    spi->transfer(CMD_FAST_READ);

    // Send 24-bit address (MSB first)
    spi->transfer((address >> 16) & 0xFF);
    spi->transfer((address >> 8) & 0xFF);
    spi->transfer(address & 0xFF);

    // Send 8 dummy cycles (wait cycles) for Fast Read
    spi->transfer(0x00);

    // Receive data
    for (uint32_t i = 0; i < size; i++) {
      data[i] = spi->transfer(0x00);
    }

    chipSelect(false);
    spi->endTransaction();
  }

  // Simplified test function for debugging
  bool testPSRAM() {
    Serial.println("Running basic PSRAM test...");
    Serial.flush();

    // Test 1: Single byte read/write at address 0
    Serial.println("Test 1: Single byte at address 0x0000");
    Serial.flush();

    uint8_t testByte = 0xA5;
    uint8_t readByte = 0x00;

    writeData(0x0000, &testByte, 1);
    delay(1);
    readData(0x0000, &readByte, 1);

    Serial.print("Wrote: 0x");
    Serial.print(testByte, HEX);
    Serial.print(", Read: 0x");
    Serial.println(readByte, HEX);
    Serial.flush();

    if (testByte == readByte) {
      Serial.println("Test 1: PASS");
    } else {
      Serial.println("Test 1: FAIL");
      return false;
    }

    // Test 2: Small array
    Serial.println("Test 2: 4-byte array");
    Serial.flush();

    uint8_t testData[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t rData[4] = {0x00, 0x00, 0x00, 0x00};

    writeData(0x1000, testData, 4);
    delay(1);
    readData(0x1000, rData, 4);

    bool test2Pass = true;
    for (int i = 0; i < 4; i++) {
      if (testData[i] != rData[i]) {
        Serial.print("Test 2 FAIL at byte ");
        Serial.print(i);
        Serial.print(": wrote 0x");
        Serial.print(testData[i], HEX);
        Serial.print(", read 0x");
        Serial.println(rData[i], HEX);
        test2Pass = false;
      }
    }

    if (test2Pass) {
      Serial.println("Test 2: PASS");
      Serial.println("=== BASIC PSRAM TESTS PASSED ===");
    } else {
      Serial.println("Test 2: FAIL");
    }

    Serial.flush();
    return test2Pass;
  }

  // Get memory capacity in bytes
  uint32_t getCapacity() {
    return 8 * 1024 * 1024; // 8MB (64Mbit)
  }

  // Get page size in bytes
  uint32_t getPageSize() {
    return 1024; // 1KB pages
  }

  bool testSpeed() {
    Serial.println("=== PSRAM SPEED TEST BEGIN ===");
    delay(50);

    const uint32_t capacity = getCapacity();
    const uint32_t pageSize = getPageSize();
    Serial.print("PSRAM Capacity: ");
    Serial.print(capacity / 1024);
    Serial.println(" KB");
    Serial.print("Page Size: ");
    Serial.print(pageSize);
    Serial.println(" bytes");

    const int chunkSize = 1024; // 256 bytes per chunk
    static uint8_t buff[chunkSize];
    for (int i = 0; i < chunkSize; i++) {
      buff[i] = i % 256; // Fill buffer with a pattern
    }
    int t0 = micros();
    writeData(0x000000, buff, chunkSize);
    int t1 = micros();
    Serial.print("Write 256 bytes took ");
    Serial.print(t1 - t0);
    Serial.println(" us");
    for (int i = 0; i < chunkSize; i++) {
      buff[i] = 0;
    }

    t0 = micros();
    readData(0x000000, buff, chunkSize);
    t1 = micros();
    Serial.print("Read 256 bytes took ");
    Serial.print(t1 - t0);
    Serial.println(" us");
    // check data integrity
    int badBytes = 0;
    for (int i = 0; i < chunkSize; i++) {
      if (buff[i] != (i % 256)) {
        badBytes++;
      }
    }
    Serial.printf("Data integrity check: %d bad bytes\n", badBytes);
  }

  bool testExtended() {
    const uint32_t capacity = getCapacity();
    const uint32_t pageSize = getPageSize();

    Serial.println("=== EXTENDED PSRAM TEST BEGIN ===");
    delay(50);

    // Allocate buffers (static to avoid stack overflow)
    static uint8_t writeBuf[256];
    static uint8_t readBuf[256];

    // Fill write buffer with known pattern (incrementing bytes)
    for (int i = 0; i < sizeof(writeBuf); i++) {
      writeBuf[i] = i;
    }

    // --- Test 1: Write to start of memory
    Serial.println("Test 1: Write/read at address 0x000000");
    writeData(0x000000, writeBuf, sizeof(writeBuf));
    delay(2);
    memset(readBuf, 0, sizeof(readBuf));
    readData(0x000000, readBuf, sizeof(readBuf));

    for (int i = 0; i < sizeof(writeBuf); i++) {
      if (readBuf[i] != writeBuf[i]) {
        Serial.print("Test 1 FAIL at byte ");
        Serial.print(i);
        Serial.print(": expected ");
        Serial.print(writeBuf[i], HEX);
        Serial.print(", got ");
        Serial.println(readBuf[i], HEX);
        return false;
      }
    }
    Serial.println("Test 1: PASS");

    // --- Test 2: Write near end of memory
    uint32_t endAddr = capacity - sizeof(writeBuf);
    Serial.print("Test 2: Write/read at end of memory (0x");
    Serial.print(endAddr, HEX);
    Serial.println(")");
    for (int i = 0; i < sizeof(writeBuf); i++) {
      writeBuf[i] = ~i;
    }
    writeData(endAddr, writeBuf, sizeof(writeBuf));
    delay(2);
    memset(readBuf, 0, sizeof(readBuf));
    readData(endAddr, readBuf, sizeof(readBuf));

    for (int i = 0; i < sizeof(writeBuf); i++) {
      if (readBuf[i] != writeBuf[i]) {
        Serial.print("Test 2 FAIL at byte ");
        Serial.print(i);
        Serial.print(": expected ");
        Serial.print(writeBuf[i], HEX);
        Serial.print(", got ");
        Serial.println(readBuf[i], HEX);
        return false;
      }
    }
    Serial.println("Test 2: PASS");

    // --- Test 3: Page boundary crossing
    uint32_t boundaryAddr = pageSize - 128; // will cross into next page
    Serial.print("Test 3: Write/read across page boundary at 0x");
    Serial.println(boundaryAddr, HEX);

    for (int i = 0; i < sizeof(writeBuf); i++) {
      writeBuf[i] = i ^ 0xAA; // Different pattern
    }

    writeData(boundaryAddr, writeBuf, sizeof(writeBuf));
    delay(2);
    memset(readBuf, 0, sizeof(readBuf));
    readData(boundaryAddr, readBuf, sizeof(readBuf));

    for (int i = 0; i < sizeof(writeBuf); i++) {
      if (readBuf[i] != writeBuf[i]) {
        Serial.print("Test 3 FAIL at byte ");
        Serial.print(i);
        Serial.print(": expected ");
        Serial.print(writeBuf[i], HEX);
        Serial.print(", got ");
        Serial.println(readBuf[i], HEX);
        return false;
      }
    }
    Serial.println("Test 3: PASS");

    // --- Test 4: Multiple blocks at intervals
    Serial.println("Test 4: Multiple blocks across memory...");

    bool pass = true;
    for (uint32_t addr = 0; addr < capacity; addr += (capacity / 8)) {
      for (int i = 0; i < sizeof(writeBuf); i++) {
        writeBuf[i] = (addr >> 8) ^ i;
      }

      writeData(addr, writeBuf, sizeof(writeBuf));
      delay(1);
      memset(readBuf, 0, sizeof(readBuf));
      readData(addr, readBuf, sizeof(readBuf));

      for (int i = 0; i < sizeof(writeBuf); i++) {
        if (readBuf[i] != writeBuf[i]) {
          Serial.print("Test 4 FAIL at 0x");
          Serial.print(addr + i, HEX);
          Serial.print(": expected ");
          Serial.print(writeBuf[i], HEX);
          Serial.print(", got ");
          Serial.println(readBuf[i], HEX);
          pass = false;
          break;
        }
      }
      if (!pass)
        break;
    }

    if (pass) {
      Serial.println("Test 4: PASS");
    } else {
      Serial.println("Test 4: FAIL");
      return false;
    }

    Serial.println("=== EXTENDED PSRAM TEST PASSED ===");
    return true;
  }
};