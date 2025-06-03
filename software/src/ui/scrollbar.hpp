#pragma once
#include "ui_types.h"
#include "theme.h"
#include <Arduino.h>

class ILI9341_GFX;

class ScrollbarComponent {
private:
    int totalItems;
    int visibleItems;
    int topVisibleItem;
    int scrollAreaY;
    int scrollAreaHeight;
    
public:
    ScrollbarComponent(int scrollY = TRACK_LIST_Y, int scrollHeight = TRACKS_PER_SCREEN * TRACK_HEIGHT);
    
    // Configuration
    void setScrollData(int total, int visible, int topVisible);
    void setScrollArea(int y, int height);
    
    // Standard rendering interface - uses global buffers
    void renderRow(int y);
    void renderColumn(int x);
    
    // Utility functions
    bool isVisible() const { return totalItems > visibleItems; }
    int getThumbPosition() const;
    int getThumbHeight() const;
    
    // Legacy direct rendering (for full screen updates)
    void render(ILI9341_GFX* display);
};

// Implementation
ScrollbarComponent::ScrollbarComponent(int scrollY, int scrollHeight) 
    : totalItems(0), visibleItems(0), topVisibleItem(0), 
      scrollAreaY(scrollY), scrollAreaHeight(scrollHeight) {
}

void ScrollbarComponent::setScrollData(int total, int visible, int topVisible) {
    totalItems = total;
    visibleItems = visible;
    topVisibleItem = topVisible;
}

void ScrollbarComponent::setScrollArea(int y, int height) {
    scrollAreaY = y;
    scrollAreaHeight = height;
}

int ScrollbarComponent::getThumbHeight() const {
    if (!isVisible()) return 0;
    return max(20, (scrollAreaHeight * visibleItems) / totalItems);
}

int ScrollbarComponent::getThumbPosition() const {
    if (!isVisible()) return scrollAreaY;
    int thumbHeight = getThumbHeight();
    return scrollAreaY + ((scrollAreaHeight - thumbHeight) * topVisibleItem) / 
                        (totalItems - visibleItems);
}

void ScrollbarComponent::renderRow(int y) {
    uint16_t* rowBuffer = g_buffers.getRowBuffer();
    
    // Only modify the scrollbar area (last 8 pixels)
    for (int x = SCROLLBAR_X; x < SCREEN_WIDTH; x++) {
        if (!isVisible()) {
            rowBuffer[x] = COLOR_BACKGROUND;
            continue;
        }
        
        // Check if we're in the scrollable area
        if (y < scrollAreaY || y >= scrollAreaY + scrollAreaHeight) {
            rowBuffer[x] = COLOR_BACKGROUND;
            continue;
        }
        
        int scrollbarX = x - SCROLLBAR_X;
        uint16_t color = COLOR_BACKGROUND;
        
        // Draw track outline
        if (scrollbarX == 0 || scrollbarX == 5 || 
            y == scrollAreaY || y == scrollAreaY + scrollAreaHeight - 1) {
            if (scrollbarX <= 5) color = COLOR_ACCENT;
        }
        
        // Draw thumb
        int thumbPos = getThumbPosition();
        int thumbHeight = getThumbHeight();
        if (y >= thumbPos && y < thumbPos + thumbHeight && 
            scrollbarX >= 1 && scrollbarX <= 4) {
            color = COLOR_SECONDARY;
        }
        
        rowBuffer[x] = color;
    }
}

void ScrollbarComponent::renderColumn(int x) {
    uint16_t* columnBuffer = g_buffers.getColumnBuffer();
    
    // Only handle scrollbar columns
    if (x < SCROLLBAR_X || x >= SCREEN_WIDTH) {
        return; // Don't modify buffer if not in scrollbar area
    }
    
    int scrollbarX = x - SCROLLBAR_X;
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        if (!isVisible()) {
            columnBuffer[y] = COLOR_BACKGROUND;
            continue;
        }
        
        // Check if we're in the scrollable area
        if (y < scrollAreaY || y >= scrollAreaY + scrollAreaHeight) {
            columnBuffer[y] = COLOR_BACKGROUND;
            continue;
        }
        
        uint16_t color = COLOR_BACKGROUND;
        
        // Draw track outline
        if (scrollbarX == 0 || scrollbarX == 5 || 
            y == scrollAreaY || y == scrollAreaY + scrollAreaHeight - 1) {
            if (scrollbarX <= 5) color = COLOR_ACCENT;
        }
        
        // Draw thumb
        int thumbPos = getThumbPosition();
        int thumbHeight = getThumbHeight();
        if (y >= thumbPos && y < thumbPos + thumbHeight && 
            scrollbarX >= 1 && scrollbarX <= 4) {
            color = COLOR_SECONDARY;
        }
        
        columnBuffer[y] = color;
    }
}

void ScrollbarComponent::render(ILI9341_GFX* display) {
    if (!isVisible()) return;
    
    // Use row-by-row rendering for full update
    for (int y = scrollAreaY; y < scrollAreaY + scrollAreaHeight; y++) {
        renderRow(y);
        uint16_t* rowBuffer = g_buffers.getRowBuffer();
        
        // Only push the scrollbar portion
        display->setWindow(SCROLLBAR_X, y, SCREEN_WIDTH - 1, y);
        display->pushPixels(&rowBuffer[SCROLLBAR_X], SCROLLBAR_WIDTH);
    }
}