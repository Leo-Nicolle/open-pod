#pragma once
#include "ui_types.h"
#include "theme.h"
#include "rendering/font_renderer.h"
#include "fonts/IBMPlexSans16Bold.h"
#include <Arduino.h>
#include <string.h>

class ILI9341_GFX;

class HeaderComponent {
private:
    bool isDirty = true;
    
public:
    HeaderComponent();
    ~HeaderComponent();
    
    // Utility functions
    void markDirty() { isDirty = true; }
    bool needsUpdate() const { return isDirty; }
    
    // Battery indicator rendering
    void renderBatteryToBuffer(uint16_t* buffer, int bufferWidth, const char* title);
    
    // Direct display output (for full screen renders)
    void render(ILI9341_GFX* display, const char* title = "OpenPod");
};

// Implementation
HeaderComponent::HeaderComponent() {
}

HeaderComponent::~HeaderComponent() {
}

void HeaderComponent::renderBatteryToBuffer(uint16_t* buffer, int bufferWidth, const char* title) {
    int battX = 285;
    int battY = 8;
    int battW = 25;
    int battH = 12;
}

void HeaderComponent::render(ILI9341_GFX* display, const char* title) {
    u_int16_t* buffer = g_buffers.getCurrentBuffer();
    for (int i = 0; i < SCREEN_WIDTH * HEADER_HEIGHT; i++) {
        buffer[i] = COLOR_PRIMARY;
    }
    const char *headerTitle = title ? title : "OpenPod";

    // Truncate the title to fit within the header (with an ellipsis)
    char truncatedTitle[64];
    strncpy(truncatedTitle, headerTitle, sizeof(truncatedTitle) - 1);
    truncatedTitle[sizeof(truncatedTitle) - 1] = '\0';

    fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
    const int maxTitleWidth = SCREEN_WIDTH - 10 - 35; // left margin + battery reserve
    if (fontRenderer.measureText(truncatedTitle, IBMPlexSans16Bold) > maxTitleWidth) {
        int len = strlen(truncatedTitle);
        while (len > 1) {
            char test[64];
            snprintf(test, sizeof(test), "%.*s...", len, truncatedTitle);
            if (fontRenderer.measureText(test, IBMPlexSans16Bold) <= maxTitleWidth) {
                break;
            }
            len--;
        }
        snprintf(truncatedTitle, sizeof(truncatedTitle), "%.*s...", len, truncatedTitle);
    }

    fontRenderer.renderText(truncatedTitle, 10, 10, IBMPlexSans16Bold, COLOR_BACKGROUND, COLOR_PRIMARY);
    renderBatteryToBuffer(buffer, SCREEN_WIDTH, truncatedTitle);
    display->setWindow(0, 0, SCREEN_WIDTH - 1, HEADER_HEIGHT - 1);
    display->pushPixels(buffer, SCREEN_WIDTH * HEADER_HEIGHT);
    display->drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, COLOR_SECONDARY);
    isDirty = false;
}