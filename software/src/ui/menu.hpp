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

  // Generate subtle gradient color at specific position
  uint16_t getGradientColor(uint16_t baseColor, int y, int totalHeight) {
    // Very subtle gradient - only 5% variation from top to bottom
    float progress = (float)y / (totalHeight - 1);

    // Extract RGB components from base color (RGB565)
    uint8_t r = (baseColor >> 11) & 0x1F;
    uint8_t g = (baseColor >> 5) & 0x3F;
    uint8_t b = baseColor & 0x1F;

    // Very subtle brightness variation: +2% at top, -2% at bottom
    float brightnessMod = 1.0f + 0.04f * (0.5f - progress); // +2% to -2%

    // Apply brightness modification with clamping
    int newR = constrain((int)(r * brightnessMod), 0, 31);
    int newG = constrain((int)(g * brightnessMod), 0, 63);
    int newB = constrain((int)(b * brightnessMod), 0, 31);

    return (newR << 11) | (newG << 5) | newB;
  }

  // uint16_t darkenColor(uint16_t color, float factor) {
  //   uint8_t r = ((color >> 11) & 0x1F) * factor;
  //   uint8_t g = ((color >> 5) & 0x3F) * factor;
  //   uint8_t b = (color & 0x1F) * factor;
  //   return (r << 11) | (g << 5) | b;
  // }

  // uint16_t interpolateColors(uint16_t color1, uint16_t color2, float t) {
  //   uint8_t r1 = (color1 >> 11) & 0x1F, r2 = (color2 >> 11) & 0x1F;
  //   uint8_t g1 = (color1 >> 5) & 0x3F, g2 = (color2 >> 5) & 0x3F;
  //   uint8_t b1 = color1 & 0x1F, b2 = color2 & 0x1F;

  //   uint8_t r = r1 + (r2 - r1) * t;
  //   uint8_t g = g1 + (g2 - g1) * t;
  //   uint8_t b = b1 + (b2 - b1) * t;

  //   return (r << 11) | (g << 5) | b;
  // }

public:
  MenuItemRenderer(const FastFont &mainFont, const FastFont &numFont)
      : font(mainFont), smallFont(numFont) {}
  void renderMenuItem(const char *title, bool selected, int x = 0,
                      int width = SCREEN_WIDTH) {
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, width, CHUNK_HEIGHT);

    uint16_t baseColor = selected ? COLOR_PRIMARY : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    if (selected) {
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        uint16_t gradientColor = getGradientColor(baseColor, y, CHUNK_HEIGHT);
        int rowStart = y * width;
        for (int px = 0; px < width; px++) {
          buf[rowStart + px] = gradientColor;
        }
      }
    } else {
      for (int i = 0; i < CHUNK_HEIGHT * width; i++) {
        buf[i] = COLOR_BACKGROUND;
      }
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

      fontRenderer.renderText(toRender, offsetX, 10, font, textColor,
                              baseColor);
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