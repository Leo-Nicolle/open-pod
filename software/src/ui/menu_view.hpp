#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../fonts/IBMPlexSans16Bold.h"
#include "../fonts/IBMPlexSansSemiBold24.h"
#include "../rendering/ILI9341_GFX.h"
#include "../state/bt_menu.h"
#include "bt_widgets.hpp"
#include "header.hpp"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Settings and the Bluetooth screens (.agents/bluetooth-design/), drawn one
// 320x30 chunk at a time from BtMenu:
//   chunk 0     header (title, Bluetooth icon, battery) or the toast
//   chunks 1-7  rows, a centered hint, the forget text, or the rename
//               ribbon / text / footer
// renderChanged() redraws only the chunks whose content hash moved, so a
// scan tick repaints one row and a highlight move two.
class MenuView {
public:
  static const int CHUNKS = SCREEN_HEIGHT / CHUNK_HEIGHT;

  explicit MenuView(HeaderComponent &header) : header(header) { invalidate(); }

  // Forget what's on screen: the next renderChanged() redraws everything.
  void invalidate() {
    for (int i = 0; i < CHUNKS; i++)
      drawn[i] = 0;
  }
  // The header was repainted elsewhere (toast, icon state).
  void invalidateHeader() { drawn[0] = 0; }

  // Composes chunk `chunk` and pushes columns [x, x + w) of it to (tx, ty)
  // (default: where the chunk lives on screen).
  void renderChunk(ILI9341_GFX *display, const BtMenu &m, bool playing,
                   int chunk, int x = 0, int w = SCREEN_WIDTH, int tx = -1,
                   int ty = -1) {
    if (chunk < 0 || chunk >= CHUNKS || w <= 0 || x >= SCREEN_WIDTH)
      return;
    w = min(w, SCREEN_WIDTH - x);
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, SCREEN_WIDTH, CHUNK_HEIGHT);
    compose(buf, m, playing, chunk);
    if (x != 0 || w != SCREEN_WIDTH)
      g_buffers.crop(x, w);
    display->pushWindow(tx < 0 ? x : tx, ty < 0 ? chunk * CHUNK_HEIGHT : ty,
                        w, CHUNK_HEIGHT, buf);
    g_buffers.swapBuffers();
    if (x == 0 && w == SCREEN_WIDTH && ty < 0)
      drawn[chunk] = hash(m, chunk, playing);
  }

  void renderAll(ILI9341_GFX *display, const BtMenu &m, bool playing) {
    for (int chunk = 0; chunk < CHUNKS; chunk++)
      renderChunk(display, m, playing, chunk);
  }

  void renderChanged(ILI9341_GFX *display, const BtMenu &m, bool playing) {
    for (int chunk = 0; chunk < CHUNKS; chunk++)
      if (drawn[chunk] != hash(m, chunk, playing))
        renderChunk(display, m, playing, chunk);
  }

