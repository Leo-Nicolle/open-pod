#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "header.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
#include "tracklist.hpp"
#include "ui_types.h"
#include "appstate.hpp"
#include <Arduino.h>

class OpenPodUIEngine {
public:
  ILI9341_GFX *display;
  AnimationManager animManager;

  // UI Components
  HeaderComponent header;
  ScrollbarComponent scrollbar;
  TrackListComponent trackList;
  NowPlayingComponent nowPlaying;

  // Centralized application state
  AppState appState;

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

  // Transition rendering
  int lastDrawnOffset;

public:
  OpenPodUIEngine(ILI9341_GFX *disp);

  // Lifecycle
  void begin();
  bool update();

  // Navigation
  void scrollUp();
  void scrollDown();
  void pageUp();
  void pageDown();
  void selectTrack();
  void returnToList();

  // Playback control
  void playSelectedTrack();
  void togglePlayback();
  void stopPlayback();

  // Rendering
  void renderCurrentState();
  void renderTrackList();
  void renderNowPlaying(int xOffset = 0, int width = SCREEN_WIDTH);
  void renderTrackListArea();

  // Transitions
  void transitionToNowPlaying();
  void transitionToTrackList();

  // Smooth scrolling
  void animateScroll(bool scrollingUp);

  // Utility
  void setRotation(bool rotated) { appState.isRotatedMode = rotated; }
  float getFPS() const { return animManager.getFPS(); }
  void measurePerformance();

private:
  // Internal helpers
  void updateScrollbar();
  void updateTrackListScroll();
  bool isAnimating() const { return appState.isAnimating; }
};

// Implementation
OpenPodUIEngine::OpenPodUIEngine(ILI9341_GFX *disp)
    : display(disp), trackList(tracks, 20), lastDrawnOffset(0) {
  
  // Initialize app state with track data
  appState.setTracks(tracks, 20);
}

void OpenPodUIEngine::begin() {
  display->fillScreen(COLOR_BACKGROUND);

  // Initialize components
  trackList.begin();
  updateTrackListScroll();
  updateScrollbar();

  // Render initial state
  renderCurrentState();

  Serial.println("OpenPod UI Engine initialized with centralized state");
}

bool OpenPodUIEngine::update() { 
  bool wasAnimating = appState.isAnimating;
  appState.isAnimating = animManager.isActive();
  
  // Update animation state
  bool result = animManager.update();
  
  return result;
}

void OpenPodUIEngine::updateScrollbar() {
  scrollbar.setScrollData(appState.totalTracks, TRACKS_PER_SCREEN, appState.topVisibleTrackIndex);
}

void OpenPodUIEngine::updateTrackListScroll() {
  trackList.setScroll(appState.selectedTrackIndex, appState.topVisibleTrackIndex);
}

