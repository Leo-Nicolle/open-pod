#pragma once
#include "../fonts/IBMPlexSansSemiBold36.h"
#include "../fonts/IBMPlexSansSemiBold46.h"
#include "../fonts/IBMPlexSansSemiBold60.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>
#include <string.h>

// Fast-scroll letter overlay (see docs/ui-builder, "Track list"): a 120x90
// box at 100,90 - exactly chunks 3-5 - with a SURFACE fill, a 2px ACCENT
// border inset and r=6 corners, holding the first letters of the selected
// title's fast-scroll key (state/fast_scroll.h; Sans 600 at 60/46/36px for
// 1/2/3 letters).
//
// It is composed into a row's CHUNK_HEIGHT buffer before that row is pushed,
// so the rows under it never flash while scrolling.
class FastScrollOverlay {
public:
  static const int X = 100;
  static const int Y = 90;
  static const int W = 120;
  static const int H = 90;
  static const int RADIUS = 6;
  static const int BORDER = 2;

  // True if the screen strip [y, y + h) overlaps the box.
  static bool overlaps(int y, int h) { return y < Y + H && y + h > Y; }

  // Paints the part of the box that falls in the CHUNK_HEIGHT strip starting
  // at screen y `chunkY`. `buf` is `stride` pixels wide and its column 0 is
  // screen x `bufX0`.
  static void compose(uint16_t *buf, int stride, int bufX0, int chunkY,
                      const char *label) {
    if (!label || !overlaps(chunkY, CHUNK_HEIGHT))
      return;
    int y0 = max(Y, chunkY);
    int y1 = min(Y + H, chunkY + CHUNK_HEIGHT);
    int x0 = max(X, bufX0);
    int x1 = min(X + W, bufX0 + stride);
    if (x0 >= x1)
      return;

    for (int y = y0; y < y1; y++) {
      uint16_t *row = buf + (y - chunkY) * stride - bufX0;
      for (int x = x0; x < x1; x++) {
        int c = boxPixel(x - X, y - Y);
        if (c == OUTSIDE)
          continue;
        row[x] = c == EDGE ? COLOR_ACCENT : COLOR_SURFACE;
      }
    }

    // Label: centred on the cap height (A-Z and #, so the font's baseline
    // is the glyph height). The font has no space: a space in the key ("A D"
    // for "A Day") is a third of the cap height wide. Drawn a letter at a
    // time; renderGlyph clips rows outside this chunk.
    const FastFont &font = fontFor(strlen(label));
    fontRenderer.setBuffer(buf, stride, CHUNK_HEIGHT);
    int gap = font.baseline / 3;
    int tw = 0;
    char one[2] = {0, 0};
    for (const char *c = label; *c; c++) {
      one[0] = *c;
      tw += *c == ' ' ? gap : fontRenderer.measureText(one, font);
    }
    int tx = X + (W - tw) / 2 - bufX0;
    int ty = Y + (H - font.baseline) / 2 - chunkY;
    for (const char *c = label; *c; c++) {
      one[0] = *c;
      if (*c == ' ') {
        tx += gap;
        continue;
      }
      fontRenderer.renderText(one, tx, ty, font, COLOR_TEXT, COLOR_SURFACE);
      tx += fontRenderer.measureText(one, font);
    }
  }

private:
  enum { OUTSIDE, EDGE, FILL };

  static const FastFont &fontFor(size_t letters) {
    if (letters >= 3)
      return IBMPlexSansSemiBold36;
    if (letters == 2)
      return IBMPlexSansSemiBold46;
    return IBMPlexSansSemiBold60;
  }

  // Classifies box-local pixel (lx, ly): rounded corners measured from pixel
  // centres, in doubled coordinates to stay integer.
  static int boxPixel(int lx, int ly) {
    int cx = lx < RADIUS ? RADIUS : lx >= W - RADIUS ? W - RADIUS : -1;
    int cy = ly < RADIUS ? RADIUS : ly >= H - RADIUS ? H - RADIUS : -1;
    if (cx >= 0 && cy >= 0) {
      int dx = 2 * lx + 1 - 2 * cx;
      int dy = 2 * ly + 1 - 2 * cy;
      int d2 = dx * dx + dy * dy; // (2 * distance)^2
      if (d2 > 4 * RADIUS * RADIUS)
        return OUTSIDE;
      if (d2 > 4 * (RADIUS - BORDER) * (RADIUS - BORDER))
        return EDGE;
      return FILL;
    }
    if (lx < BORDER || lx >= W - BORDER || ly < BORDER || ly >= H - BORDER)
      return EDGE;
    return FILL;
  }
};
