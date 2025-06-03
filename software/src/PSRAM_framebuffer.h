#pragma once
#include <Arduino.h>
#include "ILI9341_GFX.h"

// SPI PSRAM interface for APS6404L-3SQR-SN using SPI1
class SPI_PSRAM {
private:
  SPI_HandleTypeDef hspi1;
  GPIO_TypeDef* cs_port;
  uint16_t cs_pin;
  
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
  
  // Timing constants (nanoseconds)
  static const uint32_t T_CSS = 20;    // CS setup time
  static const uint32_t T_CSH = 20;    // CS hold time
  static const uint32_t T_CSLH = 50;   // CS low to high time
  static const uint32_t T_SHSL = 50;   // CS high between bursts
  
public:
  bool init() {
    // Initialize SPI1 for APS6404L (64Mbit PSRAM) following datasheet specs
    
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // Configure SPI1 pins: PA5 (SCK), PA6 (MISO), PA7 (MOSI)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Configure PA1 as GPIO output for chip select
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // Add pull-up for stable CS
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Set chip select high (inactive)
    cs_port = GPIOA;
    cs_pin = GPIO_PIN_1;
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    
    // Configure SPI1 with conservative settings first
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; // Start slower: ~11MHz
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
      return false;
    }
    
    // Power-up sequence
    delay(1);
    
    // Reset sequence
    resetDevice();
    
    // Read and verify device ID
    uint8_t id[2];
    if (!readDeviceID(id)) {
      return false;
    }
    
    // Expected: Manufacturer ID = 0x0D, KGD = 0x5D
    if (id[0] != 0x0D || id[1] != 0x5D) {
      Serial.print("Unexpected ID: 0x");
      Serial.print(id[0], HEX);
      Serial.print(" 0x");
      Serial.println(id[1], HEX);
      return false;
    }
    
