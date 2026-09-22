#pragma once
#include "fonts/IBMPlexSans12.h"
#include "fonts/IBMPlexSans16.h"
#include "rendering/font_renderer.h"
#include "sprites.h"
#include "storage/AlbumArt.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

class ILI9341_GFX;

// Fill a rounded rectangle (radius r) into a caller-owned RGB565 buffer,
// clipped to the buffer. Used for the progress/seek/volume bars; the SDF
// formulation handles full capsules (r == h/2) as well as plain rounded ends.
static inline void fillRoundedRect(uint16_t *buf, int bufW, int bufH, int x,
                                   int y, int w, int h, int r,
                                   uint16_t color) {
  if (r <= 0) {
    for (int py = y; py < y + h; py++) {
      if (py < 0 || py >= bufH)
        continue;
      for (int px = x; px < x + w; px++) {
        if (px < 0 || px >= bufW)
          continue;
        buf[py * bufW + px] = color;
      }
    }
    return;
  }
  const float cx = x + (w - 1) / 2.0f;
  const float cy = y + (h - 1) / 2.0f;
  const float bx = w / 2.0f - r;
  const float by = h / 2.0f - r;
  for (int py = y; py < y + h; py++) {
    if (py < 0 || py >= bufH)
      continue;
    const float fy = py + 0.5f;
    for (int px = x; px < x + w; px++) {
      if (px < 0 || px >= bufW)
        continue;
      const float fx = px + 0.5f;
      float qx = fabsf(fx - cx) - bx;
      float qy = fabsf(fy - cy) - by;
      float e = sqrtf(fmaxf(qx, 0.0f) * fmaxf(qx, 0.0f) +
                      fmaxf(qy, 0.0f) * fmaxf(qy, 0.0f)) +
                fminf(fmaxf(qx, qy), 0.0f) - r;
      if (e <= 0.0f)
        buf[py * bufW + px] = color;
    }
  }
}

class NowPlayingComponent {
private:
  const char *currentTrack;
  char albumName[64]; // resolved by the engine via MusicLookup
  bool isPlaying;
  int trackLength; // seconds
  int progress;    // seconds
  bool seekModeActive;
  int volumeLevel; // 0-100
  bool volumeOverlayVisible;
  uint32_t coverOffset;
  uint32_t coverLength;
  uint8_t coverFormat; // 1 = raw565 (see indexer's ALBUM_COVER_FORMAT_BYTE)
  bool coverValid;

  // Layout constants (dark theme: art on the left, metadata on the right,
  // every animated element in the bottom 30px chunk). Matches
  // docs/ui-builder's box spec: album art 8,40 160x160 (largest square that
  // leaves a readable 132px metadata column); title 180,40 132x32; album
  // 180,96.
  static const int ALBUM_ART_X = 8;
  static const int ALBUM_ART_Y = 40;
  static const int ALBUM_ART_SIZE = 160; // matches the generated thumbnail size
  static const int METADATA_X = 180;
  static const int TITLE_Y = 40;
  static const int ALBUM_Y = 96;
  static const int BOTTOM_BAND_Y = 210; // one CHUNK_HEIGHT boundary (7 * 30)

  // Bottom band geometry.
  static const int BAR_X = 8;
  static const int BAR_W = 304; // 8 .. 312 (scrollbar starts at x = 312)

public:
  NowPlayingComponent();

  void setTrack(const char *trackName) { currentTrack = trackName; }
  void setAlbumName(const char *name) {
    if (!name) {
      albumName[0] = '\0';
      return;
    }
    strncpy(albumName, name, sizeof(albumName) - 1);
    albumName[sizeof(albumName) - 1] = '\0';
  }
  void setProgress(int p) { progress = p; }
  void setTrackLength(int tl) { trackLength = tl; }
  void setPlayState(bool playing) { isPlaying = playing; }
  void setSeekModeActive(bool active) { seekModeActive = active; }
  void setVolumeOverlay(int level, bool visible) {
    volumeLevel = level;
    volumeOverlayVisible = visible;
  }
  void setAlbumArt(uint32_t offset, uint32_t length, uint8_t format) {
    coverOffset = offset;
    coverLength = length;
    coverFormat = format;
    coverValid = length > 0;
  }
  void clearAlbumArt() { coverValid = false; }

