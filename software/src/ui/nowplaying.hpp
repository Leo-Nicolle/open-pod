#pragma once
#include "fonts/IBMPlexSans16.h"
#include "rendering/font_renderer.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

class ILI9341_GFX;

class NowPlayingComponent {
private:
  const char *currentTrack;
  bool isPlaying;
  /** @brief Track length in seconds */
  int trackLength;
  /** @brief Current playback time in seconds */
  int progress;
  // Layout constants
  static const int ALBUM_ART_X = 100;
  static const int ALBUM_ART_Y = 50;
  static const int ALBUM_ART_SIZE = 120;
  static const int TITLE_Y = 185;
  static const int PROGRESS_BAR_Y = 220;
  static const int PROGRESS_BAR_HEIGHT = 8;
  static const int PROGRESS_BAR_WIDTH = 300;

  static const int PROGRESS_BAR_X = 10;
  static const int PROGRESS_BAR_END_X = PROGRESS_BAR_X + PROGRESS_BAR_WIDTH;
  static const int PROGRESS_BAR_STROKE = 1;
  static const int TIME_TEXT_Y = PROGRESS_BAR_Y - PROGRESS_BAR_HEIGHT - 10;

public:
  NowPlayingComponent();

  // Configuration
  void setTrack(const char *trackName);
  /**
   * @brief Set the playback progress in second
   */
  void setProgress(int progress);
  void setTrackLength(int tl);
  void setPlayState(bool playing);

  void renderChunk(ILI9341_GFX *display, int x, int y,
                   int width = SCREEN_WIDTH, int tx = -1,
                   int ty = -1);
  void render(ILI9341_GFX *display, int x, int width);

  void updateNowPlaying(ILI9341_GFX *display);

  // Utility functions
  bool isInAlbumArtArea(int x, int y) const;
  bool isInTitleArea(int x, int y) const;
  bool isInProgressBarArea(int x, int y) const;
};

// Implementation
NowPlayingComponent::NowPlayingComponent()
    : currentTrack(""), progress(0.0f), isPlaying(false) {}

void NowPlayingComponent::setTrack(const char *trackName) {
  currentTrack = trackName;
}
void NowPlayingComponent::setProgress(int progress) {
  this->progress = progress;
}
void NowPlayingComponent::setTrackLength(int tl) { trackLength = tl; }

