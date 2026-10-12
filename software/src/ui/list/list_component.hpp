#pragma once
#include "../../fonts/IBMPlexSans16Bold.h"
#include "../fast_scroll_overlay.hpp"
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
  ListCache cache; // Using the updated ElementsCache with pointer swapping
  ListRenderer renderer;
  // The rows on screen (State's visible elements: ELEMENTS_PER_SCREEN
  // entries, kept current by State as the list scrolls).
  const char *const *visible;
  int numElements;
  int selectedIndex;
  int topVisibleElement;
  // Fast-scroll letter overlay label, composed over rows 2-4; nullptr when
  // hidden.
  const char *overlayLabel;

public:
  static const int BITS_PER_PIXEL = 2;
  ListComponent(const char *const *visible, int count)
      : visible(visible), numElements(count), selectedIndex(0),
        topVisibleElement(0), overlayLabel(nullptr),
        cache(IBMPlexSans16Bold, BITS_PER_PIXEL), renderer(cache) {}

  void setOverlayLabel(const char *label) { overlayLabel = label; }

  void begin() {
    const char *visibleElements[ELEMENTS_PER_SCREEN];
    for(int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
      visibleElements[i] = nullptr;
      cache.cacheValid[i] = false;
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
    renderRows(display, 0, ELEMENTS_PER_SCREEN - 1, xOffset, yOffset, width);
  }

  /**
   * @brief Render visible rows firstRow..lastRow (inclusive), with the
   * fast-scroll overlay composed over the rows it covers
   */
  void renderRows(ILI9341_GFX *display, int firstRow, int lastRow,
                  int xOffset = 0, int yOffset = BODY_Y,
                  int width = SCREEN_WIDTH - SCROLLBAR_WIDTH) {
    // Use global buffer for efficient rendering
    // No clear-to-background first: every row below is fully overwritten, and
    // a clear makes rows flash black (visible tearing while scrolling fast).
    uint16_t *buffer = g_buffers.getCurrentBuffer();
    for (int i = max(firstRow, 0); i <= min(lastRow, ELEMENTS_PER_SCREEN - 1);
         i++) {
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

      int yPos = yOffset + (i * ELEMENT_HEIGHT);
      FastScrollOverlay::compose(buffer, width, xOffset, yPos, overlayLabel);

      // Copy buffer to display
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
      const char *e = visible ? visible[i] : nullptr;
      visibleElements[i] = (topVisibleElement + i < numElements && e) ? e : "";
    }
    cache.build(visibleElements);
  }

  /**
   * @brief Set the list (its on-screen rows and length) and rebuild the cache
   * @param visibleRows The rows on screen, kept current by State
   * @param newCount Length of the whole list
   */
  void setElements(const char *const *visibleRows, int newCount) {
    visible = visibleRows;
    numElements = newCount;

    // Ensure indices are still valid
    selectedIndex = constrain(selectedIndex, 0, numElements - 1);
    topVisibleElement = constrain(topVisibleElement, 0,
                                  max(0, numElements - ELEMENTS_PER_SCREEN));

    // Rebuild cache with new data
    rebuildCache();
  }
};