#pragma once
#include "fonts/Fastfont.h"
#include "ui/theme.h"
#include <Arduino.h>
#include <algorithm>
// Color cache for fast blending
class ColorCache {
private:
  struct CacheEntry {
    uint16_t bgColor;
    uint16_t fgColor;
    uint16_t blendTable[256]; // Pre-computed blend values
    bool valid;
  };

  static const int CACHE_SIZE = 4;
  CacheEntry cache[CACHE_SIZE];
  int nextSlot = 0;

public:
  // Get pre-computed blend table for color pair
  const uint16_t *getBlendTable(uint16_t bgColor, uint16_t fgColor) {
    // Check cache
    for (int i = 0; i < CACHE_SIZE; i++) {
      if (cache[i].valid && cache[i].bgColor == bgColor &&
          cache[i].fgColor == fgColor) {
        return cache[i].blendTable;
      }
    }

    // Not in cache, compute it
    CacheEntry &entry = cache[nextSlot];
    entry.bgColor = bgColor;
    entry.fgColor = fgColor;
    entry.valid = true;

    // Pre-compute all 256 blend levels
    computeBlendTable(bgColor, fgColor, entry.blendTable);

    nextSlot = (nextSlot + 1) % CACHE_SIZE;
    return entry.blendTable;
  }

private:
  void computeBlendTable(uint16_t bg, uint16_t fg, uint16_t *table) {
    // Extract RGB components
    uint8_t bg_r = (bg >> 11) & 0x1F;
    uint8_t bg_g = (bg >> 5) & 0x3F;
    uint8_t bg_b = bg & 0x1F;

    uint8_t fg_r = (fg >> 11) & 0x1F;
    uint8_t fg_g = (fg >> 5) & 0x3F;
    uint8_t fg_b = fg & 0x1F;

    // Apply gamma correction for more perceptually uniform blending
    for (int alpha = 0; alpha < 256; alpha++) {
      float normalizedAlpha = alpha / 255.0f;

      // Apply gamma curve to alpha for better visual weight consistency
      float gammaCorrectedAlpha =
          pow(normalizedAlpha, 1.8f); // Adjust gamma value

      uint8_t r = bg_r + ((fg_r - bg_r) * gammaCorrectedAlpha);
      uint8_t g = bg_g + ((fg_g - bg_g) * gammaCorrectedAlpha);
      uint8_t b = bg_b + ((fg_b - bg_b) * gammaCorrectedAlpha);

      table[alpha] = (r << 11) | (g << 5) | b;
    }
  }
};

// Main font renderer
class FastFontRenderer {
private:
  ColorCache colorCache;
  uint16_t *renderBuffer;
  int bufferWidth;
  int bufferHeight;

public:
  FastFontRenderer(uint16_t *buffer, int width, int height)
      : renderBuffer(buffer), bufferWidth(width), bufferHeight(height) {}

  // Render text to buffer with anti-aliasing
  int renderText(const char *text, int x, int y, const FastFont &font,
                 uint16_t color, uint16_t bgColor) {
    int startX = x;
    const uint16_t *blendTable = colorCache.getBlendTable(bgColor, color);

    while (*text) {
      char c = *text++;

      // Skip characters outside font range
      if (c < font.firstChar || c > font.lastChar) {
        continue;
      }

      const FastGlyph &glyph = font.glyphs[c - font.firstChar];

      // Apply kerning if available
      if (font.hasKerning && *text) {
        x += getKerning(font, c, *text);
      }

      // Render glyph
      renderGlyph(glyph, x + glyph.xOffset, y + glyph.yOffset, blendTable,
                  bgColor);

      x += glyph.xAdvance;
    }

    return x - startX; // Return rendered width
  }

  // Render single glyph with alpha blending
  void renderGlyph(const FastGlyph &glyph, int x, int y,
                   const uint16_t *blendTable, uint16_t bgColor) {
    const uint8_t *alphaPtr = glyph.alphaData;

    // Detect if background is dark (like your blue selection)
    bool isDarkBackground = isColorDark(bgColor);

    for (int dy = 0; dy < glyph.height; dy++) {
      bool rowVisible = (y + dy >= 0 && y + dy < bufferHeight);
      int bufferOffset = (y + dy) * bufferWidth + x;

      for (int dx = 0; dx < glyph.width; dx++) {
        uint8_t alpha = *alphaPtr++;

        if (rowVisible && x + dx >= 0 && x + dx < bufferWidth) {
          if (alpha == 0) {
            renderBuffer[bufferOffset + dx] = bgColor;
          } else {
            // Adjust alpha for dark backgrounds to make text appear bolder
            if (isDarkBackground && alpha > 0 && alpha < 255) {
              // Curve the alpha to make anti-aliased edges more opaque
              alpha = std::min(255,
                          (alpha * alpha) / 180); // Adjust the divisor to taste
            }

            if (alpha == 255) {
              renderBuffer[bufferOffset + dx] = blendTable[255];
            } else {
              renderBuffer[bufferOffset + dx] = blendTable[alpha];
            }
          }
        }
      }
    }
  }
  // Measure text width without rendering
  int measureText(const char *text, const FastFont &font) {
    int width = 0;

    while (*text) {
      char c = *text++;

      if (c < font.firstChar || c > font.lastChar) {
        continue;
      }

      const FastGlyph &glyph = font.glyphs[c - font.firstChar];
      width += glyph.xAdvance;

      // Apply kerning
      if (font.hasKerning && *text) {
        width += getKerning(font, c, *text);
      }
    }

    return width;
  }

  // Render with alignment
  void renderTextAligned(const char *text, int x, int y, int maxWidth,
                         const FastFont &font, uint16_t color, uint16_t bgColor,
                         int align = 0) { // 0=left, 1=center, 2=right
    if (align == 0) {
      renderText(text, x, y, font, color, bgColor);
      return;
    }

    int textWidth = measureText(text, font);

    if (align == 1) { // Center
      x += (maxWidth - textWidth) / 2;
    } else if (align == 2) { // Right
      x += maxWidth - textWidth;
    }

    renderText(text, x, y, font, color, bgColor);
  }

  void setBuffer(uint16_t *buffer, int width, int height) {
    renderBuffer = buffer;
    bufferWidth = width;
    bufferHeight = height;
  }

private:
  int8_t getKerning(const FastFont &font, char c1, char c2) {
    if (!font.hasKerning || !font.kerningTable)
      return 0;

    // Simple kerning table lookup
    // You'd implement the actual lookup based on your kerning format
    return 0;
  }

  bool isColorDark(uint16_t color) {
    // Extract RGB and calculate luminance
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    // Simple luminance calculation (you could use a more sophisticated one)
    int luminance = r * 2 + g + b; // Rough approximation
    return luminance < 40;         // Adjust threshold as needed
  }
};

// Font data would be generated by a conversion tool
// Example structure for IBM Plex Sans 16pt
extern const FastFont IBMPlexSans16;
extern const FastFont IBMPlexSans16Bold;
extern const FastFont IBMPlexSans12;
