#pragma once
#include <stdint.h>
#include <Arduino.h>

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
static const int SCROLLBAR_X = 312;
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
    uint16_t bufferA[SCREEN_WIDTH * 3];  // 3 rows worth of pixels
    uint16_t bufferB[SCREEN_WIDTH * 3];  // 3 rows worth of pixels
    
    uint16_t* currentBuffer;
    uint16_t* backBuffer;
    
    // Single row/column buffers for individual operations
    uint16_t rowBuffer[SCREEN_WIDTH];
    uint16_t columnBuffer[SCREEN_HEIGHT];
    
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
    
    // Get single-line buffers
    uint16_t* getRowBuffer() { return rowBuffer; }
    uint16_t* getColumnBuffer() { return columnBuffer; }
    
    // Get buffer for specific row within the 3-row buffer
    uint16_t* getBufferRow(int rowIndex) {
        if (rowIndex < 0 || rowIndex >= 3) return nullptr;
        return currentBuffer + (rowIndex * SCREEN_WIDTH);
    }
    
    // Clear buffers
    void clearCurrentBuffer(uint16_t color = COLOR_BACKGROUND) {
        for (int i = 0; i < SCREEN_WIDTH * 3; i++) {
            currentBuffer[i] = color;
        }
    }
    
    void clearRowBuffer(uint16_t color = COLOR_BACKGROUND) {
        for (int i = 0; i < SCREEN_WIDTH; i++) {
            rowBuffer[i] = color;
        }
    }
    
    void clearColumnBuffer(uint16_t color = COLOR_BACKGROUND) {
        for (int i = 0; i < SCREEN_HEIGHT; i++) {
            columnBuffer[i] = color;
        }
    }
    
    // Memory usage info
    size_t getTotalMemoryUsage() const {
        return (SCREEN_WIDTH * 3 * 2 + SCREEN_WIDTH + SCREEN_HEIGHT) * sizeof(uint16_t);
    }
    
    void printMemoryInfo() const {
        Serial.print("BufferManager memory usage: ");
        Serial.print(getTotalMemoryUsage());
        Serial.println(" bytes");
        Serial.print("3-row buffers: 2x ");
        Serial.print(SCREEN_WIDTH * 3 * sizeof(uint16_t));
        Serial.println(" bytes");
        Serial.print("Row buffer: ");
        Serial.print(SCREEN_WIDTH * sizeof(uint16_t));
        Serial.println(" bytes");
        Serial.print("Column buffer: ");
        Serial.print(SCREEN_HEIGHT * sizeof(uint16_t));
        Serial.println(" bytes");
    }
};

// Global buffer manager instance
extern BufferManager g_buffers;