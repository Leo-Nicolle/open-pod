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

public:
  MenuItemRenderer(const FastFont &mainFont, const FastFont &numFont)
      : font(mainFont), smallFont(numFont) {}

  void renderMenuItem(int number, const char *title, bool selected) {
    uint16_t bgColor = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
    uint16_t numColor = 0x4208;

    // Clear background (optimized, excludes scrollbar area)
    const int menuWidth = SCROLLBAR_X; // Leave scrollbar at x = 312
    uint32_t BG = selected ? COLOR_ACCENT : COLOR_BACKGROUND;
    uint16_t* buf = g_buffers.getCurrentBuffer();
    for (int i = 0; i < 30 * 320; i++) {
      *buf++ = BG;
    }
    // Render title (with truncation if needed)
   int maxTitleWidth = 280; // Leave room for scrollbar
    int titleWidth = fontRenderer.measureText(title, font);

    if (titleWidth <= maxTitleWidth) {
      fontRenderer.renderText(title, 5, 15, font, textColor, bgColor);
    } else {
      // Truncate with ellipsis
      char truncated[32];
      int i = 0;
      int width = 0;
      int ellipsisWidth = fontRenderer.measureText("...", font);

      while (title[i] && width + ellipsisWidth < maxTitleWidth) {
        truncated[i] = title[i];
        width = fontRenderer.measureText(truncated, font);
        i++;
      }

      strcpy(&truncated[i - 1], "...");
      fontRenderer.renderText(truncated, 30, 15, font, textColor, bgColor);
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