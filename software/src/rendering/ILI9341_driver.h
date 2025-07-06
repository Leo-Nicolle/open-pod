#pragma once
#include <Adafruit_GFX.h>
#include <Arduino.h>

class ILI9341_Driver {
public:
  // Pin definitions - CORRECTED
  static const int TFT_DC = PB8;   // Data/Command
  static const int TFT_WR = PB9;   // Write
  static const int TFT_RD = PB10;  // Read
  static const int TFT_RST = PB12; // Reset

  // Data bus mapping:
  // D0-D15 → PC0-PC15 (perfect 1:1 mapping!)

  void setupPins() {
    // Control pins
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_WR, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    pinMode(TFT_RD, OUTPUT);

    digitalWrite(TFT_WR, HIGH);
    digitalWrite(TFT_RST, HIGH);
    digitalWrite(TFT_RD, HIGH);

    // Enable GPIO clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

    // Set all PC0-PC15 pins to output mode (data bus)
    GPIOC->MODER = 0x55555555; // Set all 16 pins to output mode (01 pattern)

    // Set all data pins to push-pull, high speed
    GPIOC->OTYPER = 0x0000;      // All push-pull
    GPIOC->OSPEEDR = 0xFFFFFFFF; // All high speed
  }

  inline void pulseWR() {
    // GPIOB->BSRR = (1 << (9 + 16));
    // // for(int i = 0; i< 250; i++){
    //   __NOP();
    // // }
    // GPIOB->BSRR = (1 << 9);
    GPIOB->BSRR = (1 << (9 + 16)); // WR LOW
    __NOP();                // More delay
    GPIOB->BSRR = (1 << 9); // WR HIGH
  }

  inline void write16(uint16_t data) {
    GPIOC->ODR = data; // Set data
    __NOP();
    __NOP(); // Data setup time
    pulseWR();
  }
  void setBusToInput() {
    // Set all PC0-PC15 pins to input mode
    GPIOC->MODER = 0x00000000; // All input mode (00 pattern)
  }

  void setBusToOutput() {
    // Set all PC0-PC15 pins back to output mode
    GPIOC->MODER = 0x55555555; // All output mode (01 pattern)
  }

  uint16_t readBus16() {
    //  Direct 16-bit read from PC0-PC15
    return GPIOC->IDR & 0xFFFF;
  }

  uint16_t read16() {
    setBusToInput();

    // Longer setup time before read
    delayMicroseconds(1);

    // RD low → start read
    digitalWrite(TFT_RD, LOW);
    delayMicroseconds(5); // Much longer delay for read access time

    uint16_t value = readBus16();

    // RD high → end read
    digitalWrite(TFT_RD, HIGH);
    delayMicroseconds(1); // Recovery time

    setBusToOutput();
    return value;
  }

  void writeCommand(uint8_t cmd) {
    digitalWrite(TFT_DC, LOW);
    write16(cmd);
  }
  void writeData2x8(uint16_t data) {
    writeData(data >> 8);
    writeData(data & 0xFF);
  }

  void writeData(uint8_t data) {
    digitalWrite(TFT_DC, HIGH);
    write16(data);
  }

  void writeData16(uint16_t data) {
    digitalWrite(TFT_DC, HIGH);
    write16(data);
  }

  void readRegisterBytes(uint8_t reg, uint8_t *buffer, uint8_t len) {
    writeCommand(reg);
    digitalWrite(TFT_DC, HIGH); // Read data mode

    // Multiple dummy reads - some displays need this
    read16(); // First dummy
    read16(); // Second dummy (sometimes needed)

    for (uint8_t i = 0; i < len; i++) {
      uint16_t val = read16();
      buffer[i] = val & 0xFF;
      delayMicroseconds(10); // Small delay between reads
    }
  }

  uint16_t readRegister(uint8_t reg) {
    writeCommand(reg);
    digitalWrite(TFT_DC, HIGH); // Data mode
    read16();                   // Dummy read
    return read16();
  }

  void initILI9341() {
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);

    writeCommand(0x01); // Software reset
    delay(100);

    writeCommand(0x28); // Display OFF

    // Power Control A
    writeCommand(0xCB);
    writeData(0x39);
    writeData(0x2C);
    writeData(0x00);
    writeData(0x34);
    writeData(0x02);

    // Power Control B
    writeCommand(0xCF);
    writeData(0x00);
    writeData(0xC1);
    writeData(0x30);

    // Driver Timing Control A
    writeCommand(0xE8);
    writeData(0x85);
    writeData(0x00);
    writeData(0x78);

    // Driver Timing Control B
    writeCommand(0xEA);
    writeData(0x00);
    writeData(0x00);

    // Power on Sequence Control
    writeCommand(0xED);
    writeData(0x64);
    writeData(0x03);
    writeData(0x12);
    writeData(0x81);

    // Pump Ratio Control
    writeCommand(0xF7);
    writeData(0x20);

    // Power Control 1
    writeCommand(0xC0);
    writeData(0x23);

