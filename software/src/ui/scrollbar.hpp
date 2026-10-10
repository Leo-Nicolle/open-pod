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
    bool wide; // fast scroll: 7px thumb instead of 4px
    
public:
    ScrollbarComponent(int scrollY = BODY_Y, int scrollHeight = ELEMENTS_PER_SCREEN * ELEMENT_HEIGHT);
    
    // Configuration
    void setScrollData(int total, int visible, int topVisible);
    void setScrollArea(int y, int height);
    void setWide(bool w) { wide = w; }
    
    // Standard rendering interface - uses global buffers
    // Utility functions
    bool isVisible() const { return totalItems > visibleItems; }
    int getThumbPosition() const;
    int getThumbHeight() const;
    
    // Legacy direct rendering (for full screen updates)
    void render(ILI9341_GFX* display, int x = SCROLLBAR_X);
};

// Implementation
ScrollbarComponent::ScrollbarComponent(int scrollY, int scrollHeight) 
    : totalItems(0), visibleItems(0), topVisibleItem(0), 
      scrollAreaY(scrollY), scrollAreaHeight(scrollHeight), wide(false) {
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


void ScrollbarComponent::render(ILI9341_GFX* display, int x) {
    int scrollBarX = x;
    int scrollBarY = scrollAreaY;
    int scrollBarHeight = scrollAreaHeight;

    // The scrollbar owns its column: clear it even when hidden, so nothing
    // left there (e.g. by a transition) survives on a short list.
    display->fillRect(scrollBarX, scrollBarY, SCREEN_WIDTH - scrollBarX,
                      scrollBarHeight, COLOR_BG);
    if (!isVisible())
        return;
    display->drawRect(scrollBarX, scrollBarY, 6, scrollBarHeight, COLOR_LINE);

    int thumbHeight = max(20, (scrollBarHeight * visibleItems) / totalItems);
    int thumbPos = scrollBarY + ((scrollBarHeight - thumbHeight) * topVisibleItem) /
                              (totalItems - visibleItems);

    if (wide)
        display->fillRect(scrollBarX, thumbPos, 7, thumbHeight, COLOR_ACCENT);
    else
        display->fillRect(scrollBarX + 1, thumbPos, 4, thumbHeight, COLOR_DIM);
}