#pragma once
#include "../ui_types.h"
#include "utils.h"
#include <Arduino.h>

/**
 * @brief Cache for rendering a list of elements efficiently
 */
class ListCache {
public:
  // Cache storage for binary bitmaps
  uint8_t *binaryCache[ELEMENTS_PER_SCREEN];
  /**
   * @brief width of the cache in pixels
   */
  int cacheWidth;
  /**
   * @brief height of the cache in pixels
   */
  int cacheHeight;
  /**
   * @brief Number of bits per pixel used in the cache (1,2, or 4)
   *
   */
  int bitsPerPixel = 2;
  /**
   * @brief How many bytes arer needed to store one elements (depends on bits)
   * height and bits per pixel
   */
  int bytesPerTrack;
  /**
   * @brief Array containing whether a row contains an element or not
   */
  bool cacheValid[ELEMENTS_PER_SCREEN];
  /**
   * @brief LEFT margin for text rendering
   * TODO: (should be stored somewhere else ? )
   */
  static const int MARGIN_LEFT = 5;
  /**
   * @brief Reference to the font used for rendering text
   */
  const FastFont &font;

public:
  ListCache(const FastFont &font, int bitsPerPixel = 4) : font(font) {
    // Calculate cache dimensions
    this->bitsPerPixel = bitsPerPixel;
    cacheWidth = SCREEN_WIDTH - SCROLLBAR_WIDTH;
    cacheHeight = ELEMENT_HEIGHT;
    bytesPerTrack = calculateBufferSize(cacheWidth, cacheHeight, bitsPerPixel);

    // Allocate cache memory
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      binaryCache[i] = new uint8_t[bytesPerTrack];
      cacheValid[i] = false;
    }
  }

  ~ListCache() {
    // Clean up allocated memory
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      delete[] binaryCache[i];
    }
  }

  /**
   * @brief Render a single element to a binary buffer using class members
   * @param element The text element to render
   * @param binaryBuffer The buffer to render the element into
   * @param x X position to start rendering (default: 0)
   * @param width Width of the rendering area (default: cache width)
   */
  void renderElementBinary(const char *element, uint8_t *binaryBuffer,
                           int x = 0, int width = -1) {
    if (width == -1) {
      width = cacheWidth;
    }
    
    // Clear the binary buffer
    memset(binaryBuffer, 0, bytesPerTrack);

    // Create direct binary renderer using class members
    BinaryFontRenderer binaryRenderer(binaryBuffer, cacheWidth, cacheHeight,
                                      bitsPerPixel);

    // Find visible portion
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; element[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {element[i], '\0'};
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
      strncpy(toRender, element + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, element, startChar);
      upToStart[startChar] = '\0';
      int offsetX =
          textStartX + binaryRenderer.measureText(upToStart, font) - x;

      // Direct render to binary buffer
      binaryRenderer.renderText(toRender, offsetX, 10, font);
    }
  }

  /**
   * @brief Build the cache for the given elements
   */
  void build(const char *visibleTracks[ELEMENTS_PER_SCREEN]) {
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      if (visibleTracks[i] && strlen(visibleTracks[i]) > 0) {
        // Render element to binary cache
        renderElementBinary(visibleTracks[i], binaryCache[i]);
        cacheValid[i] = true;

      } else {
        cacheValid[i] = false;
      }
    }
  }
  /**
   * @brief Fast scroll up by a number of rows (pointer swapping)
   * @param delta Number of rows to scroll up (1-6)
   * @param newVisibleElements Array of new visible elements to update the cache
   * @note If delta is 0 or >= ELEMENTS_PER_SCREEN, it will rebuild the cache
   * completely with the new visible elements.
   * This is useful for page changes or when the entire list needs to be
   * refreshed.
   */
  void scrollUp(int delta, const char *newVisibleElements[ELEMENTS_PER_SCREEN]) {
    if (delta <= 0 || delta >= ELEMENTS_PER_SCREEN) {
      Serial.println("PAGE CHANGE");
      build(newVisibleElements);
      return;
    }

    // Shift content upward
    for (int i = ELEMENTS_PER_SCREEN - 1; i >= delta; i--) {
      memcpy(binaryCache[i], binaryCache[i - delta], bytesPerTrack);
      cacheValid[i] = cacheValid[i - delta];
    }

    // Render new top entries
    for (int i = 0; i < delta; i++) {
      const char *element = newVisibleElements[i];
      if (element && strlen(element) > 0) {
        renderElementBinary(element, binaryCache[i]);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
      }
    }
  }
  /**
   * @brief Fast scroll down by a number of rows (pointer swapping)
   * @param delta Number of rows to scroll down (1-6)
   * @param newVisibleElements Array of new visible elements to update the cache
   * @note If delta is 0 or >= ELEMENTS_PER_SCREEN, it will rebuild the cache
   * completely with the new visible elements.
   * This is useful for page changes or when the entire list needs to be
   * refreshed.
   */
  void scrollDown(int delta, const char *newVisibleElements[ELEMENTS_PER_SCREEN]) {
    if (delta <= 0 || delta >= ELEMENTS_PER_SCREEN) {
      Serial.println("PAGE CHANGE");
      build(newVisibleElements);
      return;
    }

    // Shift content downward
    for (int i = 0; i < ELEMENTS_PER_SCREEN - delta; i++) {
      memcpy(binaryCache[i], binaryCache[i + delta], bytesPerTrack);
      cacheValid[i] = cacheValid[i + delta];
    }

    // Render new bottom entries
    for (int i = ELEMENTS_PER_SCREEN - delta; i < ELEMENTS_PER_SCREEN; i++) {
      const char *element = newVisibleElements[i];
      if (element && strlen(element) > 0) {
        renderElementBinary(element, binaryCache[i]);
        cacheValid[i] = true;
      } else {
        cacheValid[i] = false;
      }
    }
  }

  /**
   * @brief Get the number of bits per pixel used in the cache
   */
  int getBitsPerPixel() const { return bitsPerPixel; }
  /**
   * @brief Get the pointer to the binary cache for a specific row in the list
   */
  uint8_t *getCacheForRow(int index) const {
    if (index < 0 || index >= ELEMENTS_PER_SCREEN)
      return nullptr;
    return binaryCache[index];
  }
  // Get cache statistics
  void printCacheStats() {
    int validEntries = 0;
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      if (cacheValid[i])
        validEntries++;
    }

    Serial.println("=== Cache Statistics ===");
    Serial.println("Cache size: " + String(cacheWidth) + "x" +
                   String(cacheHeight));
    Serial.println("Bits per pixel: " + String(bitsPerPixel));
    Serial.println("Bytes per track: " + String(bytesPerTrack));
    Serial.println("Valid entries: " + String(validEntries) + "/" +
                   String(ELEMENTS_PER_SCREEN));
    Serial.println("Total memory: " +
                   String(bytesPerTrack * ELEMENTS_PER_SCREEN) + " bytes");
  }

};