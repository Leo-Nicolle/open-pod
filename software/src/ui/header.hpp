#pragma once
#include "fonts/IBMPlexSans16Bold.h"
#include "rendering/font_renderer.h"
#include "bt_widgets.hpp"
#include "sprites.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>
#include <string.h>

class ILI9341_GFX;

uikit::HeaderStatus uikit::headerStatus;

class HeaderComponent {
private:
  bool isDirty = true;

  // SURFACE background, bottom LINE rule, play/pause glyph, Bluetooth icon
  // and battery - the parts that never animate.
  void composeChrome(uint16_t *buffer, bool playing, bool showCodec) {
    for (int i = 0; i < SCREEN_WIDTH * HEADER_HEIGHT; i++) {
      buffer[i] = COLOR_SURFACE;
    }
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      buffer[(HEADER_HEIGHT - 1) * SCREEN_WIDTH + x] = COLOR_LINE;
    }

    blitSprite(buffer, SCREEN_WIDTH, HEADER_HEIGHT,
               playing ? SPR_PAUSE : SPR_PLAY, 8, 10);

    uikit::drawHeaderRight(buffer, showCodec);
  }

  // Truncates `title` with an ellipsis to fit between the glyph and the
  // Bluetooth icon (or the codec tag), and renders it at row y.
  void drawTitle(uint16_t *buffer, const char *title, int y, bool showCodec) {
    const char *headerTitle = title ? title : "opentone";
    char truncatedTitle[64];
    strncpy(truncatedTitle, headerTitle, sizeof(truncatedTitle) - 1);
    truncatedTitle[sizeof(truncatedTitle) - 1] = '\0';

    fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
    const int titleX = 24;
    const int maxTitleWidth = uikit::headerTitleRight(showCodec) - titleX;
    if (fontRenderer.measureText(truncatedTitle, IBMPlexSans16Bold) >
        maxTitleWidth) {
      int len = strlen(truncatedTitle);
      while (len > 1) {
        char test[64];
        snprintf(test, sizeof(test), "%.*s...", len, truncatedTitle);
        if (fontRenderer.measureText(test, IBMPlexSans16Bold) <=
            maxTitleWidth) {
          break;
        }
        len--;
      }
      snprintf(truncatedTitle, sizeof(truncatedTitle), "%.*s...", len,
               truncatedTitle);
    }
    fontRenderer.renderText(truncatedTitle, titleX, y, IBMPlexSans16Bold,
                            COLOR_TEXT, COLOR_SURFACE);
  }

public:
  HeaderComponent();
  ~HeaderComponent();

  void markDirty() { isDirty = true; }

  // Composes the header chunk into `buffer` (or the toast, while one is
  // shown). showCodec: the codec tag, on Now Playing.
  void compose(uint16_t *buffer, const char *title, bool playing,
               bool showCodec) {
    if (uikit::headerStatus.toastActive) {
      uikit::drawToast(buffer, *uikit::headerStatus.toast);
      return;
    }
    fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
    composeChrome(buffer, playing, showCodec);
    // y=10 matches the play/pause glyph's top (both are 11px tall - the
    // font's cap-height/ascent and the icon's - so their boxes line up and
    // sit centered in the 30px header row alongside the battery).
    drawTitle(buffer, title, 10, showCodec);
  }

  // Repaints only the 11x11 Bluetooth icon box (not during a toast).
  void renderIconBox(ILI9341_GFX *display);
  bool needsUpdate() const { return isDirty; }

  // Render the 30px header (chunk 0) at its fixed screen position: SURFACE
  // background, a 1px LINE rule at the bottom, the transport glyph on the
  // left, the breadcrumb title, and a battery on the right.
  void render(ILI9341_GFX *display, const char *title = "opentone",
              bool playing = false, bool showCodec = false);
};

HeaderComponent::HeaderComponent() {}

HeaderComponent::~HeaderComponent() {}

void HeaderComponent::render(ILI9341_GFX *display, const char *title,
                             bool playing, bool showCodec) {
  uint16_t *buffer = g_buffers.getCurrentBuffer();
  compose(buffer, title, playing, showCodec);

  display->setWindow(0, 0, SCREEN_WIDTH - 1, HEADER_HEIGHT - 1);
  display->pushPixels(buffer, SCREEN_WIDTH * HEADER_HEIGHT);
  isDirty = false;
}

void HeaderComponent::renderIconBox(ILI9341_GFX *display) {
  if (uikit::headerStatus.toastActive)
    return;
  const int n = uikit::BT_ICON_SIZE;
  uint16_t *buffer = g_buffers.getCurrentBuffer();
  uikit::drawBtStateIcon(buffer, n, n, 0, 0, uikit::headerStatus.icon,
                      uikit::headerStatus.blinkOn);
  display->pushWindow(uikit::BT_ICON_X, uikit::BT_ICON_Y, n, n, buffer);
}
