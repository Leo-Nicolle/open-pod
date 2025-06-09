#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16Bold.h"
#include "menu.hpp"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

class ILI9341_GFX;

class TrackListComponent {
private:
  const char **tracks;
  int totalTracks;
  int selectedTrack;
  int topVisibleTrack;

public:
  MenuItemRenderer *menuRenderer;
  TrackListComponent(const char **trackList, int trackCount);
  ~TrackListComponent();

  // Navigation
  void setSelection(int selected, int topVisible);
  void getSelection(int &selected, int &topVisible) const;
  void renderTrack(int trackIndex, bool isSelected, int x = 0,
                   int width = SCREEN_WIDTH);
  void renderAllTracks(ILI9341_GFX *display, int x = 0, int y = BODY_Y,
                       int width = SCREEN_WIDTH, int tx = -1);

  // Utility functions
  int getTrackAtY(int y) const;
  int getRowInTrack(int y) const;
  bool isTrackVisible(int trackIndex) const;
  const char *getTrackName(int index) const;
};

// Implementation
TrackListComponent::TrackListComponent(const char **trackList, int trackCount)
    : tracks(trackList), totalTracks(trackCount), selectedTrack(0),
      topVisibleTrack(0) {
  menuRenderer = new MenuItemRenderer(IBMPlexSans16Bold, IBMPlexSans12);
}

TrackListComponent::~TrackListComponent() { delete menuRenderer; }

void TrackListComponent::setSelection(int selected, int topVisible) {
  selectedTrack = constrain(selected, 0, totalTracks - 1);
  topVisibleTrack =
      constrain(topVisible, 0, max(0, totalTracks - TRACKS_PER_SCREEN));
}

void TrackListComponent::getSelection(int &selected, int &topVisible) const {
  selected = selectedTrack;
  topVisible = topVisibleTrack;
}

int TrackListComponent::getTrackAtY(int y) const {
  if (y < BODY_Y)
    return -1;
  int relativeY = y - BODY_Y;
  int trackIndex = topVisibleTrack + (relativeY / TRACK_HEIGHT);
  return (trackIndex < totalTracks) ? trackIndex : -1;
}

int TrackListComponent::getRowInTrack(int y) const {
  if (y < BODY_Y)
    return -1;
  return (y - BODY_Y) % TRACK_HEIGHT;
}

bool TrackListComponent::isTrackVisible(int trackIndex) const {
  return trackIndex >= topVisibleTrack &&
         trackIndex < topVisibleTrack + TRACKS_PER_SCREEN;
}

const char *TrackListComponent::getTrackName(int index) const {
  if (index < 0 || index >= totalTracks)
    return "";
  return tracks[index];
}

void TrackListComponent::renderTrack(int trackIndex, bool isSelected, int x,
                                     int width) {
  menuRenderer->renderMenuItem(tracks[trackIndex], isSelected, x, width);
}

void TrackListComponent::renderAllTracks(ILI9341_GFX *display, int x, int y,
                                         int width, int tx) {
  if (tx < 0) {
    tx = x;
  }
  
  for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
    int trackIndex = topVisibleTrack + i;
    
    // Stop if we've run out of tracks
    if (trackIndex >= totalTracks) break;
    
    // Render the track to buffer
    renderTrack(trackIndex, trackIndex == selectedTrack, x, width);
    
    // Calculate Y position based on screen position, not absolute track index
    uint16_t trackY = y + (i * TRACK_HEIGHT);
    
    // Clip the window to screen bounds
    int windowBottom = min(trackY + TRACK_HEIGHT - 1, SCREEN_HEIGHT - 1);
    int actualHeight = windowBottom - trackY + 1;
    
    // Only render if there's visible area
    if (actualHeight > 0 && trackY < SCREEN_HEIGHT) {
      display->setWindow(tx, trackY, tx + width - 1, windowBottom);
      display->pushPixels(g_buffers.getCurrentBuffer(), width * actualHeight);
    }
    g_buffers.swapBuffers();
  }
}