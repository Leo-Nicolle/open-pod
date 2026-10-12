#pragma once
#include "../bluetooth/BtModel.h"
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../fonts/IBMPlexSans16Bold.h"
#include "../state/text_entry.h"
#include "sprites.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Small pieces shared by the header, the toast, the search screen and the
// Settings/Bluetooth screens (.agents/bluetooth-design/.../README.md),
// drawn into an RGB565 buffer `bw` pixels wide and `bh` high (usually one
// 320x30 chunk). Coordinates are in the buffer; everything clips.
namespace uikit {

// The Bluetooth state the header shows on every screen, and the toast that
// takes over the header for a moment.
struct HeaderStatus {
  BtModel::Icon icon = BtModel::ICON_ON;
  bool blinkOn = false;          // "connecting" alternates every 500 ms
  const char *codec = nullptr;   // Now Playing tag, nullptr when hidden
  bool toastActive = false;
  const BtModel::Toast *toast = nullptr; // BtModel's, while toastActive
};
extern HeaderStatus headerStatus;

// The header's 11x11 Bluetooth icon box (fixed, so a state change repaints
// just this box).
static const int BT_ICON_X = 270;
static const int BT_ICON_Y = 10;
static const int BT_ICON_SIZE = 11;
// Header titles end before the icon box.
static const int HEADER_TITLE_RIGHT = BT_ICON_X - 8;

inline void fill(uint16_t *buf, int bw, int bh, int x0, int y0, int x1,
                 int y1, uint16_t color) {
  x0 = max(x0, 0);
  y0 = max(y0, 0);
  x1 = min(x1, bw);
  y1 = min(y1, bh);
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      buf[y * bw + x] = color;
}

inline void outline(uint16_t *buf, int bw, int bh, int x, int y, int w,
                    int h, uint16_t color) {
  fill(buf, bw, bh, x, y, x + w, y + 1, color);
  fill(buf, bw, bh, x, y + h - 1, x + w, y + h, color);
  fill(buf, bw, bh, x, y, x + 1, y + h, color);
  fill(buf, bw, bh, x + w - 1, y, x + w, y + h, color);
}

// 1-bit pattern, '#' = on.
inline void blitRows(uint16_t *buf, int bw, int bh, const char *const *rows,
                     int h, int x, int y, uint16_t color) {
  for (int ry = 0; ry < h; ry++) {
    int dy = y + ry;
    if (dy < 0 || dy >= bh)
      continue;
    for (int rx = 0; rows[ry][rx]; rx++) {
      int dx = x + rx;
      if (rows[ry][rx] == '#' && dx >= 0 && dx < bw)
        buf[dy * bw + dx] = color;
    }
  }
}

// Copies `text` into `out`, cut with "..." to fit maxWidth.
inline void ellipsize(const char *text, char *out, size_t outSize,
                      const FastFont &font, int maxWidth) {
  strncpy(out, text, outSize - 1);
  out[outSize - 1] = '\0';
  if (fontRenderer.measureText(out, font) <= maxWidth)
    return;
  char test[72];
  int len = strlen(out);
  while (len > 0) {
    snprintf(test, sizeof(test), "%.*s...", len, out);
    if (fontRenderer.measureText(test, font) <= maxWidth)
      break;
    len--;
  }
  snprintf(test, sizeof(test), "%.*s...", len, out);
  strncpy(out, test, outSize - 1);
  out[outSize - 1] = '\0';
}

// Bluetooth rune, 7x11.
static const char *const BT_RUNE[11] = {
    "...#...", "...##..", ".#.#.#.", "..##..#", "...#.#.", "...##..",
    "...#.#.", "..##..#", ".#.#.#.", "...##..", "...#...",
};

// The 11x11 icon at (x, y): rune at x+2, plus the two "connected" ticks.
// `bg` clears the box first.
inline void drawBtIcon(uint16_t *buf, int bw, int bh, int x, int y,
                       uint16_t rune, bool ticks, uint16_t tickColor,
                       uint16_t bg) {
  fill(buf, bw, bh, x, y, x + BT_ICON_SIZE, y + BT_ICON_SIZE, bg);
  blitRows(buf, bw, bh, BT_RUNE, 11, x + 2, y, rune);
  if (ticks) {
    fill(buf, bw, bh, x, y + 4, x + 2, y + 7, tickColor);
    fill(buf, bw, bh, x + 9, y + 4, x + 11, y + 7, tickColor);
  }
}

// The header icon for a state (README §1 table).
inline void drawBtStateIcon(uint16_t *buf, int bw, int bh, int x, int y,
                            BtModel::Icon icon, bool blinkOn) {
  bool connected = icon == BtModel::ICON_CONNECTED ||
                   (icon == BtModel::ICON_CONNECTING && blinkOn);
  uint16_t rune = icon == BtModel::ICON_OFF ? COLOR_MUTED
                  : connected               ? COLOR_ACCENT
                                            : COLOR_TEXT;
  drawBtIcon(buf, bw, bh, x, y, rune, connected, COLOR_ACCENT, COLOR_SURFACE);
}

// Signal bars, 11x10 at (x, y): 3px bars, 1px gap, 4/7/10 tall.
inline void drawBars(uint16_t *buf, int bw, int bh, int x, int y, int bars,
                     bool highlighted) {
  static const int HEIGHTS[3] = {4, 7, 10};
  for (int i = 0; i < 3; i++) {
    uint16_t c = i < bars ? (highlighted ? COLOR_TEXT : COLOR_DIM)
                          : (highlighted ? COLOR_BAR_OFF_HI : COLOR_BAR_OFF);
    fill(buf, bw, bh, x + i * 4, y + 10 - HEIGHTS[i], x + i * 4 + 3, y + 10, c);
  }
}

// Right-pointing chevron, 4x8.
static const char *const CHEVRON_RIGHT[8] = {
    "#...", "##..", "###.", "####", "####", "###.", "##..", "#...",
};

// 22x12 switch at (x, y), radius 6, with an 8x8 knob.
inline void drawSwitch(uint16_t *buf, int bw, int bh, int x, int y, bool on) {
  const int w = 22, h = 12, r = 6;
  // Inside the rounded rectangle (inset by `in` px)?
  auto inside = [&](int px, int py, int in) {
    int cx = constrain(px, r, w - 1 - r);
    int cy = constrain(py, r, h - 1 - r);
    int dx = 2 * (px - cx), dy = 2 * (py - cy);
    int rr = 2 * (r - in) - 1;
    return px >= in && px < w - in && py >= in && py < h - in &&
           dx * dx + dy * dy <= rr * rr;
  };
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      int sx = x + px, sy = y + py;
      if (sx < 0 || sx >= bw || sy < 0 || sy >= bh || !inside(px, py, 0))
        continue;
      if (on)
        buf[sy * bw + sx] = COLOR_ACCENT;
      else if (!inside(px, py, 1))
        buf[sy * bw + sx] = COLOR_DIM; // 1px border
    }
  }
  // Knob: an 8x8 circle, 2px from the side.
  int kx = on ? x + w - 2 - 8 : x + 2;
  int ky = y + 2;
  for (int py = 0; py < 8; py++)
    for (int px = 0; px < 8; px++) {
      int dx = 2 * px - 7, dy = 2 * py - 7;
      if (dx * dx + dy * dy <= 64)
        fill(buf, bw, bh, kx + px, ky + py, kx + px + 1, ky + py + 1,
             on ? COLOR_BG : COLOR_DIM);
    }
}

