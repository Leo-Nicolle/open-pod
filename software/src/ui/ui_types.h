#pragma once
#include "rendering/font_renderer.h"
#include "ui/theme.h"
#include <Arduino.h>
#include <stdint.h>

// UI States
enum UIState { STATE_TRACK_LIST, STATE_NOW_PLAYING, STATE_TRANSITIONING };

// UI Constants
/** @brief Screen width (in landscape) */
static const int SCREEN_WIDTH = 320;
/** @brief Screen height (in landscape) */
static const int SCREEN_HEIGHT = 240;
/** @brief Height of the header */
static const int HEADER_HEIGHT = 30;
/** The height of a track (not the actual font size, which is smaller) */
static const int TRACK_HEIGHT = 30;
/** @brief How many tracks are visible on the screen */
static const int TRACKS_PER_SCREEN = 7;
/** The width of the scrollbar in the lists */
static const int SCROLLBAR_WIDTH = 8;
/** @brief Margin between header and body (always BG color, never redrawn) */
static const int MARGIN = 5;
/** @brief The first body pixel Y coordinate */
static const int BODY_Y = HEADER_HEIGHT + MARGIN;
/** @brief The height of the body */
static const int BODY_HEIGHT = SCREEN_HEIGHT - BODY_Y;

/** The height of the render buffer when landscape mode */
static const int CHUNK_HEIGHT =
    30; // Height of each chunk in the now playing screen

// Coordinate transformation helper
struct Coords {
  int16_t x, y, w, h;

  void transform(bool isRotated) {
    if (isRotated) {
      int16_t newX = SCREEN_HEIGHT - 1 - y - h + 1;
      int16_t newY = x;
      int16_t newW = h;
      int16_t newH = w;
      x = newX;
      y = newY;
      w = newW;
      h = newH;
    }
  }
};

// Forward declaration for theme colors
#ifndef COLOR_BACKGROUND
#define COLOR_BACKGROUND 0x0000
#endif

// Centralized buffer management for efficient rendering
class BufferManager {
private:
  // Two swap buffers for double-buffering
  uint16_t bufferA[SCREEN_WIDTH * CHUNK_HEIGHT]; // 30 rows worth of pixels
  uint16_t bufferB[SCREEN_WIDTH * CHUNK_HEIGHT]; // 30 rows worth of pixels

  uint16_t *currentBuffer;
  uint16_t *backBuffer;

public:
  BufferManager() {
    currentBuffer = bufferA;
    backBuffer = bufferB;
  }

  // Swap the buffers
  void swapBuffers() {
    uint16_t *temp = currentBuffer;
    currentBuffer = backBuffer;
    backBuffer = temp;
  }

  // Get buffer pointers
  uint16_t *getCurrentBuffer() { return currentBuffer; }
  uint16_t *getBackBuffer() { return backBuffer; }

  void crop(int xstart, int w) {
    // Crop the current buffer to the specified rectangle
    uint16_t *buf = currentBuffer;
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
      for (int x = 0; x < w; x++) {
        int sindex = y * SCREEN_WIDTH + xstart+ x;
        int tindex = y * w + x;
        buf[tindex] = buf[sindex];
      }
    }
  }
  void clearCurrentBuffer(uint16_t color = COLOR_BACKGROUND) {
    for (int i = 0; i < SCREEN_WIDTH * CHUNK_HEIGHT; i++) {
      currentBuffer[i] = color;
    }
  }
};

// Global buffer manager instance
extern BufferManager g_buffers;
extern FontRenderer fontRenderer;