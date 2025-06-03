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
    
    void render(ILI9341_GFX* display);
    
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



void NowPlayingComponent::render(ILI9341_GFX* display) {
    // Fill background
    // TODO: Just fill the right area instead of the whole screen
    // display->fillRect(COLOR_BACKGROUND);

    // Album art
    display->fillRect(ALBUM_ART_X, ALBUM_ART_Y, ALBUM_ART_SIZE, ALBUM_ART_SIZE, COLOR_SECONDARY);
    display->drawRect(ALBUM_ART_X, ALBUM_ART_Y, ALBUM_ART_SIZE, ALBUM_ART_SIZE, COLOR_ACCENT);

    // Track title
    fontRenderer.setBuffer(nullptr, 0, 0); // Use direct rendering if needed
    fontRenderer.renderText(currentTrack, 10, TITLE_Y, IBMPlexSans16, COLOR_TEXT, COLOR_BACKGROUND);

    // Progress bar
    display->drawRect(10, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, COLOR_ACCENT);
    int filledWidth = static_cast<int>((PROGRESS_BAR_WIDTH - 2) * progress);
    if (filledWidth > 0) {
        display->fillRect(11, PROGRESS_BAR_Y + 1, filledWidth, PROGRESS_BAR_HEIGHT - 2, COLOR_HIGHLIGHT);
    }
}