  // Render the volume band (the bottom chunk shows the volume overlay).
  void renderVolumeOverlay(ILI9341_GFX *display);

  // Render one 30px chunk row [y, y+CHUNK_HEIGHT) for x in [x, x+width).
  void renderChunk(ILI9341_GFX *display, int x, int y, int width,
                   int tx = -1, int ty = -1);
  void render(ILI9341_GFX *display, int x, int width);

  // Re-render only the bottom band (progress/seek/volume) - the cheap path
  // taken on each progress tick.
  void updateNowPlaying(ILI9341_GFX *display);

private:
  void renderBodyChunk(ILI9341_GFX *display, int x, int y, int width, int tx,
                       int ty);
  void renderBandChunk(ILI9341_GFX *display, int x, int width, int tx, int ty);
};

// --- implementation ----------------------------------------------------------

NowPlayingComponent::NowPlayingComponent()
    : currentTrack(""), isPlaying(false), trackLength(0), progress(0),
      seekModeActive(false), volumeLevel(0), volumeOverlayVisible(false),
      coverOffset(0), coverLength(0), coverFormat(0), coverValid(false) {
  albumName[0] = '\0';
}

void NowPlayingComponent::render(ILI9341_GFX *display, int x, int width) {
  for (int y = BODY_Y; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT) {
    renderChunk(display, x, y, width);
  }
}

void NowPlayingComponent::renderChunk(ILI9341_GFX *display, int x, int y,
                                      int width, int tx, int ty) {
  if (y >= BOTTOM_BAND_Y) {
    renderBandChunk(display, x, width, tx, ty);
  } else {
    renderBodyChunk(display, x, y, width, tx, ty);
  }
}

