#pragma once
#include "trackrenderer.hpp"
#include "ui_types.h"
#include <Arduino.h>

class TracksCache {
public:
  TrackRenderer renderer;
  
  // Cache storage for binary bitmaps
  uint8_t* binaryCache[TRACKS_PER_SCREEN];
  int cacheWidth;
  int cacheHeight;
  int bytesPerTrack;
  bool cacheValid[TRACKS_PER_SCREEN];
  
  // Track names for cache validation
  String cachedTrackNames[TRACKS_PER_SCREEN];

public:
  TracksCache(const FastFont &font, int bitsPerPixel = 2) 
    : renderer(font, bitsPerPixel) {
    
    // Calculate cache dimensions
    cacheWidth = SCREEN_WIDTH - SCROLLBAR_WIDTH;
    cacheHeight = TRACK_HEIGHT;
    bytesPerTrack = TrackRenderer::calculateBufferSize(cacheWidth, cacheHeight, bitsPerPixel);
    
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
  void build(const char* visibleTracks[TRACKS_PER_SCREEN]) {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (visibleTracks[i] && strlen(visibleTracks[i]) > 0) {
        // Render track to binary cache
        renderer.renderTrackBinary(visibleTracks[i], binaryCache[i], 
                                 cacheWidth, cacheHeight, 0, cacheWidth);
        cachedTrackNames[i] = String(visibleTracks[i]);
        cacheValid[i] = true;
        
      } else {
        cacheValid[i] = false;
        cachedTrackNames[i] = "";
      }
    }
  }

  // Efficient scrolling up with pointer swapping
  void scrollUp(int delta, const char* newVisibleTracks[TRACKS_PER_SCREEN]) {
    if (delta <= 0 || delta >= TRACKS_PER_SCREEN) {
      // Full rebuild needed
      build(newVisibleTracks);
      return;
    }
    
    // Save pointers to tracks that are scrolling off
    uint8_t* tempPointers[delta];
    String tempNames[delta];
    bool tempValid[delta];
    
    for (int i = 0; i < delta; i++) {
      tempPointers[i] = binaryCache[i];
      tempNames[i] = cachedTrackNames[i];
      tempValid[i] = cacheValid[i];
    }
    
    // Shift existing cache entries down
    for (int i = 0; i < TRACKS_PER_SCREEN - delta; i++) {
      binaryCache[i] = binaryCache[i + delta];
      cachedTrackNames[i] = cachedTrackNames[i + delta];
      cacheValid[i] = cacheValid[i + delta];
    }
    
    // Use the freed pointers for new tracks at the top
    for (int i = 0; i < delta; i++) {
      int cacheIndex = TRACKS_PER_SCREEN - delta + i;
      binaryCache[cacheIndex] = tempPointers[i];
      
      // Render new track
      const char* trackName = newVisibleTracks[cacheIndex];
      if (trackName && strlen(trackName) > 0) {
        renderer.renderTrackBinary(trackName, binaryCache[cacheIndex], 
                                 cacheWidth, cacheHeight, 0, cacheWidth);
        cachedTrackNames[cacheIndex] = String(trackName);
        cacheValid[cacheIndex] = true;
      } else {
        cacheValid[cacheIndex] = false;
        cachedTrackNames[cacheIndex] = "";
      }
    }
  }