void NowPlayingComponent::setPlayState(bool playing) { isPlaying = playing; }
void NowPlayingComponent::renderChunk(ILI9341_GFX *display, int x,
                                      int y, int width, int tx,
                                      int ty) {
  // Define chunk height and clamp to screen
  if (y >= SCREEN_HEIGHT)
    return;

  int actualHeight =
      (y + CHUNK_HEIGHT > SCREEN_HEIGHT) ? (SCREEN_HEIGHT - y) : CHUNK_HEIGHT;
  if (x >= SCREEN_WIDTH || width <= 0)
    return;

  int actualWidth =
      (width < SCREEN_WIDTH - x) ? width : (SCREEN_WIDTH - x);
  uint16_t *buffer = g_buffers.getCurrentBuffer();

  // Pre-calculate bounds for each UI element
  int artTop = ALBUM_ART_Y;
  int artBottom = ALBUM_ART_Y + ALBUM_ART_SIZE;
  int artLeft = ALBUM_ART_X;
  int artRight = ALBUM_ART_X + ALBUM_ART_SIZE;

  int chunkTop = y;
  int chunkBottom = y + actualHeight;

  int barTop = PROGRESS_BAR_Y;
  int barBottom = PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT;
  int barLeft = PROGRESS_BAR_X;
  int barRight = PROGRESS_BAR_X + PROGRESS_BAR_WIDTH;

  float barProgress =
      constrain((float)progress / (float)trackLength, 0.0f, 1.0f);
  int filledWidth = static_cast<int>(
      (PROGRESS_BAR_WIDTH - 2 * PROGRESS_BAR_STROKE) * barProgress);

  const int TIME_TEXT_Y = PROGRESS_BAR_Y - PROGRESS_BAR_HEIGHT - 10;

  // Fill background for the specified width only
  int row, col, bufRow;
  for (row = 0; row < actualHeight; ++row) {
    for (col = 0; col < actualWidth; ++col) {
      buffer[row * actualWidth + col] = COLOR_BACKGROUND;
    }
  }

  // Album art (if it overlaps this chunk and xOffset)
  int overlapTop = std::max(chunkTop, artTop);
  int overlapBottom = std::min(chunkBottom, artBottom);
  int overlapLeft = std::max(x, artLeft);
  int overlapRight = std::min(x + actualWidth, artRight);

  if (overlapTop < overlapBottom && overlapLeft < overlapRight) {
    // Fill album art area
    for (row = overlapTop; row < overlapBottom; ++row) {
      bufRow = row - y;
      for (col = overlapLeft; col < overlapRight; ++col) {
        int bufCol = col - x;
        if (bufCol >= 0 && bufCol < actualWidth && bufRow >= 0 &&
            bufRow < actualHeight) {
          buffer[bufRow * actualWidth + bufCol] = COLOR_SECONDARY;
        }
      }
    }

    // Draw album art border
    // Top border
    if (artTop >= chunkTop && artTop < chunkBottom) {
      bufRow = artTop - y;
      for (col = overlapLeft; col < overlapRight; ++col) {
        int bufCol = col - x;
        if (bufCol >= 0 && bufCol < actualWidth) {
          buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
        }
      }
    }

    // Bottom border
    if (artBottom - 1 >= chunkTop && artBottom - 1 < chunkBottom) {
      bufRow = artBottom - 1 - y;
      for (col = overlapLeft; col < overlapRight; ++col) {
        int bufCol = col - x;
        if (bufCol >= 0 && bufCol < actualWidth) {
          buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
        }
      }
    }

    // Left and right borders
    for (row = overlapTop; row < overlapBottom; ++row) {
      bufRow = row - y;
      // Left border
      if (artLeft >= x && artLeft < x + actualWidth) {
        int bufCol = artLeft - x;
        buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
      }
      // Right border
      if (artRight - 1 >= x && artRight - 1 < x + actualWidth) {
        int bufCol = artRight - 1 - x;
        buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
      }
    }
  }

  // Track title (if it overlaps this chunk and xOffset)
  if (TITLE_Y >= y && TITLE_Y < y + actualHeight && TITLE_Y - y >= 0 &&
      TITLE_Y - y < actualHeight) {
    // Only render text if it's visible in this x range
    if (10 >= x && 10 < x + actualWidth) {
      fontRenderer.setBuffer(buffer, actualWidth, actualHeight);
      fontRenderer.renderText(currentTrack, 10 - x, TITLE_Y - y, IBMPlexSans16,
                              COLOR_TEXT, COLOR_BACKGROUND);
    }
  }

  // Progress bar (if it overlaps this chunk and xOffset)
  if (barBottom > y && barTop < y + actualHeight) {
    int barStart = std::max(barTop, chunkTop);
    int barEnd = std::min(barBottom, chunkBottom);

    // Check if progress bar intersects with our x range
    if (barRight > x && barLeft < x + actualWidth) {
      for (row = barStart; row < barEnd; ++row) {
        bufRow = row - y;

        // Draw outline (top and bottom rows)
        if (row == barTop || row == barBottom - 1) {
          int lineStart = std::max(barLeft, x);
          int lineEnd = std::min(barRight, x + actualWidth);
          for (col = lineStart; col < lineEnd; ++col) {
            int bufCol = col - x;
            buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
          }
        } else {
          // Draw left and right outline
          if (barLeft >= x && barLeft < x + actualWidth) {
            int bufCol = barLeft - x;
            buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
          }
          if (barRight - 1 >= x && barRight - 1 < x + actualWidth) {
            int bufCol = barRight - 1 - x;
            buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
          }

          // Fill track background
          int trackStart = std::max(barLeft + PROGRESS_BAR_STROKE, x);
          int trackEnd =
              std::min(barRight - PROGRESS_BAR_STROKE, x + actualWidth);
          for (col = trackStart; col < trackEnd; ++col) {
            int bufCol = col - x;
            buffer[bufRow * actualWidth + bufCol] = COLOR_HIGHLIGHT;
          }

          // Fill progress (overlay on track background)
          int progressEnd = barLeft + PROGRESS_BAR_STROKE + filledWidth;
          int fillStart = std::max(barLeft + PROGRESS_BAR_STROKE, x);
          int fillEnd = std::min(progressEnd, x + actualWidth);
          for (col = fillStart; col < fillEnd && col < trackEnd; ++col) {
            int bufCol = col - x;
            buffer[bufRow * actualWidth + bufCol] = COLOR_ACCENT;
          }
        }
      }
    }
  }

  // Progress time text (if it overlaps this chunk and xOffset)
  if (TIME_TEXT_Y >= y && TIME_TEXT_Y < y + actualHeight) {
    // Format progress time (mm:ss)
    int progressMinutes = progress / 60;
    int progressSeconds = progress % 60;
    char progressText[8];
    snprintf(progressText, sizeof(progressText), "%d:%02d", progressMinutes,
             progressSeconds);

    // Format track length (mm:ss)
    int lengthMinutes = trackLength / 60;
    int lengthSecondsRemainder = trackLength % 60;
    char lengthText[8];
    snprintf(lengthText, sizeof(lengthText), "%d:%02d", lengthMinutes,
             lengthSecondsRemainder);

    fontRenderer.setBuffer(buffer, actualWidth, actualHeight);

    // Render progress time (left aligned) - only if visible in x range
    if (PROGRESS_BAR_X >= x && PROGRESS_BAR_X < x + actualWidth) {
      fontRenderer.renderText(progressText, PROGRESS_BAR_X - x, TIME_TEXT_Y - y,
                              IBMPlexSans16, COLOR_TEXT, COLOR_BACKGROUND);
    }

    // Render track length (right aligned) - only if visible in x range
    int rightTextX = PROGRESS_BAR_END_X - 40;
    if (rightTextX >= x && rightTextX < x + actualWidth) {
      fontRenderer.renderText(lengthText, rightTextX - x, TIME_TEXT_Y - y,
                              IBMPlexSans16, COLOR_TEXT, COLOR_BACKGROUND);
    }
  }
  if (tx < 0) {
    tx = x;
  }
  if (ty < 0) {
    ty = y;
  }
  // Push this chunk to the display using target coordinates
  display->setWindow(tx, ty, tx + actualWidth - 1, ty + actualHeight - 1);
  display->pushPixels(buffer, actualWidth * actualHeight);
  g_buffers.swapBuffers();
}


void NowPlayingComponent::updateNowPlaying(ILI9341_GFX *display) {
  // Only update the progress bar and time text areas for efficiency
  const int UPDATE_END_Y =
      PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT + 2 * PROGRESS_BAR_STROKE;
  for (int y = TIME_TEXT_Y; y < UPDATE_END_Y; y += CHUNK_HEIGHT) {
    renderChunk(display, 0, y, SCREEN_WIDTH);
  }
}
void NowPlayingComponent::render(ILI9341_GFX *display, int x,
                                 int width) {
  for (int y = BODY_Y; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT) {
    renderChunk(display, x, y, width);
  }
}