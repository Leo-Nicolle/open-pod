#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../rendering/font_renderer.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Simplified menu item renderer using global buffers
class MenuItemRenderer {
private:
  const FastFont &font;
  const FastFont &smallFont;
  static const int MARGIN_LEFT = 5; // Left margin for text
public:
  MenuItemRenderer(const FastFont &mainFont, const FastFont &numFont)
      : font(mainFont), smallFont(numFont) {}
  void renderMenuItem(const char *title, bool selected, int x = 0,
                      int width = SCREEN_WIDTH) {
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, width, CHUNK_HEIGHT);

    uint16_t bgColor = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    // Clear background
    for (int i = 0; i < CHUNK_HEIGHT * width; i++) {
      buf[i] = bgColor;
    }

    // Find visible portion in a single loop
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      // Measure width up to this character
      char tempChar[2] = {title[i], '\0'};
      int charWidth = fontRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;

      // Check if this character is visible
      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1) {
          startChar = i; // First visible character
        }
        endChar = i + 1; // Last visible character (exclusive)
      } else if (startChar != -1) {
        // We've passed the visible region
        break;
      }

      currentWidth += charWidth;

      // Early exit if we're way past the visible area
      if (charStartX > x + width) {
        break;
      }
    }

    if (startChar != -1) {
      // Extract and render the visible portion
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      // Calculate render position within the buffer
      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + fontRenderer.measureText(upToStart, font) - x;

      fontRenderer.renderText(toRender, offsetX, 15, font, textColor, bgColor);
    }
  }

private:
  // Safe title truncation helper
  void truncateTitle(const char *title, char *truncated, size_t bufferSize,
                     int maxWidth, FastFontRenderer &renderer) {
    if (!title || !truncated || bufferSize < 4) {
      if (truncated && bufferSize > 0)
        truncated[0] = '\0';
      return;
    }

    const char *ellipsis = "...";
    int ellipsisWidth = renderer.measureText(ellipsis, font);

    size_t titleLen = strlen(title);
    size_t maxLen = min(titleLen, bufferSize - 4); // Leave room for ellipsis

    // Find the longest substring that fits
    size_t bestLen = 0;
    for (size_t len = 1; len <= maxLen; len++) {
      strncpy(truncated, title, len);
      truncated[len] = '\0';

      int testWidth = renderer.measureText(truncated, font);
      if (testWidth + ellipsisWidth <= maxWidth) {
        bestLen = len;
      } else {
        break;
      }
    }

    // Create final string
    if (bestLen > 0 && bestLen < titleLen) {
      strncpy(truncated, title, bestLen);
      truncated[bestLen] = '\0';
      strncat(truncated, ellipsis, bufferSize - bestLen - 1);
    } else if (bestLen == titleLen) {
      strncpy(truncated, title, bufferSize - 1);
      truncated[bufferSize - 1] = '\0';
    } else {
      strncpy(truncated, ellipsis, bufferSize - 1);
      truncated[bufferSize - 1] = '\0';
    }
  }
};