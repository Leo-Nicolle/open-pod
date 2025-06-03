#pragma once
#include "ui_types.h"
#include "theme.h"
#include "menu.hpp"
#include "fonts/IBMPlexSans12.h"
#include "fonts/IBMPlexSans16.h"
#include <Arduino.h>

class ILI9341_GFX;

class TrackListComponent {
private:
    const char** tracks;
    int totalTracks;
    int selectedTrack;
    int topVisibleTrack;
    
    MenuItemRenderer* menuRenderer;
    
public:
    TrackListComponent(const char** trackList, int trackCount);
    ~TrackListComponent();
    
    // Navigation
    void setSelection(int selected, int topVisible);
    void getSelection(int& selected, int& topVisible) const;
    
    // Standard rendering interface - uses global buffers
    void renderRow(int y);
    void renderColumn(int x);
    
    // Legacy full rendering (for complete redraws)
    void renderAllTracks(ILI9341_GFX* display);
    
    // Utility functions
    int getTrackAtY(int y) const;
    int getRowInTrack(int y) const;
    bool isTrackVisible(int trackIndex) const;
    const char* getTrackName(int index) const;
};

// Implementation
TrackListComponent::TrackListComponent(const char** trackList, int trackCount) 
    : tracks(trackList), totalTracks(trackCount), selectedTrack(0), topVisibleTrack(0) {
    menuRenderer = new MenuItemRenderer(IBMPlexSans16, IBMPlexSans12);
}

TrackListComponent::~TrackListComponent() {
    delete menuRenderer;
}

void TrackListComponent::setSelection(int selected, int topVisible) {
    selectedTrack = constrain(selected, 0, totalTracks - 1);
    topVisibleTrack = constrain(topVisible, 0, max(0, totalTracks - TRACKS_PER_SCREEN));
}

void TrackListComponent::getSelection(int& selected, int& topVisible) const {
    selected = selectedTrack;
    topVisible = topVisibleTrack;
}

int TrackListComponent::getTrackAtY(int y) const {
    if (y < TRACK_LIST_Y) return -1;
    int relativeY = y - TRACK_LIST_Y;
    int trackIndex = topVisibleTrack + (relativeY / TRACK_HEIGHT);
    return (trackIndex < totalTracks) ? trackIndex : -1;
}

int TrackListComponent::getRowInTrack(int y) const {
    if (y < TRACK_LIST_Y) return -1;
    return (y - TRACK_LIST_Y) % TRACK_HEIGHT;
}

bool TrackListComponent::isTrackVisible(int trackIndex) const {
    return trackIndex >= topVisibleTrack && 
           trackIndex < topVisibleTrack + TRACKS_PER_SCREEN;
}

const char* TrackListComponent::getTrackName(int index) const {
    if (index < 0 || index >= totalTracks) return "";
    return tracks[index];
}

void TrackListComponent::renderRow(int y) {
    // Clear the row buffer first
    g_buffers.clearRowBuffer(COLOR_BACKGROUND);
    
    // Check if we're in the track list area
    // if (y < TRACK_LIST_Y || y >= TRACK_LIST_Y + TRACKS_PER_SCREEN * TRACK_HEIGHT) {
    //     return; // Outside track list area
    // }
    
    int trackIndex = getTrackAtY(y);
    // if (trackIndex < 0 || trackIndex >= totalTracks) {
    //     return; // Invalid track
    // }
    
    int rowInTrack = getRowInTrack(y);
    bool isSelected = (trackIndex == selectedTrack);
    
    // Use menu renderer to render this row
    if (menuRenderer && tracks[trackIndex]) {
        menuRenderer->renderRow(trackIndex + 1, tracks[trackIndex], isSelected, rowInTrack);
    }
}

void TrackListComponent::renderColumn(int x) {
    // Clear the column buffer first
    g_buffers.clearColumnBuffer(COLOR_BACKGROUND);
    
    // Only handle columns in track list area (not scrollbar)
    if (x >= SCROLLBAR_X) return;
    
    uint16_t* columnBuffer = g_buffers.getColumnBuffer();
    
    // Render each track that's visible
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
        int trackIndex = topVisibleTrack + i;
        if (trackIndex >= totalTracks) break;
        
        int trackStartY = TRACK_LIST_Y + (i * TRACK_HEIGHT);
        bool isSelected = (trackIndex == selectedTrack);
        
        // Create a temporary buffer for this track's column
        uint16_t trackColumnBuffer[TRACK_HEIGHT];
        
        if (menuRenderer && tracks[trackIndex]) {
            // Temporarily set the column buffer for menu renderer
            uint16_t* originalBuffer = g_buffers.getColumnBuffer();
            
            // Use track column buffer
            for (int j = 0; j < TRACK_HEIGHT; j++) {
                trackColumnBuffer[j] = COLOR_BACKGROUND;
            }
            
            // We need to simulate the column rendering
            // This is a simplified approach - in practice you'd want to optimize this
            menuRenderer->renderColumn(trackIndex + 1, tracks[trackIndex], isSelected, x);
            
            // Copy the rendered track column to the main column buffer
            uint16_t* mainColumnBuffer = g_buffers.getColumnBuffer();
            for (int j = 0; j < TRACK_HEIGHT; j++) {
                int screenY = trackStartY + j;
                if (screenY >= 0 && screenY < SCREEN_HEIGHT) {
                    columnBuffer[screenY] = mainColumnBuffer[j];
                }
            }
        }
    }
}

void TrackListComponent::renderAllTracks(ILI9341_GFX* display) {
    // Use row-by-row rendering for full update
    for (int y = TRACK_LIST_Y; y < TRACK_LIST_Y + TRACKS_PER_SCREEN * TRACK_HEIGHT; y++) {
        renderRow(y);
        uint16_t* rowBuffer = g_buffers.getRowBuffer();
        
        // Push only the track list portion (excluding scrollbar)
        display->setWindow(0, y, SCROLLBAR_X - 1, y);
        display->pushPixels(rowBuffer, SCROLLBAR_X);
    }
}