#pragma once
#include "ui/ILI9341_driver.h"
#include <Adafruit_GFX.h>
#include <Arduino.h>

class ILI9341_GFX : public Adafruit_GFX, public ILI9341_Driver {

public:
  // Constructor - Initialize Adafruit_GFX with display dimensions
  ILI9341_GFX() : Adafruit_GFX(320, 240) {}

  void begin() {
    setupPins();
    initILI9341();
    setRotation(0);
  }

  // Required Adafruit_GFX virtual methods
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
      return;

    setWindow(x, y, x, y);
    writeData16(color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    if ((x >= _width) || (y >= _height))
      return;

    int16_t x2 = x + w - 1, y2 = y + h - 1;
    if ((x2 < 0) || (y2 < 0))
      return;

    // Clip to screen boundaries
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (x2 >= _width) {
      w = _width - x;
    }
    if (y2 >= _height) {
      h = _height - y;
    }

    setWindow(x, y, x + w - 1, y + h - 1);

    // Optimized fill using 16-bit parallel interface
    digitalWrite(TFT_DC, HIGH); // Data mode

    // Unroll loop for better performance
    uint32_t pixels = (uint32_t)w * h;
    while (pixels >= 8) {
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      pixels -= 8;
    }

    // Handle remaining pixels
    while (pixels--) {
      GPIOC->ODR = color;
      pulseWR();
    }
  }

  // HIGH PERFORMANCE: Push array of pixels
  void pushPixels(uint16_t *colors, uint32_t count) {
    digitalWrite(TFT_DC, HIGH); // Data mode

    // Unrolled loop for maximum speed
    while (count >= 8) {
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      GPIOC->ODR = *colors++;
      pulseWR();
      count -= 8;
    }

    // Handle remaining pixels
    while (count--) {
      GPIOC->ODR = *colors++;
      pulseWR();
    }
  }

  // Push pixels with DMA support (if you add DMA later)
  void pushPixelsDMA(uint16_t *colors, uint32_t count) {
    // For now, fall back to regular push
    pushPixels(colors, count);

    // TODO: Implement DMA transfer
    // This would use DMA2 to transfer from memory to GPIOC->ODR
    // with hardware triggering of WR signal
  }

  // Optimized window push - send pixels to a defined window area
  void pushWindow(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t *colors) {
    setWindow(x, y, x + w - 1, y + h - 1);
    pushPixels(colors, w * h);
  }

  // Fast vertical scroll (hardware accelerated if supported)
  void verticalScroll(int16_t top, int16_t scrolllines, int16_t offset) {
    // ILI9341 supports hardware scrolling
    writeCommand(0x33); // Vertical Scrolling Definition
    writeData16(top);
    writeData16(scrolllines);
    writeData16(320 - top - scrolllines); // Bottom fixed area

    writeCommand(0x37); // Vertical Scrolling Start Address
    writeData16(offset);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    fillRect(x, y, 1, h, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    fillRect(x, y, w, 1, color);
  }

  void fillScreen(uint16_t color) override {
    setWindow(0, 0, _width - 1, _height - 1);

    digitalWrite(TFT_DC, HIGH);

    // Ultra-fast screen fill
    uint32_t pixels = (uint32_t)_width * _height;

    // Fill 16 pixels at a time for maximum speed
    while (pixels >= 16) {
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      GPIOC->ODR = color;
      pulseWR();
      pixels -= 16;
    }

    while (pixels--) {
      GPIOC->ODR = color;
      pulseWR();
    }
  }
  void setupScroll(uint16_t TF, uint16_t scrollHeight, uint16_t BF) {
    // For landscape mode (rotation = 1), scrolling is horizontal visually
    // but still uses the vertical scroll registers internally

    // Set the scrollable region to full height (usually 240px)
    writeCommand(0x33); // VSCRDEF
    writeData(TF >> 8); // Top Fixed Area (TFA)
    writeData(TF & 0xFF);
    writeData(scrollHeight >> 8); // Vertical Scroll Area
    writeData(scrollHeight & 0xFF);
    writeData(BF >> 8); // Bottom Fixed Area (BFA)
    writeData(BF & 0xFF);
  }

  void setScrollOffset(uint16_t offset) {
    // For rotation 1, this visually scrolls from right to left
    writeCommand(0x37); // VSCRSADD
    writeData(offset >> 8);
    writeData(offset & 0xFF);
  }

  void setRotation(uint8_t r) override {
    rotation = r & 3;
    switch (rotation) {
    case 0: // 320x240 landscape
      _width = 320;
      _height = 240;
      writeCommand(0x36);
      writeData(0xE8);
      break;
    case 1: // 240x320 portrait
      _width = 240;
      _height = 320;
      writeCommand(0x36);
      writeData(0x48);
      break;
    case 2: // 320x240 landscape flipped
      _width = 320;
      _height = 240;
      writeCommand(0x36);
      writeData(0x28);
      break;
    case 3: // 240x320 portrait flipped
      _width = 240;
      _height = 320;
      writeCommand(0x36);
      writeData(0x88);
      break;
    }
  }

  void invertDisplay(bool i) { writeCommand(i ? 0x21 : 0x20); }

  // Performance testing
  uint32_t testFillRate() {
    uint32_t start = micros();
    fillScreen(0xF800); // Red
    fillScreen(0x07E0); // Green
    fillScreen(0x001F); // Blue
    fillScreen(0x0000); // Black
    uint32_t elapsed = micros() - start;

    uint32_t pixels = (uint32_t)_width * _height * 4;        // 4 screens
    float rate = (pixels * 1000000.0) / elapsed / 1000000.0; // MPixels/sec

    Serial.print("Fill rate: ");
    Serial.print(rate);
    Serial.println(" MPixels/sec");

    return elapsed;
  }
};