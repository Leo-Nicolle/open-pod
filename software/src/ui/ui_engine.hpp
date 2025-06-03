#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "header.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
#include "tracklist.hpp"
#include "ui_types.h"
#include <Arduino.h>

class OpenPodUIEngine {
private:
  ILI9341_GFX *display;
  AnimationManager animManager;

  // UI Components
  HeaderComponent header;
  ScrollbarComponent scrollbar;
  TrackListComponent trackList;
  NowPlayingComponent nowPlaying;

  // State management
  UIState currentState;
  UIState targetState;
  bool isRotatedMode;

  // Animation IDs
  uint32_t scrollAnimId;
  uint32_t transitionAnimId;

  // Track data
  const char *tracks[20] = {"Bohemian Rhapsody",
                            "Hotel California",
                            "Stairway to Heaven",
                            "Sweet Child O' Mine",
                            "Imagine",
                            "Billie Jean",
                            "Like a Rolling Stone",
                            "Smells Like Teen Spirit",
                            "Purple Haze",
                            "What's Going On",
                            "Respect",
                            "Good Vibrations",
                            "Johnny B. Goode",
                            "Hey Jude",
                            "I Want to Hold Your Hand",
                            "Yesterday",
                            "Satisfaction",
                            "My Girl",
                            "Bridge Over Troubled Water",
                            "The Sound of Silence"};

  // Navigation state
  int selectedTrack;
  int topVisibleTrack;
  int previousSelectedTrack;

  // Transition rendering
  int lastDrawnOffset;

public:
  OpenPodUIEngine(ILI9341_GFX *disp);

  // Lifecycle
  void begin();
  void update();

  // Navigation
  void scrollUp();
  void scrollDown();
  void pageUp();
  void pageDown();
  void selectTrack();
  void returnToList();

  // Rendering using global buffers
  void renderCurrentState();
  void renderTrackList();
  void renderNowPlaying();

  // Efficient transition rendering
  void transitionToNowPlaying();
  void transitionToTrackList();

  // Smooth scrolling
  void animateScroll(bool scrollingUp);

  // Utility
  void setRotation(bool rotated) { isRotatedMode = rotated; }
  float getFPS() const { return animManager.getFPS(); }
  void measurePerformance();

private:
  // Internal helpers
  void updateScrollbar();
  void updateSelection(int newSelected, int newTopVisible);
  bool isAnimating() const { return animManager.isActive(); }
};

// Implementation
OpenPodUIEngine::OpenPodUIEngine(ILI9341_GFX *disp)
    : display(disp), trackList(tracks, 20), selectedTrack(0),
      topVisibleTrack(0), previousSelectedTrack(-1),
      currentState(STATE_TRACK_LIST), targetState(STATE_TRACK_LIST),
      isRotatedMode(false), scrollAnimId(0), transitionAnimId(0),
      lastDrawnOffset(0) {
}

void OpenPodUIEngine::begin() {
  display->fillScreen(COLOR_BACKGROUND);

  // Initialize components
  trackList.setSelection(selectedTrack, topVisibleTrack);
  updateScrollbar();

  // Render initial state
  renderCurrentState();

  Serial.println("OpenPod UI Engine initialized with global buffers");
}

void OpenPodUIEngine::update() { animManager.update(); }

void OpenPodUIEngine::updateScrollbar() {
  scrollbar.setScrollData(20, TRACKS_PER_SCREEN, topVisibleTrack);
}

void OpenPodUIEngine::updateSelection(int newSelected, int newTopVisible) {
  previousSelectedTrack = selectedTrack;
  selectedTrack = constrain(newSelected, 0, 19);
  topVisibleTrack = constrain(newTopVisible, 0, max(0, 20 - TRACKS_PER_SCREEN));

  trackList.setSelection(selectedTrack, topVisibleTrack);
  updateScrollbar();
}

void OpenPodUIEngine::renderCurrentState() {
  Serial.println("Rendering current state: " +
                 String(currentState == STATE_TRACK_LIST ? "Track List" : "Now Playing"));
  switch (currentState) {
  case STATE_TRACK_LIST:
    renderTrackList();
    break;
  case STATE_NOW_PLAYING:
    renderNowPlaying();
    break;
  case STATE_TRANSITIONING:
    // Don't render during transitions - handled by animation
    break;
  }
}

void OpenPodUIEngine::renderTrackList() {
  header.render(display);
  trackList.renderAllTracks(display);
  scrollbar.render(display);
}

void OpenPodUIEngine::renderNowPlaying() {
  if (!isRotatedMode) {
    // Render using global buffers row by row
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      // Clear row buffer

      // Render header
      if (y <= HEADER_HEIGHT) {
      }

      // Render now playing screen

      // Push the row to display
    }
  }
}

void OpenPodUIEngine::scrollUp() {
  if (currentState != STATE_TRACK_LIST || isAnimating())
    return;

  if (selectedTrack > 0) {
    int newSelected = selectedTrack - 1;
    int newTopVisible = topVisibleTrack;

    if (newSelected < topVisibleTrack) {
      newTopVisible = newSelected;
      updateSelection(newSelected, newTopVisible);
      animateScroll(true);
    } else {
      updateSelection(newSelected, newTopVisible);
      // Quick update - just redraw track list area
      for (int y = TRACK_LIST_Y;
           y < TRACK_LIST_Y + TRACKS_PER_SCREEN * TRACK_HEIGHT; y++) {

      }
    }
  }
}

