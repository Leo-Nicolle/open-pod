#pragma once
#include <Arduino.h>
#include <SPI.h>

// SPI Pin Definitions for PSRAM
#define PSRAM_CS PB6
#define PSRAM_CLK PB13
#define PSRAM_MISO PB14
#define PSRAM_MOSI PB15

#define PSRAM_SIZE (64 * 1024 * 1024) // 64 Mb PSRAM chip size
#define AUDIO_BUFFER_BASE_ADDRESS 0x000
#define AUDIO_BUFFER_SIZE (6 * 1024 * 1024) // 6 Mb
#define MUSIC_INDEX_BASE_ADDRESS (AUDIO_BUFFER_BASE_ADDRESS + AUDIO_BUFFER_SIZE)
#define MUSIC_INDEX_SIZE (PSRAM_SIZE - AUDIO_BUFFER_SIZE)

#define DMA_BUFFER_SIZE 4096
class SPI_PSRAM {
private:
  SPIClass *spi;
  SPISettings spiSettings;

  static const uint8_t CAPACITYMB = 64;
  static uint8_t dmaWriteBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));
  static uint8_t dmaReadBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));

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

  // Timing constants
  static const uint32_t T_POWER_UP = 150;
  static const uint32_t T_RESET = 100;
  static const uint32_t T_CS_PULSE = 1;
  static const uint32_t T_CS_SETUP = 1;
  static const uint32_t T_CS_HOLD = 1;

  SPI_HandleTypeDef *hspi;

  static volatile bool dmaTransferComplete;
  static volatile HAL_StatusTypeDef lastDmaStatus;
  bool dmaConfigured = false;

  void setupPins() {
    pinMode(PSRAM_CS, OUTPUT);
    digitalWrite(PSRAM_CS, HIGH);
    spi = new SPIClass(PSRAM_MOSI, PSRAM_MISO, PSRAM_CLK);
    spi->begin();
    hspi = spi->getHandle();
    instance = this;
  }

  // 🚀 Manual DMA Configuration for SPI2
  bool configureDMA() {
    Serial.println("🔧 Configuring DMA manually...");

    // Enable DMA clocks
    __HAL_RCC_DMA1_CLK_ENABLE();

    // Configure DMA for SPI2 TX (DMA1 Stream 4 Channel 0)
    hdma_spi_tx.Instance = DMA1_Stream4;
    hdma_spi_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi_tx.Init.Mode = DMA_NORMAL;
    hdma_spi_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_spi_tx) != HAL_OK) {
      Serial.println("❌ Failed to initialize DMA TX");
      return false;
    }

    // Configure DMA for SPI2 RX (DMA1 Stream 3 Channel 0)
    hdma_spi_rx.Instance = DMA1_Stream3;
    hdma_spi_rx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi_rx.Init.Mode = DMA_NORMAL;
    hdma_spi_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_spi_rx) != HAL_OK) {
      Serial.println("❌ Failed to initialize DMA RX");
      return false;
    }

    // Link DMA handles to SPI handle
    __HAL_LINKDMA(hspi, hdmatx, hdma_spi_tx);
    __HAL_LINKDMA(hspi, hdmarx, hdma_spi_rx);

    // Configure NVIC for DMA interrupts
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    Serial.println("✅ DMA configured successfully");
    dmaConfigured = true;
    return true;
  }

  inline void chipSelect(bool active) {
    if (active) {
      delayMicroseconds(T_CS_SETUP);
      digitalWrite(PSRAM_CS, LOW);
      delayMicroseconds(T_CS_SETUP);
    } else {
      delayMicroseconds(T_CS_HOLD);
      digitalWrite(PSRAM_CS, HIGH);
      delayMicroseconds(T_CS_PULSE);
    }
  }

  bool waitForDMAComplete(uint32_t timeoutMs = 1000) {
    uint32_t startTime = millis();

    while (!dmaTransferComplete) {
      if (millis() - startTime > timeoutMs) {
        Serial.println("❌ DMA Transfer timeout!");
        return false;
      }
      yield();
    }

    return (lastDmaStatus == HAL_OK);
  }

  // 🚀 Optimized blocking transfer (fallback when DMA fails)
  bool spiTransferOptimized(uint8_t *txData, uint8_t *rxData, uint32_t size) {
    if (!hspi || size == 0)
      return false;

    // Use HAL's optimized blocking transfer
    HAL_StatusTypeDef status;

    if (txData && rxData) {
      status = HAL_SPI_TransmitReceive(hspi, txData, rxData, size, 1000);
    } else if (txData) {
      status = HAL_SPI_Transmit(hspi, txData, size, 1000);
    } else if (rxData) {
      status = HAL_SPI_Receive(hspi, rxData, size, 1000);
    } else {
      return false;
    }

    return (status == HAL_OK);
  }

