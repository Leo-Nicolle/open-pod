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
   * @brief Number of bits per pixel used in the cache (1, 2, or 4)
   * TODO: Should we just grab this from the cache?
   */
  int bitsPerPixel;
  /**
   * @brief Maximum pixel value based on bits per pixel
   */
  uint8_t maxPixelValue;
  // Pre-computed color blend tables for each transparency level
  /**
   * @brief Pre-computed blend tables for normal and selected rows
   * Normal blend table: text on background (0 = background, max = text)
   * Selected blend table: inverted text on primary (0 = primary, max =
   * background)
   * TODO: seems like we dont use the selected blend table, should we remove it?
   */
  struct BlendTable {
    uint16_t colors[16]; // Support up to 4-bit (16 levels)
  } normalBlend, selectedBlend;

public:
  ListRenderer(ListCache &cache, int bpp = 4)
      : cache(cache), bitsPerPixel(bpp) {
    if (bpp != 1 && bpp != 2 && bpp != 4) {
      bitsPerPixel = 1;
    }
    maxPixelValue = (1 << bitsPerPixel) - 1;
    // Pre-compute blend tables
    precomputeBlendTables();
  }

  void printDebugInfo() {
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
      int cacheRowIndex = y / TRACK_HEIGHT;
      int yInList = y % TRACK_HEIGHT; // Y position within the element

      // Check if this row should be highlighted
      bool isSelected = (cacheRowIndex == selectedRow);

      // Get the cached binary data for this row
      uint8_t *binaryBuffer = cache.getCacheForRow(cacheRowIndex);

      // Calculate colors for this element row
      uint16_t bgColor =
          // TODO: I think we should hold an array of gradents for the selected
          // column instead of recomputing it. We do it once per row but
          // still... we have principles here XD.
          isSelected ? getGradientColor(COLOR_PRIMARY, yInList, TRACK_HEIGHT)
                     : COLOR_BACKGROUND;

      for (int x = startx; x < startx + width; x++) {
        uint8_t pixelValue = 0;

        // Only read from cache if we have valid data
        if (binaryBuffer && cache.cacheValid[cacheRowIndex]) {
          pixelValue =
              getBinaryPixel(binaryBuffer, x, yInList, cache.cacheWidth);
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
   * @brief Render a single element to the display buffer
   * @param elementIndex Index of the element to render
   * @param selected Whether the element is selected or not
   * @param displayBuffer Pointer to the display buffer to render into
   * TODO: not used yet, not sure we need it cause even while scrolling we need
   * to render the whole screen, not just one element
   */
  void renderElementToBuffer(int elementIndex, bool selected,
                             uint16_t *displayBuffer) {
    renderRect(0, BODY_Y + elementIndex * TRACK_HEIGHT, SCREEN_WIDTH,
               TRACK_HEIGHT, displayBuffer, selected ? elementIndex : -1);
  }

  /**
   * @brief Fill the display buffer with the background color
   * @param displayBuffer Pointer to the display buffer to fill
   * @param width Width of the display buffer
   * @param height Height of the area to fill
   * TODO: Should be removed, or use a helper, has nothin to do with the list
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
 * @brief Pre-compute blend tables for normal and selected rows
 * 
 */
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

  /**
   * @brief Get the pixel value from a binary buffer at a specific (x, y)
   * @param buffer Pointer to the binary buffer
   * @param x X coordinate of the pixel
   * @param y Y coordinate of the pixel
   * @param width Width of the binary buffer (in pixels)
   * @TODO: Width should be taken from the cache, not passed as a param
   */
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

  /**
   * @brief Get the gradient color for selected rows
   * @param baseColor The base color to apply the gradient to
   * @param y The y position of the pixel from the top of the element
   * @param totalHeight The total height of the element (used for gradient
   * calculation)
   * @TODO: totalHeight is always TRACK_HEIGHT, should be removed?
   */
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

  /**
   * @brief Set the number of bits per pixel and recompute maxPixelValue and blend tables
   * @param bpp New bits per pixel (1, 2, or 4)
   * @TODO: Since we wont change the bits per pixel at runtime, this method might be
   * removed in the future. Also, bpp should be taken from the cache.
   */
  void setBitsPerPixel(int bpp) {
    if (bpp == 1 || bpp == 2 || bpp == 4) {
      bitsPerPixel = bpp;
      maxPixelValue = (1 << bitsPerPixel) - 1;
      precomputeBlendTables(); // Recompute blend tables
    }
  }
  /**
   * @brief Get the number of bits per pixel used in the renderer
   * @return Number of bits per pixel (1, 2, or 4)
   * TODO: Remove? 
   */
  int getBitsPerPixel() const { return bitsPerPixel; }
};