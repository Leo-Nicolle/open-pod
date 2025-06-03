#pragma once
#include "ui_types.h"
#include "theme.h"
#include "rendering/font_renderer.h"
#include "fonts/IBMPlexSans16.h"
#include <Arduino.h>

class ILI9341_GFX;

class NowPlayingComponent {
private:
    const char* currentTrack;
    float progress; // 0.0 to 1.0
    bool isPlaying;
    
    // Layout constants
    static const int ALBUM_ART_X = 100;
    static const int ALBUM_ART_Y = 50;
    static const int ALBUM_ART_SIZE = 120;
    static const int TITLE_Y = 185;
    static const int PROGRESS_BAR_Y = 220;
    static const int PROGRESS_BAR_HEIGHT = 8;
    static const int PROGRESS_BAR_WIDTH = 300;
    
public:
    NowPlayingComponent();
    
    // Configuration
    void setTrack(const char* trackName);
    void setProgress(float progressPercent);
    void setPlayState(bool playing);
    
    // Standard rendering interface - uses global buffers
    void renderRow(int y);
    void renderColumn(int x);
    
    // Legacy full rendering (for complete redraws)
    void renderFull(ILI9341_GFX* display);
    
    // Utility functions
    bool isInAlbumArtArea(int x, int y) const;
    bool isInTitleArea(int x, int y) const;
    bool isInProgressBarArea(int x, int y) const;
};

// Implementation
NowPlayingComponent::NowPlayingComponent() 
    : currentTrack(""), progress(0.0f), isPlaying(false) {
}

void NowPlayingComponent::setTrack(const char* trackName) {
    currentTrack = trackName;
}

void NowPlayingComponent::setProgress(float progressPercent) {
    progress = constrain(progressPercent, 0.0f, 1.0f);
}

void NowPlayingComponent::setPlayState(bool playing) {
    isPlaying = playing;
}

bool NowPlayingComponent::isInAlbumArtArea(int x, int y) const {
    return x >= ALBUM_ART_X && x < ALBUM_ART_X + ALBUM_ART_SIZE &&
           y >= ALBUM_ART_Y && y < ALBUM_ART_Y + ALBUM_ART_SIZE;
}

bool NowPlayingComponent::isInTitleArea(int x, int y) const {
    return y >= TITLE_Y && y < TITLE_Y + 20; // Approximate text height
}

bool NowPlayingComponent::isInProgressBarArea(int x, int y) const {
    return x >= 10 && x < 10 + PROGRESS_BAR_WIDTH &&
           y >= PROGRESS_BAR_Y && y < PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT;
}

void NowPlayingComponent::renderRow(int y) {
    uint16_t* rowBuffer = g_buffers.getRowBuffer();
    
    // Skip header area (handled by HeaderComponent)
    if (y <= HEADER_HEIGHT) return;
    
    // Album art area
    if (y >= ALBUM_ART_Y && y < ALBUM_ART_Y + ALBUM_ART_SIZE) {
        int relativeY = y - ALBUM_ART_Y;
        
        for (int x = ALBUM_ART_X; x < ALBUM_ART_X + ALBUM_ART_SIZE; x++) {
            uint16_t color = COLOR_SECONDARY;
            
            // Border
            if (relativeY == 0 || relativeY == ALBUM_ART_SIZE - 1 ||
                x == ALBUM_ART_X || x == ALBUM_ART_X + ALBUM_ART_SIZE - 1) {
                color = COLOR_ACCENT;
            }
            
            rowBuffer[x] = color;
        }
    }
    
    // Title area
    if (y >= TITLE_Y && y < TITLE_Y + 20 && currentTrack && strlen(currentTrack) > 0) {
        int relativeY = y - TITLE_Y;
        FastFontRenderer tempRenderer(rowBuffer, SCREEN_WIDTH, 1);
        tempRenderer.renderTextScanline(currentTrack, 10, relativeY, IBMPlexSans16, 
                                      COLOR_TEXT, COLOR_BACKGROUND, rowBuffer);
    }
    
    // Progress bar area
    if (y >= PROGRESS_BAR_Y && y < PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT) {
        int relativeY = y - PROGRESS_BAR_Y;
        int fillWidth = (int)(PROGRESS_BAR_WIDTH * progress);
        
        for (int x = 10; x < 10 + PROGRESS_BAR_WIDTH; x++) {
            uint16_t color = COLOR_BACKGROUND;
            
            // Border
            if (relativeY == 0 || relativeY == PROGRESS_BAR_HEIGHT - 1 ||
                x == 10 || x == 10 + PROGRESS_BAR_WIDTH - 1) {
                color = COLOR_ACCENT;
            }
            // Fill
            else if (x < 10 + fillWidth && relativeY > 0 && relativeY < PROGRESS_BAR_HEIGHT - 1) {
                color = COLOR_HIGHLIGHT;
            }
            
            rowBuffer[x] = color;
        }
    }
}

