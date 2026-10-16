#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../rendering/ILI9341_GFX.h"
#include "../state/podcast_menu.h"
#include "bt_widgets.hpp"
#include "header.hpp"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// The podcast screens (.agents/openpod-podcasts-design/), drawn one 320x30
// chunk at a time from PodcastMenu, like MenuView:
//   chunk 0     header (title, Bluetooth icon, battery) or the toast
//   chunks 1-7  rows: a 6x6 state mark at x=10, the series number column,
//               the title, the status right-aligned at x=304, and the 2px
//               progress underline of in-progress episodes
// renderChanged() redraws only the chunks whose content hash moved: a
// highlight move repaints two rows, a position save the playing row.
class PodcastView {
public:
  static const int CHUNKS = SCREEN_HEIGHT / CHUNK_HEIGHT;

  explicit PodcastView(HeaderComponent &header) : header(header) {
    invalidate();
  }

  void invalidate() {
    for (int i = 0; i < CHUNKS; i++)
      drawn[i] = 0;
  }
  void invalidateHeader() { drawn[0] = 0; }

  void renderChunk(ILI9341_GFX *display, const PodcastMenu &m, bool playing,
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

  void renderAll(ILI9341_GFX *display, const PodcastMenu &m, bool playing) {
    for (int chunk = 0; chunk < CHUNKS; chunk++)
      renderChunk(display, m, playing, chunk);
  }

  void renderChanged(ILI9341_GFX *display, const PodcastMenu &m,
                     bool playing) {
    for (int chunk = 0; chunk < CHUNKS; chunk++)
      if (drawn[chunk] != hash(m, chunk, playing))
        renderChunk(display, m, playing, chunk);
  }

private:
  HeaderComponent &header;
  uint32_t drawn[CHUNKS];

  static const int STATUS_RIGHT = 304;
  static const int MARK_X = 10;
  static const int TITLE_X = 24;
  static const int NUMBER_W = 28;

  uint32_t hash(const PodcastMenu &m, int chunk, bool playing) const {
    uint32_t h = m.chunkHash(chunk);
    if (chunk == 0) {
      if (uikit::headerStatus.toastActive)
        h ^= 0x9E3779B9u ^ uikit::headerStatus.toastSerial;
      if (playing)
        h ^= 0x7F4A7C15u;
    }
    return h ? h : 1;
  }

  static void fill(uint16_t *buf, int x0, int y0, int x1, int y1,
                   uint16_t color) {
    uikit::fill(buf, SCREEN_WIDTH, CHUNK_HEIGHT, x0, y0, x1, y1, color);
  }

  void compose(uint16_t *buf, const PodcastMenu &m, bool playing, int chunk) {
    if (chunk == 0) {
      header.compose(buf, m.title(), playing, false);
      return;
    }
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, COLOR_BG);
    int i = m.first() + chunk - 1;
    if (i < m.rowCount())
      composeRow(buf, m, i);
    else if (m.empty())
      composeHint(buf, chunk * CHUNK_HEIGHT);
    composeScrollbar(buf, m, chunk * CHUNK_HEIGHT);
  }

  // Filled square (new), outline with the left half filled (in progress),
  // nothing (played).
  static void composeMark(uint16_t *buf, uint8_t mark, uint16_t color) {
    const int x = MARK_X, y = 12;
    if (mark == PodcastState::MARK_NEW) {
      fill(buf, x, y, x + 6, y + 6, color);
    } else if (mark == PodcastState::MARK_PROGRESS) {
      fill(buf, x, y, x + 3, y + 6, color);
      fill(buf, x + 3, y, x + 6, y + 1, color);
      fill(buf, x + 3, y + 5, x + 6, y + 6, color);
      fill(buf, x + 5, y, x + 6, y + 6, color);
    }
  }

  void composeRow(uint16_t *buf, const PodcastMenu &m, int i) {
    PodRow r = m.row(i);
    bool hi = i == m.selected();
    uint16_t bg = hi ? COLOR_ACCENT_DK : COLOR_BG;
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, bg);
    if (hi)
      fill(buf, 0, 0, 3, CHUNK_HEIGHT, COLOR_ACCENT);
    fill(buf, 0, CHUNK_HEIGHT - 1, SCREEN_WIDTH, CHUNK_HEIGHT, COLOR_SEPARATOR);

