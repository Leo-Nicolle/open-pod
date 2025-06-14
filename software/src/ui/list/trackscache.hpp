#pragma once
#include "../ui_types.h"
#include "trackrenderer.hpp"
#include "utils.h"
#include <Arduino.h>

class TracksCache {
public:
  // Cache storage for binary bitmaps
  uint8_t *binaryCache[TRACKS_PER_SCREEN];
  int cacheWidth;
  int cacheHeight;
  int bytesPerTrack;
  int bitsPerPixel = 2; // Default to 2 bits per pixel
  bool cacheValid[TRACKS_PER_SCREEN];
  static const int MARGIN_LEFT = 5;
  const FastFont& font;
public:
  TracksCache(const FastFont &font, int bitsPerPixel = 2): font(font) {
    // Calculate cache dimensions
    this->bitsPerPixel = bitsPerPixel;
    cacheWidth = SCREEN_WIDTH - SCROLLBAR_WIDTH;
    cacheHeight = TRACK_HEIGHT;
    bytesPerTrack = calculateBufferSize(cacheWidth, cacheHeight, bitsPerPixel);

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

  // Direct render to binary - no temp buffer needed
  void renderTrackBinary(const char *title, uint8_t *binaryBuffer,
                         int bufferWidth, int bufferHeight, int x = 0,
                         int width = SCREEN_WIDTH) {
    // Clear the binary buffer
    int totalBytes =
        calculateBufferSize(bufferWidth, bufferHeight, bitsPerPixel);
    memset(binaryBuffer, 0, totalBytes);

    // Create direct binary renderer
    BinaryFontRenderer binaryRenderer(binaryBuffer, bufferWidth, bufferHeight,
                                      bitsPerPixel);

    // Find visible portion
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = binaryRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;

      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1) {
          startChar = i;
        }
        endChar = i + 1;
      } else if (startChar != -1) {
        break;
      }

      currentWidth += charWidth;
      if (charStartX > x + width) {
        break;
      }
    }

    // Render visible text directly to binary
    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX =
          textStartX + binaryRenderer.measureText(upToStart, font) - x;

      // Direct render to binary buffer
      binaryRenderer.renderText(toRender, offsetX, 10, font);
    }
  }

  // Build cache for visible tracks
  void build(const char *visibleTracks[TRACKS_PER_SCREEN]) {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (visibleTracks[i] && strlen(visibleTracks[i]) > 0) {
        // Render track to binary cache
        renderTrackBinary(visibleTracks[i], binaryCache[i], cacheWidth,
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
        renderTrackBinary(trackName, binaryCache[i], cacheWidth,
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
        renderTrackBinary(trackName, binaryCache[i], cacheWidth,
                                   cacheHeight, 0, cacheWidth);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
      }
    }
  }
  // Update a specific track in the cache
  void updateTrack(int trackIndex, const char *trackName) {
    if (trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN)
      return;

    if (trackName && strlen(trackName) > 0) {
      renderTrackBinary(trackName, binaryCache[trackIndex], cacheWidth,
                                 cacheHeight, 0, cacheWidth);
      cacheValid[trackIndex] = true;
    } else {
      cacheValid[trackIndex] = false;
    }
  }

  // Change the bits per pixel for the renderer (useful for testing)
  void setBitsPerPixel(int bpp) {
    setBitsPerPixel(bpp);

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
  int getBitsPerPixel() const { return bitsPerPixel; }
  uint8_t *getCacheForRow(int index) {
    if (index < 0 || index >= TRACKS_PER_SCREEN)
      return nullptr;
    return binaryCache[index];
  }
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
    Serial.println("Bits per pixel: " + String(bitsPerPixel));
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