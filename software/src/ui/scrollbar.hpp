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


void ScrollbarComponent::render(ILI9341_GFX* display) {
    if (!isVisible())
        return;

    int scrollBarX = SCROLLBAR_X;
    int scrollBarY = scrollAreaY;
    int scrollBarHeight = scrollAreaHeight;

    display->fillRect(scrollBarX, scrollBarY, 8, scrollBarHeight, COLOR_BACKGROUND);
    display->drawRect(scrollBarX, scrollBarY, 6, scrollBarHeight, COLOR_ACCENT);

    int thumbHeight = max(20, (scrollBarHeight * visibleItems) / totalItems);
    int thumbPos = scrollBarY + ((scrollBarHeight - thumbHeight) * topVisibleItem) /
                              (totalItems - visibleItems);

    display->fillRect(scrollBarX + 1, thumbPos, 4, thumbHeight, COLOR_SECONDARY);
}