void NowPlayingComponent::renderBodyChunk(ILI9341_GFX *display, int x, int y,
                                          int width, int tx, int ty) {
  if (y >= SCREEN_HEIGHT)
    return;
  int actualHeight =
      (y + CHUNK_HEIGHT > SCREEN_HEIGHT) ? (SCREEN_HEIGHT - y) : CHUNK_HEIGHT;
  if (x >= SCREEN_WIDTH || width <= 0)
    return;
  int actualWidth = (width < SCREEN_WIDTH - x) ? width : (SCREEN_WIDTH - x);
  uint16_t *buffer = g_buffers.getCurrentBuffer();

  const int artTop = ALBUM_ART_Y;
  const int artBottom = ALBUM_ART_Y + ALBUM_ART_SIZE;
  const int artLeft = ALBUM_ART_X;
  const int artRight = ALBUM_ART_X + ALBUM_ART_SIZE;
  const int chunkTop = y;
  const int chunkBottom = y + actualHeight;

  // Background.
  for (int i = 0; i < actualWidth * actualHeight; i++) {
    buffer[i] = COLOR_BG;
  }

  // Album art (if it overlaps this chunk and the x-range).
  int overlapTop = max(chunkTop, artTop);
  int overlapBottom = min(chunkBottom, artBottom);
  int overlapLeft = max(x, artLeft);
  int overlapRight = min(x + actualWidth, artRight);

  if (overlapTop < overlapBottom && overlapLeft < overlapRight) {
    bool blitted = false;
    if (coverValid && coverFormat == 1 && overlapLeft == artLeft &&
        overlapRight == artRight) {
      int destRow0 = overlapTop - y;
      int destCol0 = artLeft - x;
      int coverRowStart = overlapTop - artTop;
      int rowCount = overlapBottom - overlapTop;
      blitted = albumArt.blitCoverRows(buffer, actualWidth, destRow0, destCol0,
                                       coverOffset, coverLength,
                                       coverRowStart, rowCount);
    }
    if (!blitted) {
      // Solid art-plate placeholder.
      for (int row = overlapTop; row < overlapBottom; ++row) {
        int bufRow = row - y;
        for (int col = overlapLeft; col < overlapRight; ++col) {
          int bufCol = col - x;
          if (bufCol >= 0 && bufCol < actualWidth && bufRow >= 0 &&
              bufRow < actualHeight) {
            buffer[bufRow * actualWidth + bufCol] = COLOR_SURFACE;
          }
        }
      }
    }

    // 1px art border (COLOR_LINE), drawn on the chunk's overlapping edges.
    auto put = [&](int px, int py) {
      if (px >= x && px < x + actualWidth && py >= y && py < y + actualHeight) {
        buffer[(py - y) * actualWidth + (px - x)] = COLOR_LINE;
      }
    };
    if (artTop >= chunkTop && artTop < chunkBottom)
      for (int col = overlapLeft; col < overlapRight; ++col)
        put(col, artTop);
    if (artBottom - 1 >= chunkTop && artBottom - 1 < chunkBottom)
      for (int col = overlapLeft; col < overlapRight; ++col)
        put(col, artBottom - 1);
    if (artLeft >= x && artLeft < x + actualWidth)
      for (int row = overlapTop; row < overlapBottom; ++row)
        put(artLeft, row);
    if (artRight - 1 >= x && artRight - 1 < x + actualWidth)
      for (int row = overlapTop; row < overlapBottom; ++row)
        put(artRight - 1, row);
  }

  // Metadata (title + album). Only render if the text's x range overlaps this
  // strip; the font renderer clips to the buffer otherwise.
  fontRenderer.setBuffer(buffer, actualWidth, actualHeight);
  const int metadataMaxWidth = SCREEN_WIDTH - METADATA_X - 8;

  if (TITLE_Y >= y && TITLE_Y < y + actualHeight && METADATA_X >= x &&
      METADATA_X < x + actualWidth) {
    char line[64];
    strncpy(line, currentTrack ? currentTrack : "", sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    if (fontRenderer.measureText(line, IBMPlexSans16) > metadataMaxWidth) {
      int len = strlen(line);
      while (len > 1) {
        char test[64];
        snprintf(test, sizeof(test), "%.*s...", len, line);
        if (fontRenderer.measureText(test, IBMPlexSans16) <= metadataMaxWidth)
          break;
        len--;
      }
      snprintf(line, sizeof(line), "%.*s...", len, line);
    }
    fontRenderer.renderText(line, METADATA_X - x, TITLE_Y - y, IBMPlexSans16,
                            COLOR_TEXT, COLOR_BG);
  }

  if (albumName[0] && ALBUM_Y >= y && ALBUM_Y < y + actualHeight &&
      METADATA_X >= x && METADATA_X < x + actualWidth) {
    char line[64];
    strncpy(line, albumName, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    if (fontRenderer.measureText(line, IBMPlexSans12) > metadataMaxWidth) {
      int len = strlen(line);
      while (len > 1) {
        char test[64];
        snprintf(test, sizeof(test), "%.*s...", len, line);
        if (fontRenderer.measureText(test, IBMPlexSans12) <= metadataMaxWidth)
          break;
        len--;
      }
      snprintf(line, sizeof(line), "%.*s...", len, line);
    }
    fontRenderer.renderText(line, METADATA_X - x, ALBUM_Y - y, IBMPlexSans12,
                            COLOR_MUTED, COLOR_BG);
  }

  if (tx < 0)
    tx = x;
  if (ty < 0)
    ty = y;
  display->setWindow(tx, ty, tx + actualWidth - 1, ty + actualHeight - 1);
  display->pushPixels(buffer, actualWidth * actualHeight);
  g_buffers.swapBuffers();
}

void NowPlayingComponent::renderBandChunk(ILI9341_GFX *display, int x,
                                          int width, int tx, int ty) {
  const int y = BOTTOM_BAND_Y;
  int actualHeight = (y + CHUNK_HEIGHT > SCREEN_HEIGHT) ? (SCREEN_HEIGHT - y)
                                                        : CHUNK_HEIGHT;
  if (x >= SCREEN_WIDTH || width <= 0)
    return;
  int actualWidth = (width < SCREEN_WIDTH - x) ? width : (SCREEN_WIDTH - x);
  uint16_t *buffer = g_buffers.getCurrentBuffer();

  const uint16_t bandBg = volumeOverlayVisible ? COLOR_SURFACE : COLOR_BG;
  for (int i = 0; i < actualWidth * actualHeight; i++) {
    buffer[i] = bandBg;
  }

  fontRenderer.setBuffer(buffer, actualWidth, actualHeight);
  const float frac =
      (trackLength > 0)
          ? constrain((float)progress / (float)trackLength, 0.0f, 1.0f)
          : 0.0f;

  if (volumeOverlayVisible) {
    // Speaker glyph + volume bar, on a SURFACE panel.
    blitSprite(buffer, actualWidth, actualHeight, SPR_SPEAKER, 36 - x,
               220 - y);
    const int vx = 56, vy = 221, vw = 220, vh = 8;
    fillRoundedRect(buffer, actualWidth, actualHeight, vx - x, vy - y, vw, vh,
                    vh / 2, COLOR_LINE);
    int filled = (vw * constrain(volumeLevel, 0, 100)) / 100;
    if (filled > 0)
      fillRoundedRect(buffer, actualWidth, actualHeight, vx - x, vy - y,
                      filled, vh, vh / 2, COLOR_ACCENT);
  } else if (seekModeActive) {
    // Elapsed (accent) + total (muted), seek bar + handle.
    char elapsed[8];
    char total[8];
    int em = progress / 60, es = progress % 60;
    int tm = trackLength / 60, ts = trackLength % 60;
    snprintf(elapsed, sizeof(elapsed), "%d:%02d", em, es);
    snprintf(total, sizeof(total), "%d:%02d", tm, ts);

    fontRenderer.renderText(elapsed, 8 - x, 212 - y, IBMPlexSans12,
                            COLOR_ACCENT, COLOR_BG);
    int totalW = fontRenderer.measureText(total, IBMPlexSans12);
    fontRenderer.renderText(total, 312 - x - totalW, 212 - y, IBMPlexSans12,
                            COLOR_MUTED, COLOR_BG);

    const int by = 228, bh = 10;
    fillRoundedRect(buffer, actualWidth, actualHeight, BAR_X - x, by - y,
                    BAR_W, bh, 2, COLOR_LINE);
    int fillW = (int)(BAR_W * frac);
    if (fillW > 0)
      fillRoundedRect(buffer, actualWidth, actualHeight, BAR_X - x, by - y,
                      fillW, bh, 2, COLOR_ACCENT_DK);
    blitSprite(buffer, actualWidth, actualHeight, SPR_HANDLE,
               BAR_X + fillW - 3 - x, by - 2 - y);
  } else {
    // Elapsed + total, progress bar (accent fill on LINE track).
    char elapsed[8];
    char total[8];
    int em = progress / 60, es = progress % 60;
    int tm = trackLength / 60, ts = trackLength % 60;
    snprintf(elapsed, sizeof(elapsed), "%d:%02d", em, es);
    snprintf(total, sizeof(total), "%d:%02d", tm, ts);

    fontRenderer.renderText(elapsed, 8 - x, 212 - y, IBMPlexSans12,
                            COLOR_TEXT, COLOR_BG);
    int totalW = fontRenderer.measureText(total, IBMPlexSans12);
    fontRenderer.renderText(total, 312 - x - totalW, 212 - y, IBMPlexSans12,
                            COLOR_MUTED, COLOR_BG);

    const int by = 232, bh = 6;
    fillRoundedRect(buffer, actualWidth, actualHeight, BAR_X - x, by - y,
                    BAR_W, bh, bh / 2, COLOR_LINE);
    int fillW = (int)(BAR_W * frac);
    if (fillW > 0)
      fillRoundedRect(buffer, actualWidth, actualHeight, BAR_X - x, by - y,
                      fillW, bh, bh / 2, COLOR_ACCENT);
  }

  if (tx < 0)
    tx = x;
  if (ty < 0)
    ty = y;
  display->setWindow(tx, ty, tx + actualWidth - 1, ty + actualHeight - 1);
  display->pushPixels(buffer, actualWidth * actualHeight);
  g_buffers.swapBuffers();
}

void NowPlayingComponent::updateNowPlaying(ILI9341_GFX *display) {
  // Only the bottom band (progress/seek/volume) moves between progress ticks.
  renderChunk(display, 0, BOTTOM_BAND_Y, SCREEN_WIDTH);
}

void NowPlayingComponent::renderVolumeOverlay(ILI9341_GFX *display) {
  renderChunk(display, 0, BOTTOM_BAND_Y, SCREEN_WIDTH);
}