    // Test basic communication
    return testPSRAM();
  }
  
  void resetDevice() {
    // Reset Enable
    chipSelect(true);
    uint8_t cmd = CMD_RESET_ENABLE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    chipSelect(false);
    
    delayMicroseconds(50);
    
    // Reset
    chipSelect(true);
    cmd = CMD_RESET;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    chipSelect(false);
    
    // Wait for reset completion (datasheet: tRST = 20µs max)
    delayMicroseconds(100);
  }
  
  bool readDeviceID(uint8_t* id) {
    chipSelect(true);
    
    uint8_t cmd = CMD_READ_ID;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    
    // Send 24-bit address (000000h for ID read)
    uint8_t addr[3] = {0, 0, 0};
    HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
    
    // Read 2 bytes: Manufacturer ID and KGD
    HAL_SPI_Receive(&hspi1, id, 2, HAL_MAX_DELAY);
    
    chipSelect(false);
    return true;
  }
  
  void chipSelect(bool active) {
    if (active) {
      // Ensure minimum CS high time between operations
      delayMicroseconds(1);
      HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
      // CS setup time
      __DSB();
      __ISB();
      delayMicroseconds(1);
    } else {
      // CS hold time
      delayMicroseconds(1);
      HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
      // Ensure CS high time
      delayMicroseconds(1);
    }
  }
  
  void writeData(uint32_t address, uint8_t* data, uint32_t size) {
    // Split large transfers into smaller chunks to avoid issues
    const uint32_t MAX_CHUNK = 1024;  // 1KB chunks to stay within page boundaries
    
    while (size > 0) {
      uint32_t chunkSize = (size > MAX_CHUNK) ? MAX_CHUNK : size;
      
      // Ensure we don't cross 1KB page boundaries
      uint32_t pageOffset = address & 0x3FF;  // Offset within 1KB page
      uint32_t remainingInPage = 1024 - pageOffset;
      if (chunkSize > remainingInPage) {
        chunkSize = remainingInPage;
      }
      
      writeChunk(address, data, chunkSize);
      
      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
  }
  
  void writeChunk(uint32_t address, uint8_t* data, uint32_t size) {
    chipSelect(true);
    
    // Send write command
    uint8_t cmd = CMD_WRITE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    
    // Send 24-bit address
    uint8_t addr[3];
    addr[0] = (address >> 16) & 0xFF;
    addr[1] = (address >> 8) & 0xFF;
    addr[2] = address & 0xFF;
    HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
    
    // Send data
    HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
    
    chipSelect(false);
    
    // Small delay between writes to ensure proper operation
    delayMicroseconds(10);
  }
  
  void readData(uint32_t address, uint8_t* data, uint32_t size) {
    // Split large transfers into smaller chunks
    const uint32_t MAX_CHUNK = 1024;
    
    while (size > 0) {
      uint32_t chunkSize = (size > MAX_CHUNK) ? MAX_CHUNK : size;
      
      // Ensure we don't cross 1KB page boundaries
      uint32_t pageOffset = address & 0x3FF;
      uint32_t remainingInPage = 1024 - pageOffset;
      if (chunkSize > remainingInPage) {
        chunkSize = remainingInPage;
      }
      
      readChunk(address, data, chunkSize);
      
      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
  }
  
  void readChunk(uint32_t address, uint8_t* data, uint32_t size) {
    chipSelect(true);
    
    // Send read command
    uint8_t cmd = CMD_READ;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    
    // Send 24-bit address
    uint8_t addr[3];
    addr[0] = (address >> 16) & 0xFF;
    addr[1] = (address >> 8) & 0xFF;
    addr[2] = address & 0xFF;
    HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
    
    // Receive data
    HAL_SPI_Receive(&hspi1, data, size, HAL_MAX_DELAY);
    
    chipSelect(false);
    
    // Small delay between reads
    delayMicroseconds(10);
  }
  
  // Enhanced test function with better diagnostics
  bool testPSRAM() {
    Serial.println("Running PSRAM basic test...");
    
    // Test 1: Small aligned transfer
    uint8_t testData1[16] = {0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34, 0x56, 0x78,
                            0x87, 0x65, 0x43, 0x21, 0x00, 0xFF, 0x55, 0xAA};
    uint8_t readBuffer1[16] = {0};
    
    writeData(0x0000, testData1, 16);
    readData(0x0000, readBuffer1, 16);
    
    bool test1Pass = true;
    for (int i = 0; i < 16; i++) {
      if (testData1[i] != readBuffer1[i]) {
        Serial.print("Test 1 fail at byte ");
        Serial.print(i);
        Serial.print(": wrote 0x");
        Serial.print(testData1[i], HEX);
        Serial.print(", read 0x");
        Serial.println(readBuffer1[i], HEX);
        test1Pass = false;
      }
    }
    
    if (!test1Pass) return false;
    Serial.println("Test 1 (16 bytes aligned): PASS");
    
    // Test 2: Page boundary test
    uint8_t testData2[32];
    uint8_t readBuffer2[32];
    for (int i = 0; i < 32; i++) {
      testData2[i] = i + 0x80;
    }
    
    // Write across 1KB boundary
    writeData(0x3F0, testData2, 32);
    readData(0x3F0, readBuffer2, 32);
    
    bool test2Pass = true;
    for (int i = 0; i < 32; i++) {
      if (testData2[i] != readBuffer2[i]) {
        Serial.print("Test 2 fail at byte ");
        Serial.print(i);
        Serial.print(": wrote 0x");
        Serial.print(testData2[i], HEX);
        Serial.print(", read 0x");
        Serial.println(readBuffer2[i], HEX);
        test2Pass = false;
      }
    }
    
    Serial.print("Test 2 (32 bytes across boundary): ");
    Serial.println(test2Pass ? "PASS" : "FAIL");
    
    return test1Pass && test2Pass;
  }
  
  // DMA functions remain the same but need proper cleanup
  void writeDMA(uint32_t address, uint8_t* data, uint32_t size) {
    // Not implemented for initial testing
  }
  
  void readDMA(uint32_t address, uint8_t* data, uint32_t size) {
    // Not implemented for initial testing
  }
  
  void onTransmitComplete() {
    chipSelect(false);
  }
  
  void onReceiveComplete() {
    chipSelect(false);
  }
};

// Rest of the PSRAMFramebuffer class remains the same...