#pragma once
#include "ui_types.h"
#include "theme.h"
#include "rendering/font_renderer.h"
#include "fonts/IBMPlexSans16.h"
#include <Arduino.h>

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
    fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
    fontRenderer.renderText(headerTitle, 10, 10, IBMPlexSans16, COLOR_TEXT, COLOR_PRIMARY);
    renderBatteryToBuffer(buffer, SCREEN_WIDTH, headerTitle);
    display->setWindow(0, 0, SCREEN_WIDTH - 1, HEADER_HEIGHT - 1);
    display->pushPixels(buffer, SCREEN_WIDTH * HEADER_HEIGHT);
    display->drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, COLOR_SECONDARY);
    isDirty = false;
}