void OpenPodUIEngine::renderCurrentState() {
  Serial.println(
      "Rendering current state: " +
      String(appState.currentUIState == STATE_TRACK_LIST ? "Track List" : "Now Playing"));
  
  switch (appState.currentUIState) {
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

void OpenPodUIEngine::renderNowPlaying(int xOffset, int width) {
  header.render(display);
  
  const char* trackName = appState.getPlayingTrackName();
  if (!trackName) {
    trackName = appState.getCurrentTrackName();
  }
  
  if (trackName) {
    nowPlaying.setTrack(trackName);
  }
  
  nowPlaying.setTrackLength(appState.trackDuration);
  nowPlaying.setProgress(appState.playbackPosition);
  nowPlaying.setPlayState(appState.isPlaying);
  nowPlaying.render(display, xOffset, width);
}

void OpenPodUIEngine::scrollUp() {
  if (appState.currentUIState != STATE_TRACK_LIST || isAnimating())
    return;

  int oldSelected = appState.selectedTrackIndex;
  int oldTopVisible = appState.topVisibleTrackIndex;
  
  appState.scrollUp();
  
  if (appState.needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateTrackListScroll();
    updateScrollbar();
    
    // Check if we need to scroll the cache
    if (appState.topVisibleTrackIndex != oldTopVisible) {
      // Calculate how many tracks we scrolled up
      int scrollDelta = oldTopVisible - appState.topVisibleTrackIndex;
      
      // Get the tracks that should now be visible
      int startIdx, endIdx;
      appState.getVisibleTrackIndices(startIdx, endIdx);
      
      // Create array of visible track names for cache update
      const char* visibleTracks[TRACKS_PER_SCREEN];
      for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
        int trackIdx = startIdx + i;
        visibleTracks[i] = (trackIdx < appState.totalTracks) ? appState.tracks[trackIdx] : "";
      }
      
      // Update the track list cache
      trackList.scrollUp(scrollDelta, visibleTracks);
    }
    
    renderTrackListArea();
  }
}

void OpenPodUIEngine::scrollDown() {
  if (appState.currentUIState != STATE_TRACK_LIST || isAnimating())
    return;

  int oldSelected = appState.selectedTrackIndex;
  int oldTopVisible = appState.topVisibleTrackIndex;
  
  appState.scrollDown();
  
  if (appState.needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateTrackListScroll();
    updateScrollbar();
    
    // Check if we need to scroll the cache
    if (appState.topVisibleTrackIndex != oldTopVisible) {
      // Calculate how many tracks we scrolled down
      int scrollDelta = appState.topVisibleTrackIndex - oldTopVisible;
      
      // Get the tracks that should now be visible
      int startIdx, endIdx;
      appState.getVisibleTrackIndices(startIdx, endIdx);
      
      // Create array of visible track names for cache update
      const char* visibleTracks[TRACKS_PER_SCREEN];
      for (int i = 0; i < TRACKS_PER_SCREEN; i++) {
        int trackIdx = startIdx + i;
        visibleTracks[i] = (trackIdx < appState.totalTracks) ? appState.tracks[trackIdx] : "";
      }
      
      // Update the track list cache
      trackList.scrollDown(scrollDelta, visibleTracks);
    }
    
    renderTrackListArea();
  }
}

void OpenPodUIEngine::renderTrackListArea() {
  // Only re-render the track list portion (not header or scrollbar)
  trackList.renderAllTracks(display, 0, BODY_Y, SCREEN_WIDTH - SCROLLBAR_WIDTH);
  // Update scrollbar to reflect new position
  scrollbar.render(display);
}

void OpenPodUIEngine::selectTrack() {
  if (appState.currentUIState == STATE_TRACK_LIST && !isAnimating()) {
    transitionToNowPlaying();
  }
}

void OpenPodUIEngine::returnToList() {
  if (appState.currentUIState == STATE_NOW_PLAYING && !isAnimating()) {
    transitionToTrackList();
  }
}

void OpenPodUIEngine::playSelectedTrack() {
  appState.startPlayback();
  // Here you would typically interface with your audio system
  Serial.println("Starting playback of: " + String(appState.getPlayingTrackName()));
}

void OpenPodUIEngine::togglePlayback() {
  appState.togglePlayback();
  Serial.println(appState.isPlaying ? "Resumed playback" : "Paused playback");
  
  // Update now playing display if visible
  if (appState.currentUIState == STATE_NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::stopPlayback() {
  appState.stopPlayback();
  Serial.println("Stopped playback");
  
  // Update now playing display if visible
  if (appState.currentUIState == STATE_NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::transitionToNowPlaying() {
  appState.currentUIState = STATE_TRANSITIONING;
  appState.targetUIState = STATE_NOW_PLAYING;
  appState.isAnimating = true;
  lastDrawnOffset = 0;

  appState.transitionAnimId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset) {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        if (!width)
          return;
        for (int y = BODY_Y; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT) {
          nowPlaying.renderChunk(display, lastDrawnOffset, y, width);
        }
        display->setScrollOffset(SCREEN_WIDTH - currentOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]() {
        appState.currentUIState = STATE_NOW_PLAYING;
        appState.isAnimating = false;
        renderNowPlaying();
      },
      Easing::easeInOutCubic);
}

void OpenPodUIEngine::transitionToTrackList() {
  appState.currentUIState = STATE_TRANSITIONING;
  appState.targetUIState = STATE_TRACK_LIST;
  appState.isAnimating = true;
  lastDrawnOffset = 0;

  appState.transitionAnimId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset) {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        if (!width)
          return;

        trackList.renderAllTracks(
            display, SCREEN_WIDTH - width - lastDrawnOffset, BODY_Y, width);
        display->setScrollOffset(currentOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]() {
        appState.currentUIState = STATE_TRACK_LIST;
        appState.isAnimating = false;
        renderTrackList();
      },
      Easing::easeInOutCubic);
}

void OpenPodUIEngine::animateScroll(bool scrollingUp) {
  // Setup hardware scrolling
  display->writeCommand(0x33);                  // VSCRDEF
  display->writeData16(HEADER_HEIGHT + MARGIN); // TFA = header area
  display->writeData16(TRACKS_PER_SCREEN * TRACK_HEIGHT); // VSA = scrollable area
  display->writeData16(SCREEN_HEIGHT - HEADER_HEIGHT - MARGIN -
                       (TRACKS_PER_SCREEN * TRACK_HEIGHT)); // BFA

  int startOffset = scrollingUp ? -TRACK_HEIGHT : TRACK_HEIGHT;
  appState.isAnimating = true;

  appState.scrollAnimId = animManager.animate(
      ANIM_CUSTOM, startOffset, 2 * startOffset, 500,
      [this](float offset) {
        int intOffset = (int)offset;
        display->writeCommand(0x37); // VSCRSADD
        display->writeData16(appState.topVisibleTrackIndex * TRACK_HEIGHT + intOffset);
      },
      [this]() {
        display->writeCommand(0x37);
        display->writeData16(0);
        appState.isAnimating = false;
        renderTrackList();
      },
      [](float t) { return Easing::easeOutCubic(t); });
}

void OpenPodUIEngine::pageUp() {
  if (appState.currentUIState != STATE_TRACK_LIST || isAnimating())
    return;

  appState.pageUp();
  updateTrackListScroll();
  updateScrollbar();
  renderTrackList();
}

void OpenPodUIEngine::pageDown() {
  if (appState.currentUIState != STATE_TRACK_LIST || isAnimating())
    return;

  appState.pageDown();
  updateTrackListScroll();
  updateScrollbar();
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