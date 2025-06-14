#pragma once
#include "trackrenderer.hpp"
#include "ui_types.h"
#include <Arduino.h>

class TracksCache {
public:
  TrackRenderer renderer;

  // Cache storage for binary bitmaps
  uint8_t *binaryCache[TRACKS_PER_SCREEN];
  int cacheWidth;
  int cacheHeight;
  int bytesPerTrack;
  bool cacheValid[TRACKS_PER_SCREEN];

public:
  TracksCache(const FastFont &font, int bitsPerPixel = 2)
      : renderer(font, bitsPerPixel) {

    // Calculate cache dimensions
    cacheWidth = SCREEN_WIDTH - SCROLLBAR_WIDTH;
    cacheHeight = TRACK_HEIGHT;
    bytesPerTrack = TrackRenderer::calculateBufferSize(cacheWidth, cacheHeight,
                                                       bitsPerPixel);

    // Allocate cache memory
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      binaryCache[i] = new uint8_t[bytesPerTrack];
      cacheValid[i] = false;
    }
  }

  ~TracksCache() {
    // Clean up allocated memory
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      delete[] binaryCache[i];
    }
  }

  // Build cache for visible tracks
  void build(const char *visibleTracks[TRACKS_PER_SCREEN]) {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (visibleTracks[i] && strlen(visibleTracks[i]) > 0) {
        // Render track to binary cache
        renderer.renderTrackBinary(visibleTracks[i], binaryCache[i], cacheWidth,
                                   cacheHeight, 0, cacheWidth);
        cacheValid[i] = true;

      } else {
        cacheValid[i] = false;
      }
    }
  }
  void scrollUp(int delta, const char *newVisibleTracks[TRACKS_PER_SCREEN]) {
    if (delta <= 0 || delta >= TRACKS_PER_SCREEN) {
      Serial.println("PAGE CHANGE");
      build(newVisibleTracks);
      return;
    }

    // Shift content upward
    for (int i = TRACKS_PER_SCREEN - 1; i >= delta; i--) {
      memcpy(binaryCache[i], binaryCache[i - delta], bytesPerTrack);
      cacheValid[i] = cacheValid[i - delta];
    }

    // Render new top entries
    for (int i = 0; i < delta; i++) {
      const char *trackName = newVisibleTracks[i];
      if (trackName && strlen(trackName) > 0) {
        renderer.renderTrackBinary(trackName, binaryCache[i], cacheWidth,
                                   cacheHeight, 0, cacheWidth);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
      }
    }
  }

  void scrollDown(int delta, const char *newVisibleTracks[TRACKS_PER_SCREEN]) {
    if (delta <= 0 || delta >= TRACKS_PER_SCREEN) {
      Serial.println("PAGE CHANGE");
      build(newVisibleTracks);
      return;
    }

    // Shift content downward
    for (int i = 0; i < TRACKS_PER_SCREEN - delta; i++) {
      memcpy(binaryCache[i], binaryCache[i + delta], bytesPerTrack);
      cacheValid[i] = cacheValid[i + delta];
    }

    // Render new bottom entries
    for (int i = TRACKS_PER_SCREEN - delta; i < TRACKS_PER_SCREEN; i++) {
      const char *trackName = newVisibleTracks[i];
      if (trackName && strlen(trackName) > 0) {
        renderer.renderTrackBinary(trackName, binaryCache[i], cacheWidth,
                                   cacheHeight, 0, cacheWidth);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
      }
    }
  }

  // Render cached track to display buffer
  void renderTrackToBuffer(int trackIndex, bool selected,
                           uint16_t *displayBuffer, int displayWidth) {
    if (trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN) {
      fillBufferWithBackground(displayBuffer, displayWidth);
      return;
    }

    if (!cacheValid[trackIndex]) {
      fillBufferWithBackground(displayBuffer, displayWidth);
      return;
    }

    // Use the renderer to convert binary cache to colored display
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
    uint16_t backgroundColor = COLOR_BACKGROUND;

    renderer.renderBinaryToDisplay(
        binaryCache[trackIndex], cacheWidth, cacheHeight, displayBuffer,
        displayWidth, cacheHeight, textColor, backgroundColor, selected);
  }

  // Fill buffer with background color
  void fillBufferWithBackground(uint16_t *displayBuffer, int displayWidth) {
    int totalPixels = displayWidth * cacheHeight;
    for (int i = 0; i < totalPixels; i++) {
      displayBuffer[i] = COLOR_BACKGROUND;
    }
  }
  // Update a specific track in the cache
  void updateTrack(int trackIndex, const char *trackName) {
    if (trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN)
      return;

    if (trackName && strlen(trackName) > 0) {
      renderer.renderTrackBinary(trackName, binaryCache[trackIndex], cacheWidth,
                                 cacheHeight, 0, cacheWidth);
      cacheValid[trackIndex] = true;
    } else {
      cacheValid[trackIndex] = false;
    }
  }

  // Change the bits per pixel for the renderer (useful for testing)
  void setBitsPerPixel(int bpp) {
    renderer.setBitsPerPixel(bpp);

    // Recalculate buffer size if needed
    int newBytesPerTrack =
        TrackRenderer::calculateBufferSize(cacheWidth, cacheHeight, bpp);
    if (newBytesPerTrack != bytesPerTrack) {
      // Need to reallocate cache
      for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
        delete[] binaryCache[i];
        binaryCache[i] = new uint8_t[newBytesPerTrack];
        cacheValid[i] = false;
      }
      bytesPerTrack = newBytesPerTrack;
    }

    // Invalidate all cache entries since bit depth changed
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      cacheValid[i] = false;
    }
  }

  // Get current bits per pixel
  int getBitsPerPixel() const { return renderer.getBitsPerPixel(); }

  // Get cache statistics
  void printCacheStats() {
    int validEntries = 0;
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (cacheValid[i])
        validEntries++;
    }

    Serial.println("=== Cache Statistics ===");
    Serial.println("Cache size: " + String(cacheWidth) + "x" +
                   String(cacheHeight));
    Serial.println("Bits per pixel: " + String(renderer.getBitsPerPixel()));
    Serial.println("Bytes per track: " + String(bytesPerTrack));
    Serial.println("Valid entries: " + String(validEntries) + "/" +
                   String(TRACKS_PER_SCREEN));
    Serial.println("Total memory: " +
                   String(bytesPerTrack * TRACKS_PER_SCREEN) + " bytes");
  }

  // Force invalidate all cache entries
  void invalidateAll() {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      cacheValid[i] = false;
    }
  }
};