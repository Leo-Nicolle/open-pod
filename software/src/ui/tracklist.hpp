#pragma once
#include "../fonts/IBMPlexSans16Bold.h"
#include "theme.h"
#include "trackscache.hpp"
#include "ui_types.h"
#include <Arduino.h>

class ILI9341_GFX;
class TrackListComponent {
private:
  TracksCache cache; // Using the updated TracksCache with pointer swapping
  const char **tracks;
  int totalTracks;
  int selectedTrack;
  int topVisibleTrack;

public:
  static const int BITS_PER_PIXEL = 2;
  TrackListComponent(const char **trackList, int count)
      : tracks(trackList), totalTracks(count), selectedTrack(0),
        topVisibleTrack(0), cache(IBMPlexSans16Bold, 2) {}

  void begin() {
    // Build initial cache with visible tracks
    const char *visibleTracks[TRACKS_PER_SCREEN];
    for (int i = 0; i < TRACKS_PER_SCREEN && i < totalTracks; i++) {
      visibleTracks[i] = tracks[i];
    }
    cache.build(visibleTracks);
  }

  void setScroll(int selected, int topVisible) {
    selectedTrack = selected;
    topVisibleTrack = topVisible;
  }

  // Efficient scrolling using the cache's pointer swapping
  void scrollUp(int delta, const char **newVisibleTracks) {
    if (delta <= 0)
      return;

    // Update the cache with efficient pointer swapping
    cache.scrollUp(delta, newVisibleTracks);
  }

  void scrollDown(int delta, const char **newVisibleTracks) {
    if (delta <= 0)
      return;

    // Update the cache with efficient pointer swapping
    cache.scrollDown(delta, newVisibleTracks);
  }

  void renderAllTracks(ILI9341_GFX *display, int xOffset = 0,
                       int yOffset = BODY_Y,
                       int width = SCREEN_WIDTH - SCROLLBAR_WIDTH) {
    // Use global buffer for efficient rendering
    uint16_t *buffer = g_buffers.getCurrentBuffer();

    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      int trackIndex = topVisibleTrack + i;
      bool isVisible = (trackIndex < totalTracks);
      bool isSelected = (trackIndex == selectedTrack);

      if (isVisible) {
        // Render track to buffer using cache
        cache.renderTrackToBuffer(i, isSelected, buffer, width);
      } else {
        // Fill with background for empty slots
        cache.fillBufferWithBackground(buffer, width);
      }

      // Copy buffer to display
      int yPos = yOffset + (i * TRACK_HEIGHT);
      display->pushWindow(xOffset, yPos, width, TRACK_HEIGHT, buffer);
    }
  }

  void renderTrackRange(ILI9341_GFX *display, int startTrack, int endTrack,
                        int xOffset = 0, int yOffset = BODY_Y,
                        int width = SCREEN_WIDTH - SCROLLBAR_WIDTH) {
    uint16_t *buffer = g_buffers.getCurrentBuffer();

    for (int i = startTrack; i <= endTrack && i < TRACKS_PER_SCREEN; i++) {
      int trackIndex = topVisibleTrack + i;
      bool isVisible = (trackIndex < totalTracks);
      bool isSelected = (trackIndex == selectedTrack);

      if (isVisible) {
        cache.renderTrackToBuffer(i, isSelected, buffer, width);
      } else {
        cache.fillBufferWithBackground(buffer, width);
      }

      int yPos = yOffset + (i * TRACK_HEIGHT);
      display->pushWindow(xOffset, yPos, width, TRACK_HEIGHT, buffer);
    }
  }

  // Render a specific chunk for transitions
  void renderChunk(ILI9341_GFX *display, int xOffset, int yOffset, int width) {
    // Calculate which tracks are visible in this chunk
    int startY = yOffset - BODY_Y;
    int endY = startY + CHUNK_HEIGHT;

    int startTrack = startY / TRACK_HEIGHT;
    int endTrack = (endY - 1) / TRACK_HEIGHT;

    // Constrain to valid range
    startTrack = max(0, min(startTrack, TRACKS_PER_SCREEN - 1));
    endTrack = max(0, min(endTrack, TRACKS_PER_SCREEN - 1));

    renderTrackRange(display, startTrack, endTrack, xOffset, yOffset, width);
  }

  // Force rebuild cache (useful when track list changes significantly)
  void rebuildCache() {
    const char *visibleTracks[TRACKS_PER_SCREEN];
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      int trackIdx = topVisibleTrack + i;
      visibleTracks[i] = (trackIdx < totalTracks) ? tracks[trackIdx] : "";
    }
    cache.build(visibleTracks);
  }

  // Update track data (call this if the track list changes)
  void updateTracks(const char **newTracks, int newCount) {
    tracks = newTracks;
    totalTracks = newCount;

    // Ensure indices are still valid
    selectedTrack = constrain(selectedTrack, 0, totalTracks - 1);
    topVisibleTrack =
        constrain(topVisibleTrack, 0, max(0, totalTracks - TRACKS_PER_SCREEN));

    // Rebuild cache with new data
    rebuildCache();
  }
};