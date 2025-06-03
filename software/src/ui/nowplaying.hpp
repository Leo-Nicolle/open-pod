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



void NowPlayingComponent::renderFull(ILI9341_GFX* display) {
}