  // Efficient scrolling down with pointer swapping
  void scrollDown(int delta, const char* newVisibleTracks[TRACKS_PER_SCREEN]) {
    if (delta <= 0 || delta >= TRACKS_PER_SCREEN) {
      // Full rebuild needed
      build(newVisibleTracks);
      return;
    }
    
    // Save pointers to tracks that are scrolling off
    uint8_t* tempPointers[delta];
    String tempNames[delta];
    bool tempValid[delta];
    
    for (int i = 0; i < delta; i++) {
      int sourceIndex = TRACKS_PER_SCREEN - delta + i;
      tempPointers[i] = binaryCache[sourceIndex];
      tempNames[i] = cachedTrackNames[sourceIndex];
      tempValid[i] = cacheValid[sourceIndex];
    }
    
    // Shift existing cache entries up
    for (int i = TRACKS_PER_SCREEN - 1; i >= delta; i--) {
      binaryCache[i] = binaryCache[i - delta];
      cachedTrackNames[i] = cachedTrackNames[i - delta];
      cacheValid[i] = cacheValid[i - delta];
    }
    
    // Use the freed pointers for new tracks at the bottom
    for (int i = 0; i < delta; i++) {
      binaryCache[i] = tempPointers[i];
      
      // Render new track
      const char* trackName = newVisibleTracks[i];
      if (trackName && strlen(trackName) > 0) {
        renderer.renderTrackBinary(trackName, binaryCache[i], 
                                 cacheWidth, cacheHeight, 0, cacheWidth);
        cachedTrackNames[i] = String(trackName);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
        cachedTrackNames[i] = "";
      }
    }
    
  }

  // Render cached track to display buffer
  void renderTrackToBuffer(int trackIndex, bool selected, uint16_t* displayBuffer, int displayWidth) {
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
    
    renderer.renderBinaryToDisplay(binaryCache[trackIndex], cacheWidth, cacheHeight,
                                 displayBuffer, displayWidth, cacheHeight,
                                 textColor, backgroundColor, selected);
  }

  // Fill buffer with background color
  void fillBufferWithBackground(uint16_t* displayBuffer, int displayWidth) {
    int totalPixels = displayWidth * cacheHeight;
    for (int i = 0; i < totalPixels; i++) {
      displayBuffer[i] = COLOR_BACKGROUND;
    }
  }

  // Check if a specific track needs to be re-rendered
  bool needsUpdate(int trackIndex, const char* trackName) {
    if (trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN) return false;
    if (!cacheValid[trackIndex]) return true;
    
    String currentName = (trackName) ? String(trackName) : String("");
    return cachedTrackNames[trackIndex] != currentName;
  }

  // Update a specific track in the cache
  void updateTrack(int trackIndex, const char* trackName) {
    if (trackIndex < 0 || trackIndex >= TRACKS_PER_SCREEN) return;
    
    if (trackName && strlen(trackName) > 0) {
      renderer.renderTrackBinary(trackName, binaryCache[trackIndex], 
                               cacheWidth, cacheHeight, 0, cacheWidth);
      cachedTrackNames[trackIndex] = String(trackName);
      cacheValid[trackIndex] = true;
    } else {
      cacheValid[trackIndex] = false;
      cachedTrackNames[trackIndex] = "";
    }
  }

  // Change the bits per pixel for the renderer (useful for testing)
  void setBitsPerPixel(int bpp) {
    renderer.setBitsPerPixel(bpp);
    
    // Recalculate buffer size if needed
    int newBytesPerTrack = TrackRenderer::calculateBufferSize(cacheWidth, cacheHeight, bpp);
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
  int getBitsPerPixel() const {
    return renderer.getBitsPerPixel();
  }

  // Get cache statistics
  void printCacheStats() {
    int validEntries = 0;
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (cacheValid[i]) validEntries++;
    }
    
    Serial.println("=== Cache Statistics ===");
    Serial.println("Cache size: " + String(cacheWidth) + "x" + String(cacheHeight));
    Serial.println("Bits per pixel: " + String(renderer.getBitsPerPixel()));
    Serial.println("Bytes per track: " + String(bytesPerTrack));
    Serial.println("Valid entries: " + String(validEntries) + "/" + String(TRACKS_PER_SCREEN));
    Serial.println("Total memory: " + String(bytesPerTrack * TRACKS_PER_SCREEN) + " bytes");
    
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      if (cacheValid[i]) {
        Serial.println("  [" + String(i) + "] " + cachedTrackNames[i]);
      }
    }
  }

  // Force invalidate all cache entries
  void invalidateAll() {
    for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
      cacheValid[i] = false;
      cachedTrackNames[i] = "";
    }
  }
};