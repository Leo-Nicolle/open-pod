#pragma once
#include "../rendering/font_renderer.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Binary track renderer for fast caching
class TrackRenderer {
private:
  const FastFont &font;
  static const int MARGIN_LEFT = 5;
  
public:
  TrackRenderer(const FastFont &mainFont)
      : font(mainFont) {}

  // Render track to binary bitmap (1 = text, 0 = background)
  void renderTrackBinary(const char *title, uint8_t* binaryBuffer, 
                         int bufferWidth, int bufferHeight, int x = 0, int width = SCREEN_WIDTH) {
    
    // Clear the binary buffer first
    int totalBits = bufferWidth * bufferHeight;
    int totalBytes = (totalBits + 7) / 8;
    memset(binaryBuffer, 0, totalBytes);
    
    // Set up font renderer to use a temporary pixel buffer
    uint16_t* tempPixelBuffer = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(tempPixelBuffer, width, bufferHeight);
    
    // Clear temp buffer to background color
    for (int i = 0; i < bufferHeight * width; i++) {
      tempPixelBuffer[i] = COLOR_BACKGROUND;
    }
    
    // Find visible portion (same logic as original)
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = fontRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;

      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1) {
          startChar = i;
        }
        endChar = i + 1;
      } else if (startChar != -1) {
        break;
      }

      currentWidth += charWidth;
      if (charStartX > x + width) {
        break;
      }
    }

    // Render visible text to temp buffer
    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + fontRenderer.measureText(upToStart, font) - x;

      // Render to temp buffer with contrasting colors
      fontRenderer.renderText(toRender, offsetX, 10, font, COLOR_TEXT, COLOR_BACKGROUND);
    }
    
    // Convert pixel buffer to binary bitmap
    convertPixelsToBinary(tempPixelBuffer, binaryBuffer, width, bufferHeight, bufferWidth);
  }

  // Original color rendering method (keep for compatibility)
  void renderTrack(const char *title, bool selected, int x = 0, int width = SCREEN_WIDTH) {
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, width, CHUNK_HEIGHT);

    uint16_t baseColor = selected ? COLOR_PRIMARY : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    // Fill background
    if (selected) {
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        uint16_t gradientColor = getGradientColor(baseColor, y, CHUNK_HEIGHT);
        int rowStart = y * width;
        for (int px = 0; px < width; px++) {
          buf[rowStart + px] = gradientColor;
        }
      }
    } else {
      for (int i = 0; i < CHUNK_HEIGHT * width; i++) {
        buf[i] = COLOR_BACKGROUND;
      }
    }

    // Render text (same logic as before)
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = fontRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;
      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1) startChar = i;
        endChar = i + 1;
      } else if (startChar != -1) {
        break;
      }

      currentWidth += charWidth;
      if (charStartX > x + width) break;
    }

    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + fontRenderer.measureText(upToStart, font) - x;

      fontRenderer.renderText(toRender, offsetX, 10, font, textColor, baseColor);
    }
  }

private:
  // Convert rendered pixels to binary bitmap
  void convertPixelsToBinary(uint16_t* pixels, uint8_t* binaryBuffer, 
                            int pixelWidth, int pixelHeight, int binaryWidth) {
    for (int y = 0; y < pixelHeight && y < binaryWidth; y++) { // Prevent overflow
      for (int x = 0; x < pixelWidth && x < binaryWidth; x++) {
        int pixelIndex = y * pixelWidth + x;
        bool isText = (pixels[pixelIndex] != COLOR_BACKGROUND);
        
        setBinaryBit(binaryBuffer, x, y, binaryWidth, isText);
      }
    }
  }
  
  // Set bit in binary buffer
  void setBinaryBit(uint8_t* buffer, int x, int y, int width, bool value) {
    if (x >= width || y >= width) return; // bounds check
    
    int bitIndex = y * width + x;
    int byteIndex = bitIndex / 8;
    int bitOffset = bitIndex % 8;
    
    if (value) {
      buffer[byteIndex] |= (1 << bitOffset);
    } else {
      buffer[byteIndex] &= ~(1 << bitOffset);
    }
  }
  
  // Get bit from binary buffer
  bool getBinaryBit(uint8_t* buffer, int x, int y, int width) {
    if (x >= width || y >= width) return false;
    
    int bitIndex = y * width + x;
    int byteIndex = bitIndex / 8;
    int bitOffset = bitIndex % 8;
    
    return (buffer[byteIndex] & (1 << bitOffset)) != 0;
  }

  // Generate gradient color (keep for color rendering)
  uint16_t getGradientColor(uint16_t baseColor, int y, int totalHeight) {
    float progress = (float)y / (totalHeight - 1);
    uint8_t r = (baseColor >> 11) & 0x1F;
    uint8_t g = (baseColor >> 5) & 0x3F;
    uint8_t b = baseColor & 0x1F;

    float brightnessMod = 1.0f + 0.04f * (0.5f - progress);
    int newR = constrain((int)(r * brightnessMod), 0, 31);
    int newG = constrain((int)(g * brightnessMod), 0, 63);
    int newB = constrain((int)(b * brightnessMod), 0, 31);

    return (newR << 11) | (newG << 5) | newB;
  }
};
// Pure bitmap cache - no screen buffers here!