    // Power Control 2
    writeCommand(0xC1);
    writeData(0x10);

    // VCOM Control 1
    writeCommand(0xC5);
    writeData(0x3E);
    writeData(0x28);

    // VCOM Control 2
    writeCommand(0xC7);
    writeData(0x86);

    // Memory Access Control for 320x240 landscape mode
    writeCommand(0x36);
    // writeData(0xE8); // Landscape mode with proper RGB order
    writeData(0x48);

    // Pixel Format Set
    writeCommand(0x3A);
    writeData(0x55); // 16-bit color

    // Frame Rate Control
    writeCommand(0xB1);
    writeData(0x00);
    writeData(0x18);

    // Display Function Control
    writeCommand(0xB6);
    writeData(0x08);
    writeData(0x82);
    writeData(0x27);

    // 3Gamma Function Disable
    // writeCommand(0xF2);
    // writeData(0x00);

    // Gamma Curve Selected
    // writeCommand(0x26);
    // writeData(0x01);

    // // Set Gamma
    // writeCommand(0xE0);
    // writeData(0x0F);
    // writeData(0x31);
    // writeData(0x2B);
    // writeData(0x0C);
    // writeData(0x0E);
    // writeData(0x08);
    // writeData(0x4E);
    // writeData(0xF1);
    // writeData(0x37);
    // writeData(0x07);
    // writeData(0x10);
    // writeData(0x03);
    // writeData(0x0E);
    // writeData(0x09);
    // writeData(0x00);

    // // Set Gamma
    // writeCommand(0xE1);
    // writeData(0x00);
    // writeData(0x0E);
    // writeData(0x14);
    // writeData(0x03);
    // writeData(0x11);
    // writeData(0x07);
    // writeData(0x31);
    // writeData(0xC1);
    // writeData(0x48);
    // writeData(0x08);
    // writeData(0x0F);
    // writeData(0x0C);
    // writeData(0x31);
    // writeData(0x36);
    // writeData(0x0F);

    // Sleep Out
    writeCommand(0x11);
    delay(120);

    // Display ON
    writeCommand(0x29);
    delay(20);
  }

  void printRegister(const char *name, uint8_t reg, uint8_t len) {
    uint8_t data[4] = {0};
    readRegisterBytes(reg, data, len);

    Serial.print(name);
    Serial.print(" (0x");
    Serial.print(reg, HEX);
    Serial.print("): ");

    for (uint8_t i = 0; i < len; i++) {
      Serial.print("0x");
      if (data[i] < 0x10)
        Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column Address Set
    writeCommand(0x2A);
    writeData(x0 >> 8);   // High byte of x0
    writeData(x0 & 0xFF); // Low byte of x0
    writeData(x1 >> 8);   // High byte of x1
    writeData(x1 & 0xFF); // Low byte of x1

    // Page Address Set
    writeCommand(0x2B);
    writeData(y0 >> 8);   // High byte of y0
    writeData(y0 & 0xFF); // Low byte of y0
    writeData(y1 >> 8);   // High byte of y1
    writeData(y1 & 0xFF); // Low byte of y1

    writeCommand(0x2C); // Memory Write
  }

public:
  ILI9341_Driver() {}

  void invertDisplay(bool i) { writeCommand(i ? 0x21 : 0x20); }

  // Public methods
  void begin() {
    setupPins();
    initILI9341();
  }

  void detectController() {
    Serial.println("=== Controller Detection ===");

    // Try different ID registers
    printRegister("ID (0x04)", 0x04, 3);
    printRegister("ID (0xD3)", 0xD3, 4); // Alternative ID register
    printRegister("ID (0xDA)", 0xDA, 1); // Read ID1
    printRegister("ID (0xDB)", 0xDB, 1); // Read ID2
    printRegister("ID (0xDC)", 0xDC, 1); // Read ID3

    // Check if it might be ST7789
    Serial.println("Checking for ST7789...");
    writeCommand(0x04); // Get ID for ST7789
    digitalWrite(TFT_DC, HIGH);
    read16(); // dummy
    uint8_t id1 = read16() & 0xFF;
    uint8_t id2 = read16() & 0xFF;
    uint8_t id3 = read16() & 0xFF;

    Serial.print("ST7789 Style ID: 0x");
    Serial.print(id1, HEX);
    Serial.print(" 0x");
    Serial.print(id2, HEX);
    Serial.print(" 0x");
    Serial.println(id3, HEX);
  }

  // Color conversion helpers for RGB888 to RGB565
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  uint16_t color565(uint32_t color) {
    return color565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
  }

  void printStatus() {
    printRegister("Status", 0x09, 4);
    printRegister("Power Mode", 0x0A, 1);
    printRegister("MADCTL Status", 0x0B, 1);
    printRegister("Pixel Format", 0x0C, 1);
    printRegister("Image Format", 0x0D, 1);
    printRegister("Signal Mode", 0x0E, 1);
    printRegister("Self Diag", 0x0F, 1);
  }
};