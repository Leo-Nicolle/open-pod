#pragma once
#include "fonts/IBMPlexSans16Bold.h"
#include "rendering/font_renderer.h"
#include "sprites.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>
#include <string.h>

class ILI9341_GFX;

class HeaderComponent {
private:
  bool isDirty = true;

public:
  HeaderComponent();
  ~HeaderComponent();

  void markDirty() { isDirty = true; }
  bool needsUpdate() const { return isDirty; }

  // Render the 30px header (chunk 0): SURFACE background, a 1px LINE rule at
  // the bottom, the transport glyph on the left, the breadcrumb title, and a
  // battery on the right.
  void render(ILI9341_GFX *display, const char *title = "OpenPod",
              bool playing = false);
};

HeaderComponent::HeaderComponent() {}

HeaderComponent::~HeaderComponent() {}

void HeaderComponent::render(ILI9341_GFX *display, const char *title,
                             bool playing) {
  uint16_t *buffer = g_buffers.getCurrentBuffer();

  // SURFACE background.
  for (int i = 0; i < SCREEN_WIDTH * HEADER_HEIGHT; i++) {
    buffer[i] = COLOR_SURFACE;
  }

  // 1px LINE rule at y = HEADER_HEIGHT - 1.
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    buffer[(HEADER_HEIGHT - 1) * SCREEN_WIDTH + x] = COLOR_LINE;
  }

  // Transport glyph (tinted COLOR_ACCENT, baked into the sprite).
  blitSprite(buffer, SCREEN_WIDTH, HEADER_HEIGHT,
             playing ? SPR_PAUSE : SPR_PLAY, 8, 10);

  // Breadcrumb title, truncated with an ellipsis to fit between the glyph
  // and the battery.
  const char *headerTitle = title ? title : "OpenPod";
  char truncatedTitle[64];
  strncpy(truncatedTitle, headerTitle, sizeof(truncatedTitle) - 1);
  truncatedTitle[sizeof(truncatedTitle) - 1] = '\0';

  fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
  const int titleX = 24;
  const int maxTitleWidth = 290 - titleX - 8; // battery sits at x=290
  if (fontRenderer.measureText(truncatedTitle, IBMPlexSans16Bold) >
      maxTitleWidth) {
    int len = strlen(truncatedTitle);
    while (len > 1) {
      char test[64];
      snprintf(test, sizeof(test), "%.*s...", len, truncatedTitle);
      if (fontRenderer.measureText(test, IBMPlexSans16Bold) <= maxTitleWidth) {
        break;
      }
      len--;
    }
    snprintf(truncatedTitle, sizeof(truncatedTitle), "%.*s...", len,
             truncatedTitle);
  }
  fontRenderer.renderText(truncatedTitle, titleX, 7, IBMPlexSans16Bold,
                          COLOR_TEXT, COLOR_SURFACE);

  // Battery sprite (outline + nub) at (290,9), then its 3 fill cells drawn as
  // rects so the level is free. 2 filled + 1 empty as a placeholder.
  blitSprite(buffer, SCREEN_WIDTH, HEADER_HEIGHT, SPR_BATTERY, 290, 9);
  auto fillCell = [&](int cx, int cy, uint16_t color) {
    for (int y = cy; y < cy + 5; y++) {
      for (int x = cx; x < cx + 4; x++) {
        buffer[y * SCREEN_WIDTH + x] = color;
      }
    }
  };
  fillCell(293, 12, COLOR_TEXT);
  fillCell(298, 12, COLOR_TEXT);
  fillCell(303, 12, COLOR_LINE);

  display->setWindow(0, 0, SCREEN_WIDTH - 1, HEADER_HEIGHT - 1);
  display->pushPixels(buffer, SCREEN_WIDTH * HEADER_HEIGHT);
  isDirty = false;
}
