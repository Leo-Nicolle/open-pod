#pragma once
#include "../../rendering/font_renderer.h"
#include "../theme.h"
#include "../ui_types.h"
#include "list_cache.hpp"
#include <Arduino.h>

/**
 * @brief Renderer for displaying a list of elements from a ListCache
 */
class ListRenderer {
  friend class ListCache;
  friend class BitOperationsTestFixture;
#ifdef UNIT_TEST
public:
#else
private:
#endif
  /**
   * @brief Reference to the ListCache to render from
   */
  const ListCache &cache;
  /**
   * @brief Maximum pixel value based on bits per pixel
   */
  uint8_t maxPixelValue;
  // Pre-computed color blend table for normal rows
  /**
   * @brief Pre-computed blend table for normal rows
   * Normal blend table: text on background (0 = background, max = text)
   */
  struct BlendTable {
    uint16_t colors[16]; // Support up to 4-bit (16 levels)
  } normalBlend;

public:
  ListRenderer(const ListCache &cache) : cache(cache) {
    int bitsPerPixel = cache.getBitsPerPixel();
    maxPixelValue = (1 << bitsPerPixel) - 1;
    // Pre-compute blend table
    precomputeBlendTable();
  }

  void printDebugInfo() {
    int bitsPerPixel = cache.getBitsPerPixel();
    Serial.print("ListRenderer: bitsPerPixel=");
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
  /**
   * @brief Render a viewport rectangle to the display buffer
   * @param startx X list x position to start rendering
   * @param starty Y list y position to start rendering
   * @param width Width of the rectangle to render
   * @param height Height of the rectangle to render
   * @param displayBuffer Pointer to the display buffer to render into
   * @param selectedRow Index of the row to highlight (or -1 for no highlight)
   * @note This method renders a rectangle of pixels from the cache to the
   * display
   */
  void renderRect(int startx, int starty, int width, int height,
                  uint16_t *displayBuffer, int selectedRow) {
    int pixelIndex = 0;
    const int endy = starty + height;

    for (int y = starty; y < endy; y++) {
      // Calculate which cache row this y position corresponds to
      int cacheRowIndex = y / ELEMENT_HEIGHT;
      int yInList = y % ELEMENT_HEIGHT; // Y position within the element

      // Check if this row should be highlighted
      bool isSelected = (cacheRowIndex == selectedRow);

      // Get the cached binary data for this row
      uint8_t *binaryBuffer = cache.getCacheForRow(cacheRowIndex);

      // Calculate colors for this element row
      uint16_t bgColor = isSelected ? getGradientColor(COLOR_PRIMARY, yInList)
                                    : COLOR_BACKGROUND;

      for (int x = startx; x < startx + width; x++) {
        uint8_t pixelValue = 0;

        // Only read from cache if we have valid data
        if (binaryBuffer && cache.cacheValid[cacheRowIndex]) {
          pixelValue = getBinaryPixel(binaryBuffer, x, yInList);
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
  /**
   * @brief Fill the display buffer with the background color
   * @param displayBuffer Pointer to the display buffer to fill
   * @param width Width of the display buffer
   * @param height Height of the area to fill
   */
  void fillBufferWithBackground(uint16_t *displayBuffer, int width,
                                int height) {
    int totalPixels = width * height;
    for (int i = 0; i < totalPixels; i++) {
      displayBuffer[i] = COLOR_BACKGROUND;
    }
  }

#ifdef UNIT_TEST
public:
#else
private:
#endif
  /**
   * @brief Pre-compute blend table for normal rows
   */
  void precomputeBlendTable() {
    // Normal blend table - text on background (0 = background, max = text)
    for (int i = 0; i <= maxPixelValue; i++) {
      float alpha = (float)i / maxPixelValue;
      normalBlend.colors[i] = blendColor(COLOR_BACKGROUND, COLOR_TEXT, alpha);
    }
  }

  /**
   * @brief Get the pixel value from a binary buffer at a specific (x, y)
   * @param buffer Pointer to the binary buffer
   * @param x X coordinate of the pixel
   * @param y Y coordinate of the pixel
   */
  inline uint8_t getBinaryPixel(uint8_t *buffer, int x, int y) {
    int bitsPerPixel = cache.getBitsPerPixel();
    int totalBitIndex = (y * cache.cacheWidth + x) * bitsPerPixel;
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

  /**
   * @brief Get the gradient color for selected rows
   * @param baseColor The base color to apply the gradient to
   * @param y The y position of the pixel from the top of the element
   */
  uint16_t getGradientColor(uint16_t baseColor, int y) {
    float progress = (float)y / (ELEMENT_HEIGHT - 1);
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