void NowPlayingComponent::renderColumn(int x) {
    uint16_t* columnBuffer = g_buffers.getColumnBuffer();
    
    // Skip header area (handled by HeaderComponent)
    // Initialize non-header area with background
    for (int y = HEADER_HEIGHT + 1; y < SCREEN_HEIGHT; y++) {
        columnBuffer[y] = COLOR_BACKGROUND;
    }
    
    // Album art area
    if (x >= ALBUM_ART_X && x < ALBUM_ART_X + ALBUM_ART_SIZE) {
        int relativeX = x - ALBUM_ART_X;
        
        for (int y = ALBUM_ART_Y; y < ALBUM_ART_Y + ALBUM_ART_SIZE; y++) {
            uint16_t color = COLOR_SECONDARY;
            
            // Border
            if (relativeX == 0 || relativeX == ALBUM_ART_SIZE - 1 ||
                y == ALBUM_ART_Y || y == ALBUM_ART_Y + ALBUM_ART_SIZE - 1) {
                color = COLOR_ACCENT;
            }
            
            columnBuffer[y] = color;
        }
    }
    
    // Title area (all columns potentially have text)
    if (currentTrack && strlen(currentTrack) > 0) {
        FastFontRenderer tempRenderer(columnBuffer, 1, SCREEN_HEIGHT);
        tempRenderer.renderTextColumn(currentTrack, 10, TITLE_Y, IBMPlexSans16, 
                                    COLOR_TEXT, COLOR_BACKGROUND, columnBuffer);
    }
    
    // Progress bar area
    if (x >= 10 && x < 10 + PROGRESS_BAR_WIDTH) {
        int relativeX = x - 10;
        int fillWidth = (int)(PROGRESS_BAR_WIDTH * progress);
        
        for (int y = PROGRESS_BAR_Y; y < PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT; y++) {
            uint16_t color = COLOR_BACKGROUND;
            
            // Border
            if (y == PROGRESS_BAR_Y || y == PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT - 1 ||
                relativeX == 0 || relativeX == PROGRESS_BAR_WIDTH - 1) {
                color = COLOR_ACCENT;
            }
            // Fill
            else if (relativeX < fillWidth && y > PROGRESS_BAR_Y && y < PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT - 1) {
                color = COLOR_HIGHLIGHT;
            }
            
            columnBuffer[y] = color;
        }
    }
}

void NowPlayingComponent::renderFull(ILI9341_GFX* display) {
    // Use row-by-row rendering for full screen update
    for (int y = HEADER_HEIGHT + 1; y < SCREEN_HEIGHT; y++) {
        g_buffers.clearRowBuffer(COLOR_BACKGROUND);
        renderRow(y);
        
        uint16_t* rowBuffer = g_buffers.getRowBuffer();
        display->setWindow(0, y, SCREEN_WIDTH - 1, y);
        display->pushPixels(rowBuffer, SCREEN_WIDTH);
    }
}