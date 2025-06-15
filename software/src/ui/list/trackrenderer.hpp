#pragma once
#include "../../rendering/font_renderer.h"
#include "../theme.h"
#include "../ui_types.h"
#include "trackscache.hpp"
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
  const TracksCache &cache;
  int bitsPerPixel;
  uint8_t maxPixelValue;
  // Pre-computed color blend tables for each transparency level
  struct BlendTable {
    uint16_t colors[16]; // Support up to 4-bit (16 levels)
  } normalBlend, selectedBlend;

public:
  TrackRenderer(TracksCache &cache, int bpp = 4)
      : cache(cache), bitsPerPixel(bpp) {
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
  }

  void renderRect(int startx, int starty, int width, int height,
                  uint16_t *displayBuffer, int selectedRow) {
    int pixelIndex = 0;
    const int endy = starty + height;

    for (int y = starty; y < endy; y++) {
      // Calculate which cache row this y position corresponds to
      int cacheRowIndex = y / TRACK_HEIGHT;
      int yInTrack = y % TRACK_HEIGHT; // Y position within the track

      // Check if this row should be highlighted
      bool isSelected = (cacheRowIndex == selectedRow);

      // Get the cached binary data for this row
      uint8_t *binaryBuffer = cache.getCacheForRow(cacheRowIndex);

      // Calculate colors for this track row
      uint16_t bgColor =
          isSelected ? getGradientColor(COLOR_PRIMARY, yInTrack, TRACK_HEIGHT)
                     : COLOR_BACKGROUND;

      for (int x = startx; x < startx + width; x++) {
        uint8_t pixelValue = 0;

        // Only read from cache if we have valid data
        if (binaryBuffer && cache.cacheValid[cacheRowIndex]) {
          pixelValue =
              getBinaryPixel(binaryBuffer, x, yInTrack, cache.cacheWidth);
        }

        uint16_t color;
        if (pixelValue == 0) {
          color = bgColor; // Background color
        } else if (isSelected) {
          // For selected rows: blend text color with gradient background
          // Fix: Correct blending order - text should be visible!
          float alpha = (float)pixelValue / maxPixelValue;
          color = blendColor(bgColor, COLOR_BACKGROUND,
                             alpha); // Text in white/background color
        } else {
          // Use pre-computed normal blend table
          color = normalBlend.colors[pixelValue];
        }

        displayBuffer[pixelIndex++] = color;
      }
    }
  }
  // Render cached track to display buffer
  void renderTrackToBuffer(int trackIndex, bool selected,
                           uint16_t *displayBuffer, int width) {  
    renderRect(0, BODY_Y + trackIndex * TRACK_HEIGHT,SCREEN_WIDTH, TRACK_HEIGHT,
      displayBuffer, selected ? trackIndex : -1);
  }

  // Fill buffer with background color
  void fillBufferWithBackground(uint16_t *displayBuffer, int displayWidth,
                                int height) {
    int totalPixels = displayWidth * height;
    for (int i = 0; i < totalPixels; i++) {
      displayBuffer[i] = COLOR_BACKGROUND;
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

    // Selected blend table - inverted text on primary (0 = primary, max =
    // background)
    for (int i = 0; i <= maxPixelValue; i++) {
      float alpha = (float)i / maxPixelValue;
      selectedBlend.colors[i] =
          blendColor(COLOR_PRIMARY, COLOR_BACKGROUND, alpha);
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

      uint8_t firstPart =
          (buffer[byteIndex] >> bitOffset) & ((1 << bitsInFirstByte) - 1);
      uint8_t secondPart =
          buffer[byteIndex + 1] & ((1 << bitsInSecondByte) - 1);

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