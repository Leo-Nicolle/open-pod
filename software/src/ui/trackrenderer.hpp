#pragma once
#include "../rendering/font_renderer.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Optimized track renderer
class TrackRenderer {
  friend class TracksCache;
  friend class BitOperationsTestFixture;
#ifdef UNIT_TEST
public:
#else
private:
#endif
  const FastFont &font;
  static const int MARGIN_LEFT = 5;
  int bitsPerPixel;
  uint8_t maxPixelValue;
  
  // Pre-computed color blend tables for each transparency level
  struct BlendTable {
    uint16_t colors[16]; // Support up to 4-bit (16 levels)
  } normalBlend, selectedBlend;

public:
  TrackRenderer(const FastFont &mainFont, int bpp = 1)
      : font(mainFont), bitsPerPixel(bpp) {
    if (bpp != 1 && bpp != 2 && bpp != 4) {
      bitsPerPixel = 1;
    }
    maxPixelValue = (1 << bitsPerPixel) - 1;
    // Pre-compute blend tables
    precomputeBlendTables();
  }

void printDebugInfo() {
    Serial.print("TrackRenderer: bitsPerPixel=");
    Serial.println(bitsPerPixel);
    Serial.print("maxPixelValue=");
    Serial.println(maxPixelValue);

    // blendColor(COLOR_BACKGROUND, COLOR_TEXT, alpha)
      // Normal blend table - text on background (0 = background, max = text)
      Serial.println("Normal Blend:");
    for (int i = 0; i <= maxPixelValue; i++) {
      float alpha = (float)i / maxPixelValue;
      Serial.print("Alpha:  ");
      Serial.print(alpha, 2);
      Serial.print(" BACK ");
      Serial.print(COLOR_BACKGROUND, HEX);
      Serial.print(" TEXT ");
      Serial.print(COLOR_TEXT, HEX);
      Serial.print(" -> Color: ");
      uint16_t color = blendColor(COLOR_BACKGROUND, COLOR_TEXT, alpha);
      Serial.println(color, HEX);
    }
    // Serial.print("Normal Blend: [");
    // for(int i = 0; i <= maxPixelValue; i++) {
    //   Serial.print(normalBlend.colors[i], HEX);
    //   if (i < maxPixelValue) Serial.print(", ");
    // }
    // Serial.println("]");
  }

  // Direct render to binary - no temp buffer needed
  void renderTrackBinary(const char *title, uint8_t *binaryBuffer,
                         int bufferWidth, int bufferHeight, int x = 0,
                         int width = SCREEN_WIDTH) {
    // Clear the binary buffer
    int totalBytes = calculateBufferSize(bufferWidth, bufferHeight, bitsPerPixel);
    memset(binaryBuffer, 0, totalBytes);
    
    // Create direct binary renderer
    BinaryFontRenderer binaryRenderer(binaryBuffer, bufferWidth, bufferHeight, bitsPerPixel);
    
    // Find visible portion
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;
    
    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = binaryRenderer.measureText(tempChar, font);
      
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
    
    // Render visible text directly to binary
    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';
      
      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + binaryRenderer.measureText(upToStart, font) - x;
      
      // Direct render to binary buffer
      binaryRenderer.renderText(toRender, offsetX, 10, font);
    }
  }

  // Optimized render to display using pre-computed blend tables
  void renderBinaryToDisplay(uint8_t *binaryBuffer, int bufferWidth,
                             int bufferHeight, uint16_t *displayBuffer,
                             int displayWidth, int displayHeight,
                             uint16_t textColor, uint16_t backgroundColor,
                             bool selected = false) {
    
    // For selected rows, we need to fill with primary color first
    uint16_t baseColor = selected ? COLOR_PRIMARY : backgroundColor;
    
    // Pre-fill the display buffer with base color
    for (int y = 0; y < displayHeight; y++) {
      uint16_t *displayRow = &displayBuffer[y * displayWidth];
      uint16_t fillColor = selected ? getGradientColor(baseColor, y, displayHeight) : baseColor;
      for (int x = 0; x < displayWidth; x++) {
        displayRow[x] = fillColor;
      }
    }
    
    // Now render the text on top
    for (int y = 0; y < bufferHeight && y < displayHeight; y++) {
      uint16_t *displayRow = &displayBuffer[y * displayWidth];
      
      for (int x = 0; x < bufferWidth && x < displayWidth; x++) {
        uint8_t pixelValue = getBinaryPixel(binaryBuffer, x, y, bufferWidth);
        
        if (pixelValue > 0) {
          // Calculate the actual color based on selection state
          uint16_t color;
          if (selected) {
            // For selected rows: blend from primary (with gradient) to background color
            uint16_t gradientBase = getGradientColor(COLOR_PRIMARY, y, displayHeight);
            float alpha = (float)pixelValue / maxPixelValue;
            color = blendColor(gradientBase, backgroundColor, alpha);
          } else {
            // For unselected rows: use the pre-computed normal blend table
            color = normalBlend.colors[pixelValue];
            // color = pixelValue<<4;
          }
          
          displayRow[x] = color;
        }
      }
    }
  }

#ifdef UNIT_TEST
public:
#else
private:
#endif
  // Pre-compute blend tables for all transparency levels
  void precomputeBlendTables() {
    // Normal blend table - text on background (0 = background, max = text)
    for (int i = 0; i <= maxPixelValue; i++) {
      float alpha = (float)i / maxPixelValue;
      normalBlend.colors[i] = blendColor(COLOR_BACKGROUND, COLOR_TEXT, alpha);
    }
    
    // Selected blend table - inverted text on primary (0 = primary, max = background)
    for (int i = 0; i <= maxPixelValue; i++) {
      float alpha = (float)i / maxPixelValue;
      selectedBlend.colors[i] = blendColor(COLOR_PRIMARY, COLOR_BACKGROUND, alpha);
    }
  }
  
  // Optimized pixel reading
  inline uint8_t getBinaryPixel(uint8_t *buffer, int x, int y, int width) {
    int totalBitIndex = (y * width + x) * bitsPerPixel;
    int byteIndex = totalBitIndex >> 3;
    int bitOffset = totalBitIndex & 7;
    
    if (bitOffset + bitsPerPixel <= 8) {
      return (buffer[byteIndex] >> bitOffset) & maxPixelValue;
    } else {
      int bitsInFirstByte = 8 - bitOffset;
      int bitsInSecondByte = bitsPerPixel - bitsInFirstByte;
      
      uint8_t firstPart = (buffer[byteIndex] >> bitOffset) & ((1 << bitsInFirstByte) - 1);
      uint8_t secondPart = buffer[byteIndex + 1] & ((1 << bitsInSecondByte) - 1);
      
      return firstPart | (secondPart << bitsInFirstByte);
    }
  }
  
  // Generate gradient color
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
  
  // Calculate buffer size needed
  static int calculateBufferSize(int width, int height, int bpp) {
    int totalBits = width * height * bpp;
    return (totalBits + 7) / 8;
  }
  
  void setBitsPerPixel(int bpp) {
    if (bpp == 1 || bpp == 2 || bpp == 4) {
      bitsPerPixel = bpp;
      maxPixelValue = (1 << bitsPerPixel) - 1;
      precomputeBlendTables(); // Recompute blend tables
    }
  }
  
  int getBitsPerPixel() const { return bitsPerPixel; }
};