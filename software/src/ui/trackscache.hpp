#pragma once
#include "../fonts/IBMPlexSans16Bold.h"
#include "theme.h"
#include "trackrenderer.hpp"
#include "ui_types.h"
#include <Arduino.h>

class TracksCache {
private:
  static const int CACHE_WIDTH = SCREEN_WIDTH - SCROLLBAR_WIDTH;
  static const int BITS_PER_TRACK = CACHE_WIDTH * TRACK_HEIGHT;
  static const int BYTES_PER_TRACK =
      (BITS_PER_TRACK + 7) / 8; // Round up to bytes
  // Binary bitmaps for all tracks (1 = text, 0 = background) - PACKED!
  uint8_t trackBitmaps[TRACKS_PER_SCREEN][BYTES_PER_TRACK];

  // Pre-computed gradient colors for selected track
  uint16_t gradientColors[TRACK_HEIGHT];

  bool cacheBuilt = false;
  TrackRenderer renderer; // Declare as a value, not a pointer

public:
  TracksCache() : renderer(IBMPlexSans16Bold) { buildGradient(); }

  // Build binary cache for all tracks (call once at startup)
  void buildCache(const char **tracks) {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      renderer.renderTrackBinary(tracks[i], trackBitmaps[i], CACHE_WIDTH,
                                 TRACK_HEIGHT, 0, CACHE_WIDTH);
    }

    cacheBuilt = true;
  }

  // Render single track to buffer using packed bits
  void renderTrackToBuffer(int trackIndex, bool isSelected, uint16_t *buffer,
                           int bufferWidth) {
    if (!cacheBuilt || trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN) {
      fillBufferWithBackground(buffer, bufferWidth);
      return;
    }

    // Colors
    uint16_t normalBg = COLOR_BACKGROUND;
    uint16_t normalText = COLOR_TEXT;
    uint16_t selectedText = COLOR_BACKGROUND;

    for (int y = 0; y < TRACK_HEIGHT; y++) {
      for (int x = 0; x < bufferWidth && x < CACHE_WIDTH; x++) {
        bool isTextPixel = getBitPacked(trackBitmaps[trackIndex], x, y);

        if (isSelected) {
          uint16_t gradientBg = gradientColors[y];
          buffer[y * bufferWidth + x] = isTextPixel ? selectedText : gradientBg;
        } else {
          buffer[y * bufferWidth + x] = isTextPixel ? normalText : normalBg;
        }
      }
    }
  }

  // Fill buffer with background color
  void fillBufferWithBackground(uint16_t *buffer, int bufferWidth) {
    for (int i = 0; i < bufferWidth * TRACK_HEIGHT; i++) {
      buffer[i] = COLOR_BACKGROUND;
    }
  }

  // Check if cache is built
  bool isReady() const { return cacheBuilt; }

private:
  // BITWISE OPERATIONS - 8 pixels per byte!
  bool getBitPacked(uint8_t *bitmap, int x, int y) {
    if (x >= CACHE_WIDTH || y >= TRACK_HEIGHT)
      return false;

    int bitIndex = y * CACHE_WIDTH + x;
    int byteIndex = bitIndex / 8; // Which byte
    int bitOffset = bitIndex % 8; // Which bit in that byte

    return (bitmap[byteIndex] & (1 << bitOffset)) != 0;
  }

  void setBitPacked(uint8_t *bitmap, int x, int y, bool value) {
    if (x >= CACHE_WIDTH || y >= TRACK_HEIGHT)
      return;

    int bitIndex = y * CACHE_WIDTH + x;
    int byteIndex = bitIndex / 8;
    int bitOffset = bitIndex % 8;

    if (value) {
      bitmap[byteIndex] |= (1 << bitOffset); // Set bit
    } else {
      bitmap[byteIndex] &= ~(1 << bitOffset); // Clear bit
    }
  }

  // Pre-compute gradient colors
  void buildGradient() {
    uint16_t baseColor = COLOR_PRIMARY;

    for (int y = 0; y < TRACK_HEIGHT; y++) {
      gradientColors[y] = getGradientColor(baseColor, y, TRACK_HEIGHT);
    }
  }

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
