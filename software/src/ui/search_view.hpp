#pragma once
#include "../fonts/IBMPlexSans12.h"
#include "../fonts/IBMPlexSans16.h"
#include "../rendering/ILI9341_GFX.h"
#include "../state/search.h"
#include "bt_widgets.hpp"
#include "sprites.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>
#include <string.h>

// The search screen (see docs/ui-builder, "Search"), rendered one
// CHUNK_HEIGHT strip at a time:
//   chunk 0      header: back chevron, magnifier, query + caret, battery
//   chunk 1      letter ribbon (TYPING only)
//   chunks 1/2-7 result rows (7 while browsing, 6 under the ribbon)
// Rows draw their slice of the scrollbar, so a row redraw keeps it right.
class SearchView {
public:
  static const int CHUNKS = SCREEN_HEIGHT / CHUNK_HEIGHT;

  // First chunk of the result list.
  static int listTopChunk(const SearchModel &s) {
    return s.getMode() == SearchModel::TYPING ? 2 : 1;
  }

  // Composes chunk `chunk` and pushes columns [x, x + w) of it to (tx, ty)
  // (default: where the chunk lives on screen).
  void renderChunk(ILI9341_GFX *display, const SearchModel &s, int chunk,
                   int x = 0, int w = SCREEN_WIDTH, int tx = -1, int ty = -1) {
    if (chunk < 0 || chunk >= CHUNKS || w <= 0 || x >= SCREEN_WIDTH)
      return;
    w = min(w, SCREEN_WIDTH - x);
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, SCREEN_WIDTH, CHUNK_HEIGHT);

    int listTop = listTopChunk(s);
    if (chunk == 0)
      composeHeader(buf, s);
    else if (chunk < listTop)
      composeRibbon(buf, s);
    else
      composeRow(buf, s, chunk - listTop, chunk * CHUNK_HEIGHT);

    if (x != 0 || w != SCREEN_WIDTH)
      g_buffers.crop(x, w);
    display->pushWindow(tx < 0 ? x : tx, ty < 0 ? chunk * CHUNK_HEIGHT : ty,
                        w, CHUNK_HEIGHT, buf);
    g_buffers.swapBuffers();
  }

  void renderAll(ILI9341_GFX *display, const SearchModel &s) {
    for (int chunk = 0; chunk < CHUNKS; chunk++)
      renderChunk(display, s, chunk);
  }

  void renderRows(ILI9341_GFX *display, const SearchModel &s) {
    for (int chunk = listTopChunk(s); chunk < CHUNKS; chunk++)
      renderChunk(display, s, chunk);
  }

  // Redraws the row showing hit `hitIndex`, if it's on screen.
  void renderHitRow(ILI9341_GFX *display, const SearchModel &s, int hitIndex) {
    int row = hitIndex - s.getFirst();
    if (row >= 0 && row < s.getCapacity())
      renderChunk(display, s, listTopChunk(s) + row);
  }

