#pragma once
#include "../../fonts/IBMPlexSans16Bold.h"
#include "../theme.h"
#include "../ui_types.h"
#include "list_cache.hpp"
#include "list_renderer.hpp"
#include "utils.h"
#include <Arduino.h>

class ILI9341_GFX;
/**
 * @brief Component for rendering a list of Elements with efficient scrolling
 */
class ListComponent {
public:
  ListCache cache; // Using the updated TracksCache with pointer swapping
  ListRenderer renderer;
  const char **elements;
  int numElements;
  int selectedIndex;
  int topVisibleElement;

public:
  static const int BITS_PER_PIXEL = 2;
  ListComponent(const char **elements, int count)
      : elements(elements), numElements(count), selectedIndex(0),
        topVisibleElement(0), cache(IBMPlexSans16Bold, BITS_PER_PIXEL),
        renderer(cache) {}

  void begin() {
    // Build initial cache with visible elements
    const char *visibleElements[ELEMENTS_PER_SCREEN];
    for (int i = 0; i < ELEMENTS_PER_SCREEN && i < numElements; i++) {
      visibleElements[i] = elements[i];
    }
    cache.build(visibleElements);
  }

  void setScroll(int selected, int topVisible) {
    selectedIndex = selected;
    topVisibleElement = topVisible;
  }

  /**
   * @brief Scroll the list up or down by a specified delta
   * @param delta Number of elements to scroll (alsways positive)
   * @param newVisibleElements Array of new visible elements to update the cache
   */
  void scrollUp(int delta, const char **newVisibleElements) {
    if (delta <= 0)
      return;

    // Update the cache with efficient pointer swapping
    cache.scrollUp(delta, newVisibleElements);
  }

  /**
   * @brief Scroll the list down by a specified delta
   * @param delta Number of elements to scroll (always positive)
   * @param newVisibleElements Array of new visible elements to update the cache
   */
  void scrollDown(int delta, const char **newVisibleElements) {
    if (delta <= 0)
      return;

    // Update the cache with efficient pointer swapping
    cache.scrollDown(delta, newVisibleElements);
  }

  /**
   * @brief Render all visible elements in the list to the display
   * @param display The display to render to
   * @param xOffset X offset for rendering
   * @param yOffset Y offset for rendering (default: BODY_Y)
   * @param width Width of the rendering area (default: SCREEN_WIDTH -
   * SCROLLBAR_WIDTH)
   */
  void renderAllElements(ILI9341_GFX *display, int xOffset = 0,
                         int yOffset = BODY_Y,
                         int width = SCREEN_WIDTH - SCROLLBAR_WIDTH) {
    // Use global buffer for efficient rendering
    uint16_t *buffer = g_buffers.getCurrentBuffer();
    display->fillRect(xOffset, yOffset, width, ELEMENTS_PER_SCREEN * ELEMENT_HEIGHT,
                      COLOR_BACKGROUND);
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      int elementIndex = topVisibleElement + i;
      bool isVisible = (elementIndex < numElements);
      bool isSelected = (elementIndex == selectedIndex);

      if (isVisible) {
        int relativeSelectedRow = isSelected ? i : -1;
        // Render element to buffer using cache
        renderer.renderRect(0, i * ELEMENT_HEIGHT, width, ELEMENT_HEIGHT, buffer,
                            relativeSelectedRow);
      } else {
        // Fill with background for empty slots
        renderer.fillBufferWithBackground(buffer, width, ELEMENT_HEIGHT);
      }

      // Copy buffer to display
      int yPos = yOffset + (i * ELEMENT_HEIGHT);
      display->pushWindow(xOffset, yPos, width, ELEMENT_HEIGHT, buffer);
    }
  }

  /**
   * @brief Render a viewport rectangle
   * @param display The display to render to
   * @param x The x position of the rectangle
   * @param y The y position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   * @param tx The x position to render the rectangle in the display: -1 to
   * render at 0 (default: -1)
   * @param ty The y position to render the rectangle in the display -1 to
   * render at BODY_Y (default: -1)
   */
  void renderRect(ILI9341_GFX *display, int x, int y, int width, int height,
                  int tx = -1, int ty = -1) {
    uint16_t *buffer = g_buffers.getCurrentBuffer();
    renderer.renderRect(x, y, width, height, buffer,
                        selectedIndex - topVisibleElement);

    if (tx < 0) {
      tx = x;
    }
    if (ty < 0) {
      ty = BODY_Y;
    }
    display->pushWindow(tx, ty, width, height, buffer);
  }

  /**
   * @brief Rebuild the cache with the current visible elements
   * This method should be called whenever all visible elements change.
   * Use scrollUp and scrollDown for incremental updates.
   */
  void rebuildCache() {
    const char *visibleElements[ELEMENTS_PER_SCREEN];
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      int elementIdx = topVisibleElement + i;
      visibleElements[i] = (elementIdx < numElements) ? elements[elementIdx] : "";
    }
    cache.build(visibleElements);
  }

  /**
   * @brief Set new elements for the list and rebuild the cache
   * @param newElements Array of new elements
   * @param newCount Number of new elements
   */
  void setElements(const char **newElements, int newCount) {
    elements = newElements;
    numElements = newCount;

    // Ensure indices are still valid
    selectedIndex = constrain(selectedIndex, 0, numElements - 1);
    topVisibleElement = constrain(topVisibleElement, 0,
                                  max(0, numElements - ELEMENTS_PER_SCREEN));

    // Rebuild cache with new data
    rebuildCache();
  }
};