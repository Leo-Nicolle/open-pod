#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
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

  MenuItemRenderer *menuRenderer;

public:
  TrackListComponent(const char **trackList, int trackCount);
  ~TrackListComponent();

  // Navigation
  void setSelection(int selected, int topVisible);
  void getSelection(int &selected, int &topVisible) const;
  void renderTrack(int trackIndex, bool isSelected);
  void renderAllTracks(ILI9341_GFX *display, int x = 0, int y = BODY_Y,
                       int width = SCREEN_WIDTH - 1);

  // Chunked rendering method matching NowPlayingComponent signature
  void renderChunk(ILI9341_GFX *display, int x, int y, int width = SCREEN_WIDTH,
                   int tx = -1, int ty = -1);

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

void TrackListComponent::renderTrack(int trackIndex, bool isSelected) {
  fontRenderer.setBuffer(g_buffers.getCurrentBuffer(), SCREEN_WIDTH,
                         CHUNK_HEIGHT);
  menuRenderer->renderMenuItem(trackIndex + 1, tracks[trackIndex], isSelected);
}

void TrackListComponent::renderAllTracks(ILI9341_GFX *display, int x, int y,
                                         int width) {
  Serial.println("Rendering all tracks" + String(x));
  for (int i = topVisibleTrack; i < topVisibleTrack + TRACKS_PER_SCREEN; i++) {
    renderTrack(i, i == selectedTrack);
    uint16_t y = i * TRACK_HEIGHT + BODY_Y;
    display->setWindow(x, y, x + width, y + TRACK_HEIGHT - 1);
    if (x > 0 || width < SCREEN_WIDTH - 1) {
      g_buffers.crop(x, width + 1);
    }
    display->pushPixels(g_buffers.getCurrentBuffer(), width * TRACK_HEIGHT);
    g_buffers.swapBuffers();
  }
}