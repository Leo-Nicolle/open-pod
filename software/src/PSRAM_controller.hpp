#pragma once
#include <Arduino.h>

// SPI Pin Definitions for PSRAM
#define PSRAM_MISO PA6
#define PSRAM_MOSI PA7
#define PSRAM_CLK  PA5
#define PSRAM_CS   PA1

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
  static const uint8_t CMD_WRAP_TOGGLE = 0xC0;
  
  // Timing constants (microseconds for delayMicroseconds)
  static const uint32_t T_POWER_UP = 150;     // 150µs power-up time
  static const uint32_t T_RESET = 100;        // 100µs after reset
  static const uint32_t T_CS_PULSE = 18;      // 18ns min CS pulse width (use 1µs for safety)
  static const uint32_t T_CS_SETUP = 1;       // CS setup time
  static const uint32_t T_CS_HOLD = 1;        // CS hold time
  
public:
  bool init() {
    // Initialize SPI1 for APS6404L (64Mbit PSRAM) following datasheet specs
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // Configure SPI1 pins using defined pin mappings
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Configure SPI pins: CLK, MISO, MOSI
    GPIO_InitStruct.Pin = digitalPinToPinName(PSRAM_CLK) | 
                          digitalPinToPinName(PSRAM_MISO) | 
                          digitalPinToPinName(PSRAM_MOSI);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Configure CS pin as GPIO output
    GPIO_InitStruct.Pin = digitalPinToPinName(PSRAM_CS);
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // Pull-up for stable CS
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Set chip select high (inactive)
    cs_port = GPIOA;
    cs_pin = digitalPinToPinName(PSRAM_CS);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    
    // Configure SPI1 - Start with conservative settings for SPI mode
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;      // CPOL = 0
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;          // CPHA = 0
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; // Start slower: ~22MHz
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
      return false;
    }
    
    // Power-up initialization sequence per datasheet
    delay(1);  // Ensure stable power
    
    // Wait for power-up initialization (150µs minimum)
    delayMicroseconds(T_POWER_UP);
    
    // Reset sequence
    resetDevice();
    
    // Read and verify device ID
    uint8_t id[2];
    if (!readDeviceID(id)) {
      Serial.println("Failed to read device ID");
      return false;
    }
    
    // Expected: Manufacturer ID = 0x0D, KGD = 0x5D
    if (id[0] != 0x0D || id[1] != 0x5D) {
      Serial.print("Unexpected ID: 0x");
      Serial.print(id[0], HEX);
      Serial.print(" 0x");
      Serial.println(id[1], HEX);
      Serial.println("Expected: 0x0D 0x5D");
      return false;
    }
    
    Serial.println("PSRAM ID verified: 0x0D 0x5D");
    
    // Test basic communication
    return testPSRAM();
  }
  
  void resetDevice() {
    Serial.println("Resetting PSRAM...");
    
    // Reset Enable command
    chipSelect(true);
    uint8_t cmd = CMD_RESET_ENABLE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    chipSelect(false);
    
    // Wait between commands (datasheet requires immediate sequence)
    delayMicroseconds(1);
    
    // Reset command
    chipSelect(true);
    cmd = CMD_RESET;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    chipSelect(false);
    
    // Wait for reset completion (datasheet: tRST = 50ns, use 100µs for safety)
    delayMicroseconds(T_RESET);
    
    Serial.println("PSRAM reset complete");
  }
  
  bool readDeviceID(uint8_t* id) {
    chipSelect(true);
    
    // Send Read ID command
    uint8_t cmd = CMD_READ_ID;
    if (HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return false;
    }
    
    // Send 24-bit address (000000h for ID read)
    uint8_t addr[3] = {0x00, 0x00, 0x00};
    if (HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return false;
    }
    
    // Read 2 bytes: Manufacturer ID and KGD
    if (HAL_SPI_Receive(&hspi1, id, 2, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return false;
    }
    
    chipSelect(false);
    return true;
  }
  
  void chipSelect(bool active) {
    if (active) {
      // Ensure minimum CS high time between operations
      delayMicroseconds(T_CS_SETUP);
      HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
      // CS setup time
      delayMicroseconds(T_CS_SETUP);
    } else {
      // CS hold time  
      delayMicroseconds(T_CS_HOLD);
      HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
      // Ensure CS high time between operations
      delayMicroseconds(T_CS_PULSE);
    }
  }
  
  void writeData(uint32_t address, uint8_t* data, uint32_t size) {
    // Split large transfers into page-aligned chunks
    const uint32_t PAGE_SIZE = 1024;  // 1KB page size per datasheet
    
    while (size > 0) {
      // Calculate how much we can write in this page
      uint32_t pageOffset = address & (PAGE_SIZE - 1);  // Offset within current page
      uint32_t remainingInPage = PAGE_SIZE - pageOffset;
      uint32_t chunkSize = (size > remainingInPage) ? remainingInPage : size;
      
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
    if (HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    // Send 24-bit address (MSB first)
    uint8_t addr[3];
    addr[0] = (address >> 16) & 0xFF;
    addr[1] = (address >> 8) & 0xFF;
    addr[2] = address & 0xFF;
    if (HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    // Send data
    if (HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    chipSelect(false);
    
    // Small delay between operations for stability
    delayMicroseconds(10);
  }
  
  void readData(uint32_t address, uint8_t* data, uint32_t size) {
    // Split large transfers into smaller chunks
    const uint32_t MAX_CHUNK = 1024;
    
    while (size > 0) {
      uint32_t chunkSize = (size > MAX_CHUNK) ? MAX_CHUNK : size;
      
      readChunk(address, data, chunkSize);
      
      address += chunkSize;
      data += chunkSize;
      size -= chunkSize;
    }
  }
  
  void readChunk(uint32_t address, uint8_t* data, uint32_t size) {
    chipSelect(true);
    
    // Use Fast Read command for better performance
    uint8_t cmd = CMD_FAST_READ;
    if (HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    // Send 24-bit address (MSB first)
    uint8_t addr[3];
    addr[0] = (address >> 16) & 0xFF;
    addr[1] = (address >> 8) & 0xFF;
    addr[2] = address & 0xFF;
    if (HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    // Send 8 dummy cycles (wait cycles) for Fast Read
    uint8_t dummy = 0x00;
    if (HAL_SPI_Transmit(&hspi1, &dummy, 1, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    // Receive data
    if (HAL_SPI_Receive(&hspi1, data, size, HAL_MAX_DELAY) != HAL_OK) {
      chipSelect(false);
      return;
    }
    
    chipSelect(false);
    
    // Small delay between operations
    delayMicroseconds(10);
  }
  
  // Enhanced test function with better diagnostics
  bool testPSRAM() {
    Serial.println("Running comprehensive PSRAM test...");
    
    // Test 1: Basic read/write at address 0
    Serial.println("Test 1: Basic 16-byte read/write at address 0x0000");
    uint8_t testData1[16] = {0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34, 0x56, 0x78,
                            0x87, 0x65, 0x43, 0x21, 0x00, 0xFF, 0x55, 0xAA};
    uint8_t readBuffer1[16] = {0};
    
    writeData(0x0000, testData1, 16);
    delay(1); // Small delay
    readData(0x0000, readBuffer1, 16);
    
    bool test1Pass = true;
    for (int i = 0; i < 16; i++) {
      if (testData1[i] != readBuffer1[i]) {
        Serial.print("Test 1 FAIL at byte ");
        Serial.print(i);
        Serial.print(": wrote 0x");
        Serial.print(testData1[i], HEX);
        Serial.print(", read 0x");
        Serial.println(readBuffer1[i], HEX);
        test1Pass = false;
      }
    }
    
    if (test1Pass) {
      Serial.println("Test 1: PASS");
    }
    
    // Test 2: Page boundary test
    Serial.println("Test 2: 32-byte write across 1KB page boundary");
    uint8_t testData2[32];
    uint8_t readBuffer2[32];
    for (int i = 0; i < 32; i++) {
      testData2[i] = i + 0x80;
    }
    
    // Write across 1KB boundary (1024 = 0x400)
    uint32_t boundaryAddr = 0x3F0;  // 16 bytes before boundary
    writeData(boundaryAddr, testData2, 32);
    delay(1);
    readData(boundaryAddr, readBuffer2, 32);
    
    bool test2Pass = true;
    for (int i = 0; i < 32; i++) {
      if (testData2[i] != readBuffer2[i]) {
        Serial.print("Test 2 FAIL at byte ");
        Serial.print(i);
        Serial.print(" (addr 0x");
        Serial.print(boundaryAddr + i, HEX);
        Serial.print("): wrote 0x");
        Serial.print(testData2[i], HEX);
        Serial.print(", read 0x");
        Serial.println(readBuffer2[i], HEX);
        test2Pass = false;
      }
    }
    
    if (test2Pass) {
      Serial.println("Test 2: PASS");
    }
    
    // Test 3: Different memory locations
    Serial.println("Test 3: Testing various memory locations");
    uint32_t testAddrs[] = {0x1000, 0x8000, 0x10000, 0x7FF00}; // Various locations
    bool test3Pass = true;
    
    for (int addrIdx = 0; addrIdx < 4; addrIdx++) {
      uint32_t addr = testAddrs[addrIdx];
      uint8_t testVal = 0xA5 + addrIdx;
      uint8_t readVal = 0;
      
      writeData(addr, &testVal, 1);
      delay(1);
      readData(addr, &readVal, 1);
      
      if (testVal != readVal) {
        Serial.print("Test 3 FAIL at addr 0x");
        Serial.print(addr, HEX);
        Serial.print(": wrote 0x");
        Serial.print(testVal, HEX);
        Serial.print(", read 0x");
        Serial.println(readVal, HEX);
        test3Pass = false;
      }
    }
    
    if (test3Pass) {
      Serial.println("Test 3: PASS");
    }
    
    bool allTestsPass = test1Pass && test2Pass && test3Pass;
    
    if (allTestsPass) {
      Serial.println("=== ALL PSRAM TESTS PASSED ===");
    } else {
      Serial.println("=== SOME PSRAM TESTS FAILED ===");
    }
    
    return allTestsPass;
  }
  
  // Get memory capacity in bytes
  uint32_t getCapacity() {
    return 8 * 1024 * 1024;  // 8MB (64Mbit)
  }
  
  // Get page size in bytes
  uint32_t getPageSize() {
    return 1024;  // 1KB pages
  }
};