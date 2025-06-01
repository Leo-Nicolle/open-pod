#pragma once
#include <Arduino.h>
#include "ILI9341_GFX.h"
#include "fonts/IBMPlexSans16.h"
#include "fonts/IBMPlexSans12.h"
#include "font-renderer.h"
#include "theme.h"

class OpenPodUI {
private:
  ILI9341_GFX* display;

  const char* tracks[20] = {
    "Bohemian Rhapsody", "Hotel California", "Stairway to Heaven",
    "Sweet Child O' Mine", "Imagine", "Billie Jean",
    "Like a Rolling Stone", "Smells Like Teen Spirit", "Purple Haze",
    "What's Going On", "Respect", "Good Vibrations",
    "Johnny B. Goode", "Hey Jude", "I Want to Hold Your Hand",
    "Yesterday", "Satisfaction", "My Girl",
    "Bridge Over Troubled Water", "The Sound of Silence"
  };

  int totalTracks = 20;
  int selectedTrack = 0;
  int previousSelectedTrack = -1;
  int topVisibleTrack = 0;
  int tracksPerScreen = 6;

  static const int headerHeight = 30;
  static const int selectedMargin = 5;
  static const int trackHeight = 30;
  static const int margin = 5;
  static const int trackListY = headerHeight + margin;

  uint16_t trackBuffer[320 * 30];
  uint16_t headerBuffer[320 * 30];

  MenuItemRenderer* menuRenderer;
  FastFontRenderer* headerRenderer;

  bool headerRendered = false;

  bool isScrolling = false;
  int scrollOffset = 0;
  int scrollDirection = 0;
  uint32_t scrollStartTime = 0;

public:
  OpenPodUI(ILI9341_GFX* disp) : display(disp) {
    menuRenderer = new MenuItemRenderer(trackBuffer, IBMPlexSans16, IBMPlexSans12);
    headerRenderer = new FastFontRenderer(headerBuffer, 320, 30);
  }

  ~OpenPodUI() {
    delete menuRenderer;
    delete headerRenderer;
  }

  void begin() {
    display->fillScreen(COLOR_BACKGROUND);
    renderHeader();
    renderAllTracks();
  }

  void renderHeader() {
    for (int i = 0; i < 320 * 30; i++) {
      headerBuffer[i] = COLOR_PRIMARY;
    }

    headerRenderer->renderText("OpenPod", 10, 20, IBMPlexSans16, COLOR_TEXT, COLOR_PRIMARY);
    renderBatteryIndicator();

    display->setWindow(0, 0, 319, 29);
    display->pushPixels(headerBuffer, 320 * 30);
    display->drawFastHLine(0, headerHeight, 320, COLOR_SECONDARY);

    headerRendered = true;
  }

  void renderBatteryIndicator() {
    int battX = 285;
    int battY = 8;
    int battW = 25;
    int battH = 12;

    for (int y = battY; y < battY + battH; y++) {
      for (int x = battX; x < battX + battW; x++) {
        int idx = y * 320 + x;
        if (y == battY || y == battY + battH - 1 || x == battX || x == battX + battW - 1) {
          headerBuffer[idx] = COLOR_TEXT;
        }
      }
    }

    for (int y = battY + 3; y < battY + 9; y++) {
      headerBuffer[y * 320 + battX + battW] = COLOR_TEXT;
    }

    for (int y = battY + 2; y < battY + battH - 2; y++) {
      for (int x = battX + 2; x < battX + 18; x++) {
        headerBuffer[y * 320 + x] = COLOR_HIGHLIGHT;
      }
    }
  }

  void renderAllTracks() {
    for (int i = 0; i < tracksPerScreen; i++) {
      renderTrack(i);
    }
    drawScrollIndicator();
  }

  void renderTrack(int screenPosition) {
    int trackIndex = topVisibleTrack + screenPosition;
    if (trackIndex >= totalTracks) return;

    bool isSelected = (trackIndex == selectedTrack);
    bool previousSelected = (trackIndex-1 == selectedTrack);
    int y = trackListY + (screenPosition * trackHeight);

    uint16_t bgColor = isSelected ? COLOR_HIGHLIGHT : COLOR_BACKGROUND;
    uint16_t textColor = isSelected ? COLOR_TEXT : COLOR_TEXT;

    menuRenderer->renderMenuItem(trackIndex + 1, tracks[trackIndex], isSelected, previousSelected);
    display->setWindow(0, y, 320, y + trackHeight - 1);
    display->pushPixels(trackBuffer, 320 * trackHeight);

  }

  void drawScrollIndicator() {
    if (totalTracks <= tracksPerScreen) return;

    int scrollBarX = SCROLLBAR_X;
    int scrollBarY = trackListY;
    int scrollBarHeight = tracksPerScreen * trackHeight;

    display->fillRect(scrollBarX, scrollBarY, 8, scrollBarHeight, COLOR_BACKGROUND);
    display->drawRect(scrollBarX, scrollBarY, 6, scrollBarHeight, COLOR_ACCENT);

    int thumbHeight = max(20, (scrollBarHeight * tracksPerScreen) / totalTracks);
    int thumbPos = scrollBarY + ((scrollBarHeight - thumbHeight) * topVisibleTrack) /
                   (totalTracks - tracksPerScreen);

    display->fillRect(scrollBarX + 1, thumbPos, 4, thumbHeight, COLOR_SECONDARY);
  }

  void updateChangedTracks() {
    if (previousSelectedTrack != selectedTrack) {
      int oldPos = previousSelectedTrack - topVisibleTrack;
      if (oldPos >= 0 && oldPos < tracksPerScreen) {
        renderTrack(oldPos);
      }

      int newPos = selectedTrack - topVisibleTrack;
      if (newPos >= 0 && newPos < tracksPerScreen) {
        renderTrack(newPos);
      }

      previousSelectedTrack = selectedTrack;
    }
    drawScrollIndicator();
  }

