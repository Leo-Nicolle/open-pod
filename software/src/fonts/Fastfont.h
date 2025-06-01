#pragma once

#include <Arduino.h>

// Font structure optimized for speed
struct FastGlyph {
  uint8_t width;      // Glyph width in pixels
  uint8_t height;     // Glyph height in pixels
  int8_t xOffset;     // Horizontal offset
  int8_t yOffset;     // Vertical offset from baseline
  uint8_t xAdvance;   // Cursor advance
  const uint8_t* alphaData;  // 8-bit alpha channel data
  const uint16_t* colorData; // Optional: pre-rendered color data for common colors
};

struct FastFont {
  const FastGlyph* glyphs;   // Array of glyphs
  uint8_t firstChar;         // First ASCII character
  uint8_t lastChar;          // Last ASCII character
  uint8_t lineHeight;        // Line height
  uint8_t baseline;          // Baseline offset
  bool hasKerning;           // Kerning table available
  const int8_t* kerningTable; // Optional kerning pairs
};
