#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "ILI9341_driver.h"

class ILI9341_GFX : public Adafruit_GFX, public ILI9341_Driver {

 public:

   // Constructor - Initialize Adafruit_GFX with display dimensions
  ILI9341_GFX() : Adafruit_GFX(320, 240) {
    // Constructor can be empty since setupPins() and initILI9341() will be called explicitly
  }

  void begin() {
    setupPins(); // Setup GPIO pins
    initILI9341(); // Initialize the ILI9341 display
    setRotation(0); // Set default rotation
  }

  // Required Adafruit_GFX virtual methods
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;
    
    setWindow(x, y, x, y);
    writeData16(color);
  }
  
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if ((x >= _width) || (y >= _height)) return;
    
    int16_t x2 = x + w - 1, y2 = y + h - 1;
    if ((x2 < 0) || (y2 < 0)) return;
    
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
    for (int32_t i = 0; i < (int32_t)w * h; i++) {
      writeData16(color);
    }
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    fillRect(x, y, 1, h, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    fillRect(x, y, w, 1, color);
  }

  void fillScreen(uint16_t color) override {
    setWindow(0, 0, _width - 1, _height - 1);
    for (uint32_t i = 0; i < (uint32_t)_width * _height; i++) {
      writeData16(color);
    }
  }

  void setRotation(uint8_t r) override {
    rotation = r & 3;
    switch (rotation) {
      case 0: // 320x240 landscape
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0xE8); // MX=1, MY=1, MV=1 for proper 320x240
        break;
      case 1: // 240x320 portrait
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0x48); // Standard portrait
        break;
      case 2: // 320x240 landscape flipped
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0x28); // Flipped landscape
        break;
      case 3: // 240x320 portrait flipped
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0x88); // Flipped portrait
        break;
    }
  }

  void invertDisplay(bool i) {
    writeCommand(i ? 0x21 : 0x20);
  }

  
};