public:
  static SPI_PSRAM *instance;
  // 🔧 Manual DMA Configuration
  DMA_HandleTypeDef hdma_spi_tx;
  DMA_HandleTypeDef hdma_spi_rx;

  SPI_PSRAM() : spi(nullptr) {
    spiSettings = SPISettings(133000000, MSBFIRST, SPI_MODE0);
    dmaTransferComplete = true;
    lastDmaStatus = HAL_OK;
  }

  ~SPI_PSRAM() {
    if (spi) {
      spi->end();
      delete spi;
    }
    if (dmaConfigured) {
      HAL_DMA_DeInit(&hdma_spi_tx);
      HAL_DMA_DeInit(&hdma_spi_rx);
    }
  }

  bool init() {
    Serial.println("=== PSRAM INIT START (Manual DMA) ===");
    Serial.flush();
    delay(100);

    setupPins();

    // Try to configure DMA
    if (!configureDMA()) {
      Serial.println(
          "⚠️  DMA configuration failed, using optimized blocking mode");
    }

    delayMicroseconds(T_POWER_UP);
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

    Serial.println("✅ PSRAM ID verified: 0x0D 0x5D");
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

    delayMicroseconds(T_RESET);
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

  // 🚀 Smart write with DMA fallback
  bool writeChunkDMA(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0)
      return false;

    if (size > DMA_BUFFER_SIZE) { // Reserve 4 bytes for header
      const uint32_t CHUNK_SIZE = DMA_BUFFER_SIZE - 4;
      while (size > 0) {
        uint32_t currentChunk = (size > CHUNK_SIZE) ? CHUNK_SIZE : size;
        if (!writeChunkDMA(address, data, currentChunk)) {
          return false;
        }
        address += currentChunk;
        data += currentChunk;
        size -= currentChunk;
      }
      return true;
    }

    // Prepare buffer
    dmaWriteBuf[0] = CMD_WRITE;
    dmaWriteBuf[1] = (uint8_t)(address >> 16);
    dmaWriteBuf[2] = (uint8_t)(address >> 8);
    dmaWriteBuf[3] = (uint8_t)(address);
    memcpy(dmaWriteBuf + 4, data, size);

    chipSelect(true);

    bool success = false;

    // Try DMA first if configured
    if (dmaConfigured) {
      dmaTransferComplete = false;
      lastDmaStatus = HAL_BUSY;

      HAL_StatusTypeDef result =
          HAL_SPI_Transmit_DMA(hspi, dmaWriteBuf, size + 4);

      if (result == HAL_OK) {
        success = waitForDMAComplete();
        if (success) {
          Serial.println("✅ DMA write successful");
        } else {
          Serial.println("⚠️  DMA write timeout, falling back to blocking");
        }
      } else {
        Serial.print("⚠️  DMA init failed (");
        Serial.print(result);
        Serial.println("), using blocking");
      }
    }

    // Fallback to optimized blocking transfer
    if (!success) {
      success = spiTransferOptimized(dmaWriteBuf, nullptr, size + 4);
      if (success) {
        Serial.println("✅ Blocking write successful");
      }
    }

    chipSelect(false);
    return success;
  }

  // 🚀 Smart read with DMA fallback
  bool readChunkDMA(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0)
      return false;

    // Handle large transfers by splitting them
    if (size > DMA_BUFFER_SIZE - 5) { // Reserve 5 bytes for command
      const uint32_t CHUNK_SIZE = DMA_BUFFER_SIZE - 5;
      while (size > 0) {
        uint32_t currentChunk = (size > CHUNK_SIZE) ? CHUNK_SIZE : size;
        if (!readChunkDMA(address, data, currentChunk)) {
          return false;
        }
        address += currentChunk;
        data += currentChunk;
        size -= currentChunk;
      }
      return true;
    }

    constexpr uint8_t CMD_LEN = 5;
    const uint32_t totalLen = CMD_LEN + size;

    // Use DMA buffers for larger transfers
    uint8_t *txBuf = dmaWriteBuf;
    uint8_t *rxBuf = dmaReadBuf;

    // Prepare command
    txBuf[0] = CMD_FAST_READ;
    txBuf[1] = (uint8_t)(address >> 16);
    txBuf[2] = (uint8_t)(address >> 8);
    txBuf[3] = (uint8_t)(address);
    txBuf[4] = 0x00; // dummy byte
    memset(txBuf + CMD_LEN, 0x00, size);

    chipSelect(true);

    bool success = false;

    // Try DMA first if configured
    if (dmaConfigured) {
      dmaTransferComplete = false;
      lastDmaStatus = HAL_BUSY;

      HAL_StatusTypeDef result =
          HAL_SPI_TransmitReceive_DMA(hspi, txBuf, rxBuf, totalLen);

      if (result == HAL_OK) {
        success = waitForDMAComplete();
        if (success) {
          Serial.println("✅ DMA read successful");
        } else {
          Serial.println("⚠️  DMA read timeout, falling back to blocking");
        }
      } else {
        Serial.print("⚠️  DMA init failed (");
        Serial.print(result);
        Serial.println("), using blocking");
      }
    }

    // Fallback to optimized blocking transfer
    if (!success) {
      success = spiTransferOptimized(txBuf, rxBuf, totalLen);
      if (success) {
        Serial.println("✅ Blocking read successful");
      }
    }

    chipSelect(false);

    if (success) {
      memcpy(data, rxBuf + CMD_LEN, size);
    }

    return success;
  }

  bool writeData(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0)
      return false;

    const uint32_t PAGE_SIZE = 1024;
    while (size > 0) {
      uint32_t pageOffset = address & (PAGE_SIZE - 1);
      uint32_t remainingInPage = PAGE_SIZE - pageOffset;
      uint32_t chunkSize = (size > remainingInPage) ? remainingInPage : size;

      if (!writeChunkDMA(address, data, chunkSize)) {
        return false;
      }

      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
    return true;
  }

  bool readData(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0)
      return false;

    // Use larger chunks to maximize DMA buffer utilization
    const uint32_t MAX_CHUNK = DMA_BUFFER_SIZE - 5; // Reserve 5 bytes for command
    while (size > 0) {
      uint32_t chunkSize = (size > MAX_CHUNK) ? MAX_CHUNK : size;
      if (!readChunkDMA(address, data, chunkSize)) {
        return false;
      }
      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
    return true;
  }

  // 🧪 Enhanced performance test
  bool testDMAPerformance() {
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

    if (!writeData(0x200000, testBuffer, TEST_SIZE)) {
      Serial.println("❌ Write test failed");
      return false;
    }

    uint32_t writeTime = micros() - startTime;
    float writeSpeed = (float)TEST_SIZE / writeTime;

    Serial.printf("📈 Write: %lu bytes in %lu µs (%.2f MB/s)\n", TEST_SIZE,
                  writeTime, writeSpeed);

    delay(10);

    // Read test
    Serial.println("🚀 Testing Read Performance...");
    memset(readBuffer, 0, TEST_SIZE);

    startTime = micros();
    if (!readData(0x200000, readBuffer, TEST_SIZE)) {
      Serial.println("❌ Read test failed");
      return false;
    }
    uint32_t readTime = micros() - startTime;
    float readSpeed = (float)TEST_SIZE / readTime;

    Serial.printf("📈 Read: %lu bytes in %lu µs (%.2f MB/s)\n", TEST_SIZE,
                  readTime, readSpeed);

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
                    dmaConfigured ? "DMA" : "Optimized Blocking");
      Serial.println("🎉 === PERFORMANCE TEST PASSED ===");
      return true;
    } else {
      Serial.println("❌ Data integrity failed");
      return false;
    }
  }

  uint32_t getCapacity() { return 8 * 1024 * 1024; }
  uint32_t getPageSize() { return 1024; }
  
  // Direct access to DMA buffers for high-performance operations
  uint8_t* getDMAWriteBuffer() { return dmaWriteBuf; }
  uint8_t* getDMAReadBuffer() { return dmaReadBuf; }
  uint32_t getDMABufferSize() { return DMA_BUFFER_SIZE; }

  // Legacy compatibility
  bool testPSRAM() { return testDMAPerformance(); }
  bool testSpeed() { return testDMAPerformance(); }
  bool testExtended() { return testDMAPerformance(); }

  // Static callback for DMA completion
  static void dmaCompleteCallback(HAL_StatusTypeDef status) {
    dmaTransferComplete = true;
    lastDmaStatus = status;
  }
};