void OpenPodUIEngine::scrollDown() {
  if (currentState != STATE_TRACK_LIST || isAnimating())
    return;

  if (selectedTrack < 19) {
    int newSelected = selectedTrack + 1;
    int newTopVisible = topVisibleTrack;

    if (newSelected >= topVisibleTrack + TRACKS_PER_SCREEN) {
      newTopVisible = topVisibleTrack + 1;
      updateSelection(newSelected, newTopVisible);
      animateScroll(false);
    } else {
      updateSelection(newSelected, newTopVisible);
      // Quick update - just redraw track list area
      for (int y = TRACK_LIST_Y;
           y < TRACK_LIST_Y + TRACKS_PER_SCREEN * TRACK_HEIGHT; y++) {
      }
    }
  }
}

void OpenPodUIEngine::selectTrack() {
  if (currentState == STATE_TRACK_LIST && !isAnimating()) {
    transitionToNowPlaying();
  }
}

void OpenPodUIEngine::returnToList() {
  if (currentState == STATE_NOW_PLAYING && !isAnimating()) {
    transitionToTrackList();
  }
}

void OpenPodUIEngine::transitionToNowPlaying() {
  currentState = STATE_TRANSITIONING;
  targetState = STATE_NOW_PLAYING;
  lastDrawnOffset = 0;

  transitionAnimId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 1000,
      [this](float offset) {
        int currentOffset = (int)offset;

        // Draw columns from right to left
        for (int x = lastDrawnOffset; x < currentOffset; x++) {
          // renderTransitionColumn(SCREEN_WIDTH - 1 - x, true);
        }
        lastDrawnOffset = currentOffset;
      },
      [this]() {
        currentState = STATE_NOW_PLAYING;
        renderNowPlaying();
      },
      Easing::easeInOutCubic);
}

void OpenPodUIEngine::transitionToTrackList() {
  currentState = STATE_TRANSITIONING;
  targetState = STATE_TRACK_LIST;
  lastDrawnOffset = 0;

  transitionAnimId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 1000,
      [this](float offset) {
        int currentOffset = (int)offset;

        // Draw columns from left to right
        for (int x = lastDrawnOffset; x < currentOffset; x++) {
          // renderTransitionColumn(x, false);
        }
        lastDrawnOffset = currentOffset;
      },
      nullptr, Easing::easeInOutCubic);
}

void OpenPodUIEngine::animateScroll(bool scrollingUp) {
  // Setup hardware scrolling
  display->writeCommand(0x33);                  // VSCRDEF
  display->writeData16(HEADER_HEIGHT + MARGIN); // TFA = header area
  display->writeData16(TRACKS_PER_SCREEN *
                       TRACK_HEIGHT); // VSA = scrollable area
  display->writeData16(SCREEN_HEIGHT - HEADER_HEIGHT - MARGIN -
                       (TRACKS_PER_SCREEN * TRACK_HEIGHT)); // BFA

  int startOffset = scrollingUp ? -TRACK_HEIGHT : TRACK_HEIGHT;

  scrollAnimId = animManager.animate(
      ANIM_CUSTOM, startOffset, 2 * startOffset, 500,
      [this](float offset) {
        int intOffset = (int)offset;
        display->writeCommand(0x37); // VSCRSADD
        display->writeData16(topVisibleTrack * TRACK_HEIGHT + intOffset);
      },
      [this]() {
        display->writeCommand(0x37);
        display->writeData16(0);
        renderTrackList();
      },
      [](float t) { return Easing::easeOutCubic(t); });
}

void OpenPodUIEngine::pageUp() {
  if (currentState != STATE_TRACK_LIST || isAnimating())
    return;

  int newSelected = max(0, selectedTrack - TRACKS_PER_SCREEN);
  int newTopVisible = max(0, topVisibleTrack - TRACKS_PER_SCREEN);

  updateSelection(newSelected, newTopVisible);
  renderTrackList();
}

void OpenPodUIEngine::pageDown() {
  if (currentState != STATE_TRACK_LIST || isAnimating())
    return;

  int newSelected = min(19, selectedTrack + TRACKS_PER_SCREEN);
  int newTopVisible =
      min(20 - TRACKS_PER_SCREEN, topVisibleTrack + TRACKS_PER_SCREEN);

  updateSelection(newSelected, newTopVisible);
  renderTrackList();
}

void OpenPodUIEngine::measurePerformance() {
  Serial.println("\n=== UI Performance Metrics ===");
  Serial.print("Current FPS: ");
  Serial.println(animManager.getFPS());

  uint32_t avg, min, max;
  animManager.getPerformanceStats(avg, min, max);
  Serial.print("Frame times - Avg: ");
  Serial.print(avg);
  Serial.print("µs, Min: ");
  Serial.print(min);
  Serial.print("µs, Max: ");
  Serial.print(max);
  Serial.println("µs");
  Serial.print("Free heap: ");
  // Serial.println(ESP.getFreeHeap());
}