    bool actionRow = r.kind == PodRow::ACTION || r.kind == PodRow::INFO;
    if (!actionRow)
      composeMark(buf, r.mark, hi ? COLOR_ACCENT_ALT : COLOR_ACCENT);

    // Status, right to left from x=304.
    int x = STATUS_RIGHT;
    if (r.chevron) {
      x -= 4;
      uikit::blitRows(buf, SCREEN_WIDTH, CHUNK_HEIGHT, uikit::CHEVRON_RIGHT, 8,
                      x, 11, hi ? COLOR_TEXT : COLOR_MUTED);
      x -= 8;
    }
    if (r.status[0]) {
      uint16_t color = r.tone == PodRow::TONE_ACCENT
                           ? (hi ? COLOR_ACCENT_ALT : COLOR_ACCENT)
                           : (hi ? COLOR_TEXT : COLOR_MUTED);
      int w = fontRenderer.measureText(r.status, IBMPlexSans12);
      x -= w;
      fontRenderer.renderText(r.status, x, 11, IBMPlexSans12, color, bg);
      x -= 8;
    }

    int tx = actionRow ? MARK_X : TITLE_X;
    if (r.number[0]) {
      fontRenderer.renderText(r.number, tx, 11, IBMPlexSans12,
                              hi ? COLOR_TEXT : COLOR_MUTED, bg);
      tx += NUMBER_W + 8;
    }
    uint16_t fg = r.kind == PodRow::INFO ? COLOR_MUTED
                  : hi                   ? COLOR_TEXT_HI
                  : r.dim                ? COLOR_DIM
                                         : COLOR_TEXT;
    char title[96];
    uikit::ellipsize(r.title, title, sizeof(title), IBMPlexSans16, x - tx);
    fontRenderer.renderText(title, tx, 9, IBMPlexSans16, fg, bg);

    // In progress: a 2px underline in place of the row rule.
    if (r.hasProgress) {
      fill(buf, 0, CHUNK_HEIGHT - 2, SCREEN_WIDTH, CHUNK_HEIGHT,
           hi ? COLOR_BG : COLOR_SCROLL_TRACK);
      fill(buf, 0, CHUNK_HEIGHT - 2, r.progress, CHUNK_HEIGHT, COLOR_ACCENT);
    }
  }

  void composeScrollbar(uint16_t *buf, const PodcastMenu &m, int screenY) {
    int total = m.rowCount();
    int visible = PodcastMenu::VISIBLE_ROWS;
    if (total <= visible)
      return;
    const int top = 32, h = 204;
    int thumbH = max(24, (h * visible + total / 2) / total);
    int thumbY = top + (h - thumbH) * m.first() / (total - visible);
    fill(buf, 313, top - screenY, 318, top + h - screenY, COLOR_SCROLL_TRACK);
    fill(buf, 313, thumbY - screenY, 318, thumbY + thumbH - screenY,
         COLOR_ACCENT);
  }

  // No podcasts on the card: two centered lines over the list area.
  void composeHint(uint16_t *buf, int screenY) {
    const int areaTop = CHUNK_HEIGHT;
    int top = areaTop + (SCREEN_HEIGHT - areaTop - 41) / 2 - screenY;
    const char *title = "No podcasts yet";
    const char *sub = "set a podcasts folder in OpenPod Sync";
    int w = fontRenderer.measureText(title, IBMPlexSans16);
    fontRenderer.renderText(title, (SCREEN_WIDTH - w) / 2, top + 4,
                            IBMPlexSans16, COLOR_DIM, COLOR_BG);
    w = fontRenderer.measureText(sub, IBMPlexSans12);
    fontRenderer.renderText(sub, (SCREEN_WIDTH - w) / 2, top + 27,
                            IBMPlexSans12, COLOR_MUTED, COLOR_BG);
  }
};
