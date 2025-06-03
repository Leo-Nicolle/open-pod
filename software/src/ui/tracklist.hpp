#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../theme.h"
#include "menu.hpp"
#include "ui_types.h"
#include <Arduino.h>

class ILI9341_GFX;

class TrackListComponent {
private:
  const char **tracks;
  int totalTracks;
  int selectedTrack;
  int topVisibleTrack;

  MenuItemRenderer *menuRenderer;

public:
  TrackListComponent(const char **trackList, int trackCount);
  ~TrackListComponent();

  // Navigation
  void setSelection(int selected, int topVisible);
  void getSelection(int &selected, int &topVisible) const;
  void renderTrack(int trackIndex, bool isSelected);
  void renderAllTracks(ILI9341_GFX *display);
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
  menuRenderer = new MenuItemRenderer(IBMPlexSans16, IBMPlexSans12);
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
  if (y < TRACK_LIST_Y)
    return -1;
  int relativeY = y - TRACK_LIST_Y;
  int trackIndex = topVisibleTrack + (relativeY / TRACK_HEIGHT);
  return (trackIndex < totalTracks) ? trackIndex : -1;
}

int TrackListComponent::getRowInTrack(int y) const {
  if (y < TRACK_LIST_Y)
    return -1;
  return (y - TRACK_LIST_Y) % TRACK_HEIGHT;
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

void TrackListComponent::renderTrack(int trackIndex, bool isSelected) {
  uint16_t* buffer = g_buffers.getCurrentBuffer();
  uint16_t bgColor = isSelected ? COLOR_HIGHLIGHT : COLOR_BACKGROUND;
  uint16_t textColor = isSelected ? COLOR_TEXT : COLOR_TEXT;

  menuRenderer->renderMenuItem(trackIndex + 1, tracks[trackIndex], isSelected,
                               previousSelected);
}
void TrackListComponent::renderAllTracks(ILI9341_GFX *display) {
  // Use row-by-row rendering for full update
  for (int y = TRACK_LIST_Y;
       y < TRACK_LIST_Y + TRACKS_PER_SCREEN * TRACK_HEIGHT; y++) {
    renderRow(y);
    uint16_t *rowBuffer = g_buffers.getRowBuffer();

    // Push only the track list portion (excluding scrollbar)
    display->setWindow(0, y, SCROLLBAR_X - 1, y);
    display->pushPixels(rowBuffer, SCROLLBAR_X);
  }
}