// Three 3x3 dots with a 3px gap; the first `lit` are on.
inline void drawDots(uint16_t *buf, int bw, int bh, int x, int y, int lit) {
  for (int i = 0; i < 3; i++)
    fill(buf, bw, bh, x + i * 6, y, x + i * 6 + 3, y + 3,
         i < lit ? COLOR_TEXT_HI : COLOR_BAR_OFF_HI);
}

// Tiny 3x5 caps for the footer tags (MENU, BACK, NEXT, PLAY).
inline const char *const *tinyGlyph(char c) {
  static const char *const A[5] = {".#.", "#.#", "###", "#.#", "#.#"};
  static const char *const B[5] = {"##.", "#.#", "##.", "#.#", "##."};
  static const char *const C[5] = {".##", "#..", "#..", "#..", ".##"};
  static const char *const E[5] = {"###", "#..", "##.", "#..", "###"};
  static const char *const K[5] = {"#.#", "#.#", "##.", "#.#", "#.#"};
  static const char *const L[5] = {"#..", "#..", "#..", "#..", "###"};
  static const char *const M[5] = {"#.#", "###", "###", "#.#", "#.#"};
  static const char *const N[5] = {"#.#", "###", "###", "###", "#.#"};
  static const char *const P[5] = {"##.", "#.#", "##.", "#..", "#.."};
  static const char *const T[5] = {"###", ".#.", ".#.", ".#.", ".#."};
  static const char *const U[5] = {"#.#", "#.#", "#.#", "#.#", "###"};
  static const char *const X[5] = {"#.#", "#.#", ".#.", "#.#", "#.#"};
  static const char *const Y[5] = {"#.#", "#.#", ".#.", ".#.", ".#."};
  switch (c) {
  case 'A': return A;
  case 'B': return B;
  case 'C': return C;
  case 'E': return E;
  case 'K': return K;
  case 'L': return L;
  case 'M': return M;
  case 'N': return N;
  case 'P': return P;
  case 'T': return T;
  case 'U': return U;
  case 'X': return X;
  case 'Y': return Y;
  default: return nullptr;
  }
}

