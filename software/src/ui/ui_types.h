#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "ui/theme.h"
#include "rendering/font_renderer.h"

// UI States
enum UIState {
    STATE_TRACK_LIST,
    STATE_NOW_PLAYING,
    STATE_TRANSITIONING
};

// UI Constants
static const int SCREEN_WIDTH = 320;
static const int SCREEN_HEIGHT = 240;
static const int HEADER_HEIGHT = 30;
static const int TRACK_HEIGHT = 30;
static const int TRACKS_PER_SCREEN = 6;
static const int SCROLLBAR_WIDTH = 8;
static const int MARGIN = 5;
static const int TRACK_LIST_Y = HEADER_HEIGHT + MARGIN;

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
    uint16_t bufferA[SCREEN_WIDTH * 30];  // 30 rows worth of pixels
    uint16_t bufferB[SCREEN_WIDTH * 30];  // 30 rows worth of pixels
    
    uint16_t* currentBuffer;
    uint16_t* backBuffer;
    
public:
    BufferManager() {
        currentBuffer = bufferA;
        backBuffer = bufferB;
    }
    
    // Swap the buffers
    void swapBuffers() {
        uint16_t* temp = currentBuffer;
        currentBuffer = backBuffer;
        backBuffer = temp;
    }
    
    // Get buffer pointers
    uint16_t* getCurrentBuffer() { return currentBuffer; }
    uint16_t* getBackBuffer() { return backBuffer; }
    void clearCurrentBuffer(uint16_t color = COLOR_BACKGROUND) {
        for (int i = 0; i < SCREEN_WIDTH * 3; i++) {
            currentBuffer[i] = color;
        }
    }
};

// Global buffer manager instance
extern BufferManager g_buffers;
extern FastFontRenderer fontRenderer;