#pragma once
#include "theme.h"
#include "trackscache.hpp"
#include "ui_types.h"
#include <Arduino.h>

class ILI9341_GFX;

class TrackListComponent {
private:
  const char **tracks;
  int totalTracks;
  int selectedTrack;
  int topVisibleTrack;

  // Binary cache system
  TracksCache tracksCache;

public:
  TrackListComponent(const char **trackList, int trackCount);
  ~TrackListComponent();

  // Cache management
  void enableBinaryCache(bool enable = true);
  void begin();

  // Navigation
  void setSelection(int selected, int topVisible);
  void getSelection(int &selected, int &topVisible) const;

  // Rendering methods
  void renderTrack(int trackIndex, bool isSelected, int x = 0,
                   int width = SCREEN_WIDTH);
  void renderAllTracks(ILI9341_GFX *display, int x = 0, int y = BODY_Y,
                       int width = SCREEN_WIDTH, int tx = -1);
  void scrollDown();

  // Utility functions
  int getTrackAtY(int y) const;
  int getRowInTrack(int y) const;
  bool isTrackVisible(int trackIndex) const;
  const char *getTrackName(int index) const;
};

// Implementation
TrackListComponent::TrackListComponent(const char **trackList, int trackCount)
    : tracks(trackList), tracksCache(), totalTracks(trackCount), selectedTrack(0),
      topVisibleTrack(0){}

TrackListComponent::~TrackListComponent() {
}

void TrackListComponent::begin() {
    tracksCache.buildCache(tracks);
}

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
  if (trackIndex < 0 || trackIndex >= totalTracks)
    return;
  tracksCache.renderTrackToBuffer(trackIndex, isSelected,
                                   g_buffers.getCurrentBuffer(), width);
}

// Optimized rendering using chunk-based approach with cache
void TrackListComponent::renderAllTracks(ILI9341_GFX *display, int x, int y,
                                         int width, int tx) {
  if (tx < 0)
    tx = x;

  for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
    int trackIndex = topVisibleTrack + i;

    // Stop if we've run out of tracks
    if (trackIndex >= totalTracks)
      break;

    bool isSelected = (trackIndex == selectedTrack);

    // Render track using cache - this fills the current g_buffer
    tracksCache.renderTrackToBuffer(trackIndex, isSelected,
                                     g_buffers.getCurrentBuffer(), width);

    // Calculate Y position and render in chunks (30 pixel high chunks)
    uint16_t trackY = y + (i * TRACK_HEIGHT);

    // Render track in chunks to handle 40px track height with 30px chunks
    int rowsRendered = 0;
    while (rowsRendered < TRACK_HEIGHT) {
      int chunkHeight = min(CHUNK_HEIGHT, TRACK_HEIGHT - rowsRendered);
      int chunkY = trackY + rowsRendered;

      // Skip the chunk offset since we already have the right data in buffer
      // Just copy the right portion from our rendered track
      if (rowsRendered > 0) {
        // Copy remaining rows from the track buffer
        uint16_t *sourceBuffer = g_buffers.getCurrentBuffer();
        uint16_t *destBuffer = g_buffers.getCurrentBuffer();

        for (int row = 0; row < chunkHeight; row++) {
          int sourceRow = rowsRendered + row;
          int destRow = row;
          for (int col = 0; col < width; col++) {
            destBuffer[destRow * width + col] =
                sourceBuffer[sourceRow * width + col];
          }
        }
      }

      // Clip to screen bounds
      int windowBottom = min(chunkY + chunkHeight - 1, SCREEN_HEIGHT - 1);
      int actualHeight = windowBottom - chunkY + 1;

      if (actualHeight > 0 && chunkY < SCREEN_HEIGHT) {
        display->setWindow(tx, chunkY, tx + width - 1, windowBottom);
        display->pushPixels(g_buffers.getCurrentBuffer(), width * actualHeight);
      }

      rowsRendered += chunkHeight;
      g_buffers.swapBuffers();
    }
  }
}