// A key tag (1px border) at (x, y), 12px tall. Returns its width.
inline int drawTag(uint16_t *buf, int bw, int bh, const char *caps, int x,
                   int y, uint16_t color, uint16_t border) {
  int w = (int)strlen(caps) * 4 - 1 + 6;
  outline(buf, bw, bh, x, y, w, 12, border);
  for (int i = 0; caps[i]; i++) {
    const char *const *g = tinyGlyph(caps[i]);
    if (g)
      blitRows(buf, bw, bh, g, 5, x + 3 + i * 4, y + 4, color);
  }
  return w;
}

// The battery, as in HeaderComponent.
inline void drawBattery(uint16_t *buf, int bw, int bh) {
  blitSprite(buf, bw, bh, SPR_BATTERY, 290, 9);
  fill(buf, bw, bh, 293, 12, 297, 17, COLOR_TEXT);
  fill(buf, bw, bh, 298, 12, 302, 17, COLOR_TEXT);
  fill(buf, bw, bh, 303, 12, 307, 17, COLOR_LINE);
}

// The header's right group: Bluetooth icon (and the Now Playing codec tag
// left of it), then the battery.
inline void drawHeaderRight(uint16_t *buf, bool showCodec) {
  const HeaderStatus &s = headerStatus;
  drawBtStateIcon(buf, SCREEN_WIDTH, CHUNK_HEIGHT, BT_ICON_X, BT_ICON_Y,
                  s.icon, s.blinkOn);
  if (showCodec && s.codec) {
    int w = fontRenderer.measureText(s.codec, IBMPlexSans12) + 6;
    int x = BT_ICON_X - 4 - w;
    outline(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x, 8, w, 15, COLOR_LINE);
    fontRenderer.renderText(s.codec, x + 3, 10, IBMPlexSans12, COLOR_DIM,
                            COLOR_SURFACE);
  }
  drawBattery(buf, SCREEN_WIDTH, CHUNK_HEIGHT);
}

// Where header titles must stop (before the codec tag on Now Playing).
inline int headerTitleRight(bool showCodec) {
  const HeaderStatus &s = headerStatus;
  if (showCodec && s.codec)
    return BT_ICON_X - 4 - (fontRenderer.measureText(s.codec, IBMPlexSans12) +
                            6) - 8;
  return HEADER_TITLE_RIGHT;
}

// Text with the device name bold and "\xB7" drawn as a middle dot.
inline int drawToastText(uint16_t *buf, const char *text, int len, int x,
                         const FastFont &font, uint16_t color, uint16_t bg) {
  char part[BtModel::TOAST_LEN];
  int start = 0;
  for (int i = 0; i <= len; i++) {
    if (i < len && text[i] != '\xB7')
      continue;
    int n = min(i - start, (int)sizeof(part) - 1);
    memcpy(part, text + start, n);
    part[n] = '\0';
    x += fontRenderer.renderText(part, x, 10, font, color, bg);
    if (i < len) { // the dot
      fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x + 2, 16, x + 4, 18, color);
      x += 6;
    }
    start = i + 1;
  }
  return x;
}

inline int measureToastPart(const char *text, int len, const FastFont &font) {
  char part[BtModel::TOAST_LEN];
  int n = min(len, (int)sizeof(part) - 1);
  memcpy(part, text, n);
  part[n] = '\0';
  int w = 0;
  for (int i = 0; i < n; i++)
    if (part[i] == '\xB7')
      w += 6;
  return w + fontRenderer.measureText(part, font);
}