private:
  HeaderComponent &header;
  uint32_t drawn[CHUNKS];

  static const int STATUS_RIGHT = 304;
  static const int LABEL_X = 10;

  // The header also shows the toast and the play/pause glyph (the
  // Bluetooth icon box repaints on its own). 0 never matches a drawn chunk.
  uint32_t hash(const BtMenu &m, int chunk, bool playing) const {
    uint32_t h = m.chunkHash(chunk);
    if (chunk == 0) {
      if (uikit::headerStatus.toastActive)
        h ^= 0x9E3779B9u;
      if (playing)
        h ^= 0x7F4A7C15u;
    }
    return h ? h : 1;
  }

  static void fill(uint16_t *buf, int x0, int y0, int x1, int y1,
                   uint16_t color) {
    uikit::fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x0, y0, x1, y1, color);
  }

  void compose(uint16_t *buf, const BtMenu &m, bool playing, int chunk) {
    int screenY = chunk * CHUNK_HEIGHT;
    if (chunk == 0) {
      header.compose(buf, m.title(), playing, false);
      return;
    }
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, COLOR_BG);
    switch (m.screen()) {
    case BtMenu::RENAME:
      composeRename(buf, m, chunk, screenY);
      return;
    case BtMenu::FORGET:
      if (chunk < m.listTopChunk()) {
        composeForgetText(buf, m, screenY);
        return;
      }
      break;
    default:
      break;
    }
    int i = m.first() + chunk - m.listTopChunk();
    if (i >= 0 && i < m.rowCount())
      composeRow(buf, m, i);
    else if (m.hint() != BtMenu::HINT_NONE)
      composeHint(buf, m, screenY);
    if (m.screen() != BtMenu::FORGET)
      composeScrollbar(buf, m, screenY);
  }

  // --- Rows --------------------------------------------------------------

  static uint16_t toneColor(uint8_t tone, bool hi) {
    switch (tone) {
    case MenuRow::TONE_ACCENT:
      return hi ? COLOR_ACCENT_ALT : COLOR_ACCENT;
    case MenuRow::TONE_DIMMER:
      return hi ? COLOR_TEXT : COLOR_MUTED;
    case MenuRow::TONE_CREAM:
      return COLOR_ACCENT_ALT;
    case MenuRow::TONE_COUNT:
      return hi ? COLOR_ACCENT_ALT : COLOR_DIM;
    case MenuRow::TONE_DISABLED:
      return COLOR_DISABLED;
    default:
      return hi ? COLOR_TEXT : COLOR_DIM;
    }
  }

  void composeRow(uint16_t *buf, const BtMenu &m, int i) {
    const MenuRow &r = m.row(i);
    bool hi = i == m.selected();
    bool failed = r.kind == MenuRow::FAILED;
    uint16_t bg = failed ? COLOR_ERROR : hi ? COLOR_ACCENT_DK : COLOR_BG;
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, bg);
    if (failed)
      fill(buf, 0, 0, 3, CHUNK_HEIGHT, COLOR_ACCENT_ALT);
    else if (hi)
      fill(buf, 0, 0, 3, CHUNK_HEIGHT, COLOR_ACCENT);
    else
      fill(buf, 0, CHUNK_HEIGHT - 1, SCREEN_WIDTH, CHUNK_HEIGHT,
           COLOR_SEPARATOR);
    bool lit = hi || failed; // on a filled row: the highlighted colors

    // Status, right to left from x=304, 8px apart.
    int x = STATUS_RIGHT;
    switch (r.trail) {
    case MenuRow::TRAIL_CHEVRON:
      x -= 4;
      uikit::blitRows(buf, SCREEN_WIDTH, CHUNK_HEIGHT, uikit::CHEVRON_RIGHT, 8, x, 11,
                   lit ? COLOR_TEXT : COLOR_MUTED);
      x -= 8;
      break;
    case MenuRow::TRAIL_SWITCH_ON:
    case MenuRow::TRAIL_SWITCH_OFF:
      x -= 22;
      uikit::drawSwitch(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x, 9,
                     r.trail == MenuRow::TRAIL_SWITCH_ON);
      x -= 8;
      break;
    }
    if (r.bars) {
      x -= 11;
      uikit::drawBars(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x, 10, r.bars, lit);
      x -= 8;
    }
    if (r.value[0]) {
      int w = fontRenderer.measureText(r.value, IBMPlexSans12);
      x -= w;
      fontRenderer.renderText(r.value, x, 11, IBMPlexSans12,
                              toneColor(r.valueTone, lit), bg);
      x -= 8;
    }
    // The note gives way to the label: at most what's left after it.
    int labelW = fontRenderer.measureText(r.label, IBMPlexSans16);
    if (r.dots)
      labelW += 6 + 15;
    if (r.note && r.note[0]) {
      int room = x - (LABEL_X + min(labelW, 120) + 8);
      char note[48];
      uikit::ellipsize(r.note, note, sizeof(note), IBMPlexSans12, max(room, 0));
      int w = fontRenderer.measureText(note, IBMPlexSans12);
      x -= w;
      fontRenderer.renderText(note, x, 11, IBMPlexSans12,
                              toneColor(r.noteTone, lit), bg);
      x -= 8;
    }

    uint16_t fg;
    switch (r.kind) {
    case MenuRow::READONLY:
      fg = COLOR_MUTED;
      break;
    case MenuRow::DISABLED:
      fg = COLOR_DISABLED;
      break;
    case MenuRow::DESTRUCTIVE:
      fg = COLOR_DESTRUCTIVE;
      break;
    default:
      fg = lit ? COLOR_TEXT_HI : COLOR_TEXT;
      break;
    }
    char label[48];
    int labelRoom = x - LABEL_X - (r.dots ? 6 + 15 : 0);
    uikit::ellipsize(r.label, label, sizeof(label), IBMPlexSans16, labelRoom);
    int w = fontRenderer.renderText(label, LABEL_X, 9, IBMPlexSans16, fg, bg);
    if (r.dots)
      uikit::drawDots(buf, SCREEN_WIDTH, CHUNK_HEIGHT, LABEL_X + w + 6,
                   9 + IBMPlexSans16.baseline - 3, r.dots);

    if (r.progress) {
      fill(buf, 0, CHUNK_HEIGHT - 3, SCREEN_WIDTH, CHUNK_HEIGHT - 1, COLOR_BG);
      fill(buf, 0, CHUNK_HEIGHT - 3, r.progress, CHUNK_HEIGHT - 1,
           COLOR_ACCENT);
    }
  }

  // Track and thumb at x=313 over the rows, when there are more than 7.
  void composeScrollbar(uint16_t *buf, const BtMenu &m, int screenY) {
    int total = m.rowCount();
    int visible = BtMenu::VISIBLE_ROWS;
    if (total <= visible)
      return;
    const int top = 32, h = 204;
    int thumbH = max(24, (h * visible + total / 2) / total);
    int thumbY = top + (h - thumbH) * m.first() / (total - visible);
    fill(buf, 313, top - screenY, 318, top + h - screenY, COLOR_SCROLL_TRACK);
    fill(buf, 313, thumbY - screenY, 318, thumbY + thumbH - screenY,
         COLOR_ACCENT);
  }

  // Two centered lines over the empty part of the list.
  void composeHint(uint16_t *buf, const BtMenu &m, int screenY) {
    bool off = m.hint() == BtMenu::HINT_RADIO_OFF;
    int areaTop = (off ? 2 : 3) * CHUNK_HEIGHT;
    const char *title = off ? "Bluetooth is off" : "No devices yet";
    const char *sub = off ? "paired devices are kept" : "scan to find headphones";
    // 16px line (21) + 6 + 12px line (14)
    int top = areaTop + (SCREEN_HEIGHT - areaTop - 41) / 2 - screenY;
    int w = fontRenderer.measureText(title, IBMPlexSans16);
    fontRenderer.renderText(title, (SCREEN_WIDTH - w) / 2, top + 4,
                            IBMPlexSans16, COLOR_DIM, COLOR_BG);
    w = fontRenderer.measureText(sub, IBMPlexSans12);
    fontRenderer.renderText(sub, (SCREEN_WIDTH - w) / 2, top + 27,
                            IBMPlexSans12, COLOR_MUTED, COLOR_BG);
  }

  // --- Forget confirmation -------------------------------------------------

  // Chunks 1-3: the device name, then the explanation wrapped to the width.
  void composeForgetText(uint16_t *buf, const BtMenu &m, int screenY) {
    int y = CHUNK_HEIGHT + 14 - screenY;
    char name[48];
    uikit::ellipsize(m.deviceName(), name, sizeof(name), IBMPlexSans16Bold,
                  SCREEN_WIDTH - 26);
    fontRenderer.renderText(name, LABEL_X, y, IBMPlexSans16Bold, COLOR_TEXT,
                            COLOR_BG);
    y += 16 + 6 + 4;
    static const char *const BODY =
        "will be unpaired. To use it again, pair it from a new scan.";
    const int maxW = SCREEN_WIDTH - LABEL_X - 16;
    const char *p = BODY;
    char line[64];
    while (*p) {
      // Longest run of whole words that fits.
      int len = 0, fit = 0;
      while (p[len]) {
        int end = len;
        while (p[end] && p[end] != ' ')
          end++;
        snprintf(line, sizeof(line), "%.*s", end, p);
        if (fontRenderer.measureText(line, IBMPlexSans12) > maxW && fit)
          break;
        fit = end;
        len = p[end] ? end + 1 : end;
      }
      snprintf(line, sizeof(line), "%.*s", fit, p);
      fontRenderer.renderText(line, LABEL_X, y, IBMPlexSans12, COLOR_DIM,
                              COLOR_BG);
      y += 17;
      p += len;
    }
  }

  // --- Rename ----------------------------------------------------------------

  void composeRename(uint16_t *buf, const BtMenu &m, int chunk, int screenY) {
    const TextEntry &e = m.entry();
    if (chunk == 1) {
      uikit::drawRibbon(buf, e, COLOR_MUTED);
      return;
    }
    if (chunk == CHUNKS - 1) {
      composeFooter(buf);
      return;
    }
    if (chunk > 5)
      return;

    // y 60-179: the text (30px line) + 8 + the hint line (14), centered.
    const int blockTop = 2 * CHUNK_HEIGHT + (120 - 52) / 2;
    const int maxW = SCREEN_WIDTH - 24 - 4;
    // A long name keeps its end (and the caret) in view.
    const char *text = e.text();
    char shown[TextEntry::CAPACITY + 4];
    snprintf(shown, sizeof(shown), "%s", text);
    while (fontRenderer.measureText(shown, IBMPlexSansSemiBold24) > maxW &&
           *text) {
      text++;
      snprintf(shown, sizeof(shown), "...%s", text);
    }
    int y = blockTop + 3 - screenY;
    int w = fontRenderer.renderText(shown, 12, y, IBMPlexSansSemiBold24,
                                    COLOR_TEXT, COLOR_BG);
    fill(buf, 12 + w + 2, y, 12 + w + 4, y + 24, COLOR_ACCENT);

    int hy = blockTop + 30 + 8 - screenY;
    char hint[64];
    char count[8];
    snprintf(count, sizeof(count), "%d/%d", e.length(), e.maxLength());
    int countW = fontRenderer.measureText(count, IBMPlexSans12);
    char quoted[48];
    snprintf(quoted, sizeof(quoted), "empty name restores \"%s\"",
             m.broadcastName());
    uikit::ellipsize(quoted, hint, sizeof(hint), IBMPlexSans12,
                  SCREEN_WIDTH - 24 - countW - 8);
    fontRenderer.renderText(hint, 12, hy, IBMPlexSans12, COLOR_MUTED, COLOR_BG);
    fontRenderer.renderText(count, SCREEN_WIDTH - 12 - countW, hy,
                            IBMPlexSans12, COLOR_MUTED, COLOR_BG);
  }

  // MENU cancel · BACK delete · NEXT space · PLAY save, spread evenly.
  void composeFooter(uint16_t *buf) {
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, COLOR_RIBBON);
    fill(buf, 0, 0, SCREEN_WIDTH, 1, COLOR_LINE);
    static const char *const TAGS[4] = {"MENU", "BACK", "NEXT", "PLAY"};
    static const char *const LABELS[4] = {"cancel", "delete", "space", "save"};
    int widths[4], total = 0;
    for (int i = 0; i < 4; i++) {
      widths[i] = (int)strlen(TAGS[i]) * 4 - 1 + 6 + 4 +
                  fontRenderer.measureText(LABELS[i], IBMPlexSans12);
      total += widths[i];
    }
    int gap = (SCREEN_WIDTH - 16 - total) / 3;
    int x = 8;
    for (int i = 0; i < 4; i++) {
      int tw = uikit::drawTag(buf, SCREEN_WIDTH, CHUNK_HEIGHT, TAGS[i], x, 10,
                           COLOR_DIM, COLOR_LINE);
      fontRenderer.renderText(LABELS[i], x + tw + 4, 9, IBMPlexSans12,
                              COLOR_DIM, COLOR_RIBBON);
      x += widths[i] + gap;
    }
  }
};