private:
  // 1-bit glyphs, '#' = on, drawn in any color.
  static constexpr const char *CHEVRON[11] = {
      "......#", ".....##", "....###", "...####", "..#####", ".######",
      "..#####", "...####", "....###", ".....##", "......#",
  };
  static constexpr const char *MAGNIFIER[12] = {
      "..#####.....", ".##...##....", "##.....##...", "#.......#...",
      "#.......#...", "#.......#...", "##.....##...", ".##...###...",
      "..######....", ".......###..", "........###.", ".........##.",
  };
  // 3x5 caps for the kind chips, 11x5 per label.
  static constexpr const char *KIND_TRK[5] = {
      "###.##..#.#", ".#..#.#.#.#", ".#..##..##.", ".#..#.#.#.#",
      ".#..#.#.#.#"};
  static constexpr const char *KIND_ALB[5] = {
      ".#..#...##.", "#.#.#...#.#", "###.#...##.", "#.#.#...#.#",
      "#.#.###.##."};
  static constexpr const char *KIND_ART[5] = {
      ".#..##..###", "#.#.#.#..#.", "###.##...#.", "#.#.#.#..#.",
      "#.#.#.#..#."};
  static constexpr const char *KIND_GEN[5] = {
      ".##.###.##.", "#...#...#.#", "#.#.##..#.#", "#.#.#...#.#",
      ".##.###.#.#"};

  static void blitRows(uint16_t *buf, const char *const *rows, int h, int x,
                       int y, uint16_t color) {
    for (int ry = 0; ry < h; ry++) {
      int dy = y + ry;
      if (dy < 0 || dy >= CHUNK_HEIGHT)
        continue;
      for (int rx = 0; rows[ry][rx]; rx++) {
        int dx = x + rx;
        if (rows[ry][rx] == '#' && dx >= 0 && dx < SCREEN_WIDTH)
          buf[dy * SCREEN_WIDTH + dx] = color;
      }
    }
  }

  // Fills [x0, x1) x [y0, y1) of the chunk, clipped.
  static void fill(uint16_t *buf, int x0, int y0, int x1, int y1,
                   uint16_t color) {
    x0 = max(x0, 0);
    y0 = max(y0, 0);
    x1 = min(x1, SCREEN_WIDTH);
    y1 = min(y1, CHUNK_HEIGHT);
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++)
        buf[y * SCREEN_WIDTH + x] = color;
  }

  static void outline(uint16_t *buf, int x, int y, int w, int h,
                      uint16_t color) {
    fill(buf, x, y, x + w, y + 1, color);
    fill(buf, x, y + h - 1, x + w, y + h, color);
    fill(buf, x, y, x + 1, y + h, color);
    fill(buf, x + w - 1, y, x + w, y + h, color);
  }

  // Copies `text` into `out`, cut with "..." to fit maxWidth.
  static void ellipsize(const char *text, char *out, size_t outSize,
                        const FastFont &font, int maxWidth) {
    strncpy(out, text, outSize - 1);
    out[outSize - 1] = '\0';
    if (fontRenderer.measureText(out, font) <= maxWidth)
      return;
    char test[64];
    int len = strlen(out);
    while (len > 1) {
      snprintf(test, sizeof(test), "%.*s...", len, out);
      if (fontRenderer.measureText(test, font) <= maxWidth)
        break;
      len--;
    }
    snprintf(test, sizeof(test), "%.*s...", len, out);
    strncpy(out, test, outSize - 1);
    out[outSize - 1] = '\0';
  }

  void composeHeader(uint16_t *buf, const SearchModel &s) {
    if (uikit::headerStatus.toastActive) {
      uikit::drawToast(buf, *uikit::headerStatus.toast);
      return;
    }
    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT - 1, COLOR_SURFACE);
    fill(buf, 0, CHUNK_HEIGHT - 1, SCREEN_WIDTH, CHUNK_HEIGHT, COLOR_LINE);
    blitRows(buf, CHEVRON, 11, 8, 10, COLOR_ACCENT);
    blitRows(buf, MAGNIFIER, 12, 23, 9, COLOR_DIM);

    // The query grows at its end, so a long one keeps its tail in view.
    const int queryX = 42;
    const int maxWidth = uikit::HEADER_TITLE_RIGHT - 8 - queryX; // caret too
    const char *query = s.getQuery();
    char shown[SearchModel::MAX_QUERY + 4];
    strncpy(shown, query, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = '\0';
    while (fontRenderer.measureText(shown, IBMPlexSans16) > maxWidth &&
           *query) {
      query++;
      snprintf(shown, sizeof(shown), "...%s", query);
    }
    int textWidth = fontRenderer.renderText(shown, queryX, 10, IBMPlexSans16,
                                            COLOR_TEXT, COLOR_SURFACE);
    if (s.getMode() == SearchModel::TYPING)
      fill(buf, queryX + textWidth + 1, 8, queryX + textWidth + 3, 23,
           COLOR_ACCENT);

    // Bluetooth icon and battery, as in HeaderComponent::composeChrome
    uikit::drawHeaderRight(buf, false);
  }

  void composeRibbon(uint16_t *buf, const SearchModel &s) {
    uikit::drawRibbon(buf, s.getEntry(), COLOR_DIM);
  }

  void composeRow(uint16_t *buf, const SearchModel &s, int row, int screenY) {
    int hitIndex = s.getFirst() + row;
    bool hasHit = hitIndex < s.getHitCount();
    bool selected = hasHit && s.hasSelection() && hitIndex == s.getSelected();
    uint16_t bg = selected ? COLOR_ACCENT_DK : COLOR_BG;

    fill(buf, 0, 0, SCREEN_WIDTH, CHUNK_HEIGHT, bg);
    if (selected)
      fill(buf, 0, 0, 3, CHUNK_HEIGHT, COLOR_ACCENT);
    else if (hasHit)
      fill(buf, 0, CHUNK_HEIGHT - 1, SCREEN_WIDTH, CHUNK_HEIGHT,
           COLOR_SEPARATOR);

    if (hasHit) {
      uint16_t secondary = selected ? COLOR_TEXT : COLOR_DIM;
      outline(buf, 10, 9, 22, 14, selected ? COLOR_TEXT : COLOR_LINE);
      blitRows(buf, kindLabel(s.getHit(hitIndex).type), 5, 16, 13, secondary);

      char title[SearchModel::TITLE_LEN];
      char sub[SearchModel::SUB_LEN];
      s.describeRow(row, title, sub);
      char line[64];
      ellipsize(title, line, sizeof(line), IBMPlexSans16, 150);
      fontRenderer.renderText(line, 40, 9, IBMPlexSans16, COLOR_TEXT, bg);
      ellipsize(sub, line, sizeof(line), IBMPlexSans12, 108);
      int w = fontRenderer.measureText(line, IBMPlexSans12);
      fontRenderer.renderText(line, 304 - w, 11, IBMPlexSans12, secondary, bg);
    } else if (s.getHitCount() == 0) {
      composeEmptyState(buf, s, screenY);
    }

    composeScrollbar(buf, s, screenY);
  }

  // "No matches" (or a hint for an empty query), centered in the list area
  // and clipped to this chunk.
  void composeEmptyState(uint16_t *buf, const SearchModel &s, int screenY) {
    int listTop = listTopChunk(s) * CHUNK_HEIGHT;
    int cy = listTop + (SCREEN_HEIGHT - listTop) / 2;
    const char *title = s.getQuery()[0] ? "No matches" : "Search";
    const char *hint = s.getQuery()[0] ? "press left to delete a letter"
                                       : "scroll to a letter, click to add it";
    int w = fontRenderer.measureText(title, IBMPlexSans16);
    fontRenderer.renderText(title, (SCREEN_WIDTH - w) / 2, cy - 16 - screenY,
                            IBMPlexSans16, COLOR_TEXT, COLOR_BG);
    w = fontRenderer.measureText(hint, IBMPlexSans12);
    fontRenderer.renderText(hint, (SCREEN_WIDTH - w) / 2, cy + 6 - screenY,
                            IBMPlexSans12, COLOR_DIM, COLOR_BG);
  }

  // Track and thumb at x=313, 5px wide, over the list area.
  void composeScrollbar(uint16_t *buf, const SearchModel &s, int screenY) {
    int total = s.getHitCount();
    int visible = s.getCapacity();
    if (total <= visible)
      return;
    int listTop = listTopChunk(s) * CHUNK_HEIGHT;
    int top = listTop + 2;
    int h = SCREEN_HEIGHT - listTop - 8;
    int thumbH = max(24, h * visible / total);
    int thumbY = top + (h - thumbH) * s.getFirst() / (total - visible);
    fill(buf, 313, top - screenY, 318, top + h - screenY, COLOR_SCROLL_TRACK);
    fill(buf, 313, thumbY - screenY, 318, thumbY + thumbH - screenY,
         COLOR_ACCENT);
  }

  static const char *const *kindLabel(uint8_t type) {
    switch (type) {
    case SEARCH_ALBUM:
      return KIND_ALB;
    case SEARCH_ARTIST:
      return KIND_ART;
    case SEARCH_GENRE:
      return KIND_GEN;
    default:
      return KIND_TRK;
    }
  }
};

constexpr const char *SearchView::CHEVRON[11];
constexpr const char *SearchView::MAGNIFIER[12];
constexpr const char *SearchView::KIND_TRK[5];
constexpr const char *SearchView::KIND_ALB[5];
constexpr const char *SearchView::KIND_ART[5];
constexpr const char *SearchView::KIND_GEN[5];