// The toast over the whole header chunk (README §4).
inline void drawToast(uint16_t *buf, const BtModel::Toast &t) {
  uint16_t bg, fg;
  switch (t.tone) {
  case BtModel::TOAST_SUCCESS:
    bg = COLOR_ACCENT_DK;
    fg = COLOR_TEXT_HI;
    break;
  case BtModel::TOAST_ERROR:
    bg = COLOR_ERROR;
    fg = COLOR_TEXT_HI;
    break;
  default:
    bg = COLOR_LINE;
    fg = COLOR_TEXT;
    break;
  }
  fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, bg);
  if (t.tone == BtModel::TOAST_SUCCESS) {
    fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 0, 28, SCREEN_WIDTH, 30,
         COLOR_ACCENT);
    if (!t.plain)
      drawBtIcon(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 8, 10, COLOR_ACCENT_ALT, true,
                 COLOR_ACCENT_ALT, bg);
  } else if (!t.plain) {
    drawBtIcon(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 8, 10, COLOR_TEXT, false,
               COLOR_TEXT, bg);
  }

  fontRenderer.setBuffer(buf, SCREEN_WIDTH, CHUNK_HEIGHT);
  const int x0 = t.plain ? 8 : 27, maxW = SCREEN_WIDTH - 8 - x0;
  int len = strlen(t.text);
  int ns = min((int)t.nameStart, len);
  int nl = min((int)t.nameLen, len - ns);
  const char *after = t.text + ns + nl;
  int afterLen = len - ns - nl;
  int fixedW = measureToastPart(t.text, ns, IBMPlexSans16) +
               measureToastPart(after, afterLen, IBMPlexSans16);
  char name[BtModel::TOAST_LEN];
  memcpy(name, t.text + ns, nl);
  name[nl] = '\0';
  char shown[BtModel::TOAST_LEN + 4];
  ellipsize(name, shown, sizeof(shown), IBMPlexSans16Bold,
            max(0, maxW - fixedW));
  int x = drawToastText(buf, t.text, ns, x0, IBMPlexSans16, fg, bg);
  x += fontRenderer.renderText(shown, x, 10, IBMPlexSans16Bold, fg, bg);
  drawToastText(buf, after, afterLen, x, IBMPlexSans16, fg, bg);
}

// The letter ribbon (search and rename), one 320x30 chunk: 13 cells of
// 19x20 with a 2px gap. A clamped ribbon keeps the cursor centered until it
// nears either end; a wrapping one always centers it.
inline void drawRibbon(uint16_t *buf, const TextEntry &e, uint16_t idle) {
  fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT - 1,
       COLOR_RIBBON);
  fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, 0, CHUNK_HEIGHT - 1, SCREEN_WIDTH,
       CHUNK_HEIGHT, COLOR_LINE);
  fontRenderer.setBuffer(buf, SCREEN_WIDTH, CHUNK_HEIGHT);
  const int cells = 13, cellW = 19, cellH = 20, gap = 2, cellY = 5;
  const int x0 = (SCREEN_WIDTH - (cells * cellW + (cells - 1) * gap)) / 2;
  int n = e.charCount();
  int start = e.wraps() ? e.cursor() - cells / 2
                        : constrain(e.cursor() - cells / 2, 0, n - cells);
  for (int i = 0; i < cells; i++) {
    int index = start + i;
    if (e.wraps())
      index = ((index % n) + n) % n;
    else if (index < 0 || index >= n)
      continue;
    int x = x0 + i * (cellW + gap);
    bool on = index == e.cursor();
    uint16_t bg = on ? COLOR_ACCENT : COLOR_RIBBON;
    if (on)
      fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x, cellY, x + cellW, cellY + cellH,
           bg);
    char letter[2] = {e.chars()[index], '\0'};
    int w = fontRenderer.measureText(letter, IBMPlexSans12);
    fontRenderer.renderText(letter, x + (cellW - w) / 2,
                            cellY + (cellH - IBMPlexSans12.baseline) / 2,
                            IBMPlexSans12,
                            on ? COLOR_BG : idle,
                            bg);
  }
}

} // namespace uikit
