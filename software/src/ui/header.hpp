#pragma once
#include "ui_types.h"
#include "theme.h"
#include "rendering/font_renderer.h"
#include "fonts/IBMPlexSans16.h"
#include <Arduino.h>

class ILI9341_GFX;

class HeaderComponent {
private:
    FastFontRenderer* renderer;
    bool isDirty = true;
    
public:
    HeaderComponent();
    ~HeaderComponent();
    
    // Standard rendering interface - uses global buffers
    void renderRow(int y, const char* title = "OpenPod");
    void renderColumn(int x, const char* title = "OpenPod");
    
    // Utility functions
    void markDirty() { isDirty = true; }
    bool needsUpdate() const { return isDirty; }
    
    // Battery indicator rendering
    void renderBatteryToBuffer(uint16_t* buffer, int bufferWidth, const char* title);
    
    // Direct display output (for full screen renders)
    void pushToDisplay(ILI9341_GFX* display, const char* title = "OpenPod");
};

// Implementation
HeaderComponent::HeaderComponent() {
    // Use a temporary buffer for font renderer initialization
    uint16_t* tempBuffer = g_buffers.getRowBuffer();
    renderer = new FastFontRenderer(tempBuffer, SCREEN_WIDTH, HEADER_HEIGHT);
}

HeaderComponent::~HeaderComponent() {
    delete renderer;
}

void HeaderComponent::renderRow(int y, const char* title) {
    uint16_t* rowBuffer = g_buffers.getRowBuffer();
    g_buffers.clearRowBuffer(COLOR_PRIMARY);
    
    if (y < 0 || y >= HEADER_HEIGHT) {
        // Outside header area
        g_buffers.clearRowBuffer(COLOR_BACKGROUND);
        return;
    }
    
    // Render title text if in text area
    if (y >= 10 && y <= 25 && title) {
        renderer = new FastFontRenderer(rowBuffer, SCREEN_WIDTH, 1);
        renderer->renderTextScanline(title, 10, y - 10, IBMPlexSans16, 
                                   COLOR_TEXT, COLOR_PRIMARY, rowBuffer);
        delete renderer;
    }
    
    // Render battery indicator
    renderBatteryToBuffer(rowBuffer, SCREEN_WIDTH, title);
    
    // Add separator line at bottom of header
    if (y == HEADER_HEIGHT - 1) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            rowBuffer[x] = COLOR_SECONDARY;
        }
    }
}

void HeaderComponent::renderColumn(int x, const char* title) {
    uint16_t* columnBuffer = g_buffers.getColumnBuffer();
    g_buffers.clearColumnBuffer(COLOR_BACKGROUND);
    
    if (x < 0 || x >= SCREEN_WIDTH) return;
    
    // Header area (0 to HEADER_HEIGHT)
    for (int y = 0; y < HEADER_HEIGHT; y++) {
        columnBuffer[y] = COLOR_PRIMARY;
    }
    
    // Title text column rendering
    if (x >= 10 && x < SCREEN_WIDTH - 50 && title) {
        FastFontRenderer tempRenderer(columnBuffer, 1, SCREEN_HEIGHT);
        tempRenderer.renderTextColumn(title, 10, 10, IBMPlexSans16, 
                                    COLOR_TEXT, COLOR_PRIMARY, columnBuffer);
    }
    
    // Battery indicator column
    int battX = 285;
    int battY = 8;
    int battW = 25;
    int battH = 12;
    
    if (x >= battX && x < battX + battW + 1) {
        // Battery outline
        for (int y = battY; y < battY + battH; y++) {
            if (y == battY || y == battY + battH - 1 || 
                x == battX || x == battX + battW - 1) {
                columnBuffer[y] = COLOR_TEXT;
            }
        }
        
        // Battery terminal
        if (x == battX + battW) {
            for (int y = battY + 3; y < battY + 9; y++) {
                columnBuffer[y] = COLOR_TEXT;
            }
        }
        
        // Battery fill
        if (x >= battX + 2 && x < battX + 18) {
            for (int y = battY + 2; y < battY + battH - 2; y++) {
                columnBuffer[y] = COLOR_HIGHLIGHT;
            }
        }
    }
    
    // Separator line
    columnBuffer[HEADER_HEIGHT] = COLOR_SECONDARY;
}

void HeaderComponent::renderBatteryToBuffer(uint16_t* buffer, int bufferWidth, const char* title) {
    int battX = 285;
    int battY = 8;
    int battW = 25;
    int battH = 12;
    
    // Only render if we're in the battery Y range
    // (This function is called for single row rendering)
    // Battery rendering will be handled by renderRow/renderColumn appropriately
}

void HeaderComponent::pushToDisplay(ILI9341_GFX* display, const char* title) {
    // Render header using row-by-row method
    for (int y = 0; y < HEADER_HEIGHT + 1; y++) { // +1 for separator line
        renderRow(y, title);
        uint16_t* rowBuffer = g_buffers.getRowBuffer();
        
        if (y < HEADER_HEIGHT) {
            display->setWindow(0, y, SCREEN_WIDTH - 1, y);
            display->pushPixels(rowBuffer, SCREEN_WIDTH);
        } else {
            // Draw separator line
            display->drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, COLOR_SECONDARY);
        }
    }
}