  void animateScroll() {
    if (!isScrolling) return;

    uint32_t elapsed = millis() - scrollStartTime;
    float progress = min(1.0f, elapsed / 100.0f);
    float eased = easeInOutCubic(progress);
    scrollOffset = (int)(trackHeight * eased * scrollDirection);

    if (progress >= 1.0f) {
      isScrolling = false;
      scrollOffset = 0;
      renderAllTracks();
    } else {
      performHardwareScroll();
    }
  }

  void performHardwareScroll() {
    int baseOffset = topVisibleTrack * trackHeight;
    int animOffset = scrollOffset;
    display->verticalScroll(trackListY, tracksPerScreen * trackHeight,
                            baseOffset + animOffset);
  }

  float easeInOutCubic(float t) {
    return t < 0.5f ? 4 * t * t * t : 1 + 4 * (t - 1) * (t - 1) * (t - 1);
  }

  void scrollUp() {
    if (selectedTrack > 0) {
      previousSelectedTrack = selectedTrack;
      selectedTrack--;

      if (selectedTrack < topVisibleTrack) {
        topVisibleTrack--;
        renderAllTracks();
      } else {
        updateChangedTracks();
      }
    }
  }

  void scrollDown() {
    if (selectedTrack < totalTracks - 1) {
      previousSelectedTrack = selectedTrack;
      selectedTrack++;

      if (selectedTrack >= topVisibleTrack + tracksPerScreen) {
        topVisibleTrack++;
        renderAllTracks();
      } else {
        updateChangedTracks();
      }
    }
  }

  void pageUp() {
    int oldSelected = selectedTrack;
    int oldTop = topVisibleTrack;

    selectedTrack = max(0, selectedTrack - tracksPerScreen);
    topVisibleTrack = max(0, topVisibleTrack - tracksPerScreen);

    if (topVisibleTrack != oldTop) {
      renderAllTracks();
    } else if (selectedTrack != oldSelected) {
      previousSelectedTrack = oldSelected;
      updateChangedTracks();
    }
  }

  void pageDown() {
    int oldSelected = selectedTrack;
    int oldTop = topVisibleTrack;

    selectedTrack = min(totalTracks - 1, selectedTrack + tracksPerScreen);
    topVisibleTrack = min(totalTracks - tracksPerScreen,
                          topVisibleTrack + tracksPerScreen);

    if (topVisibleTrack != oldTop) {
      renderAllTracks();
    } else if (selectedTrack != oldSelected) {
      previousSelectedTrack = oldSelected;
      updateChangedTracks();
    }
  }

  void update() {
    animateScroll();
  }

  void selectTrack() {
    showNowPlaying();
  }

  void showNowPlaying() {
    display->fillScreen(COLOR_BACKGROUND);
    display->fillRect(0, 0, 320, 30, COLOR_PRIMARY);
    display->drawFastHLine(0, 30, 320, COLOR_SECONDARY);

    for (int i = 0; i < 320 * 30; i++) {
      headerBuffer[i] = COLOR_PRIMARY;
    }
    headerRenderer->renderText("Now Playing", 10, 20, IBMPlexSans16, COLOR_TEXT, COLOR_PRIMARY);
    display->setWindow(0, 0, 319, 29);
    display->pushPixels(headerBuffer, 320 * 30);

    int albumX = 100;
    int albumY = 50;
    display->fillRect(albumX, albumY, 120, 120, COLOR_SECONDARY);
    display->drawRect(albumX, albumY, 120, 120, COLOR_ACCENT);

    display->setTextColor(COLOR_TEXT);
    display->setTextSize(2);
    display->setCursor(10, 185);
    display->print(tracks[selectedTrack]);

    display->drawRect(10, 220, 300, 8, COLOR_ACCENT);
    display->fillRect(11, 221, 90, 6, COLOR_HIGHLIGHT);
  }

  void returnToList() {
    display->fillScreen(COLOR_BACKGROUND);
    renderHeader();
    renderAllTracks();
  }

  void measurePerformance() {
    uint32_t start = millis();
    int operations = 0;

    while (millis() - start < 1000) {
      scrollDown();
      updateChangedTracks();
      operations++;

      if (selectedTrack >= totalTracks - 1) {
        selectedTrack = 0;
        topVisibleTrack = 0;
      }
    }

    Serial.print("Scroll operations per second: ");
    Serial.println(operations);

    start = micros();
    renderTrack(0);
    uint32_t trackTime = micros() - start;

    Serial.print("Single track render: ");
    Serial.print(trackTime);
    Serial.println(" microseconds");

    float maxFPS = 1000000.0 / (trackTime * 2);
    Serial.print("Theoretical max FPS: ");
    Serial.println(maxFPS);
  }
};

// Usage example
class UIBenchmark {
public:
  static void runBenchmark(ILI9341_GFX* display) {
    Serial.println("\n=== UI Performance Benchmark ===");
    OpenPodUI ui(display);
    ui.begin();

    uint32_t start = micros();
    ui.scrollDown();
    ui.updateChangedTracks();
    uint32_t scrollTime = micros() - start;

    Serial.print("Scroll response: ");
    Serial.print(scrollTime);
    Serial.println(" µs");

    start = micros();
    ui.renderAllTracks();
    uint32_t fullRefresh = micros() - start;

    Serial.print("Full refresh: ");
    Serial.print(fullRefresh);
    Serial.println(" µs");

    ui.measurePerformance();

    float scrollFPS = 1000000.0 / scrollTime;
    Serial.print("\nSmooth scroll capability: ");
    Serial.print(scrollFPS);
    Serial.println(" FPS");
  }
};
