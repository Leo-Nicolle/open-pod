#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "../state/state.h"
#include "header.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
#include "list/tracklist.hpp"
#include "ui_types.h"
#include <Arduino.h>

// Forward declaration for global state
extern State state;

class OpenPodUIEngine {
private:
  ILI9341_GFX *display;
  AnimationManager animManager;

  // UI Components
  HeaderComponent header;
  ScrollbarComponent scrollbar;
  TrackListComponent trackList;
  NowPlayingComponent nowPlaying;

  int lastDrawnOffset;

  // Event handler - must be static to use as callback
  static void onStateEvent(int eventType, void *eventData, EventTarget *source);

  // Instance pointer for static callback access
  static OpenPodUIEngine *instance;

  // Internal UI update methods (called by event handlers)
  void handleTrackSelected(const TrackSelectedEvent *event);
  void handleScrollChanged(const ScrollChangedEvent *event);
  void handlePageChanged(const ScrollChangedEvent *event);
  void handlePlaybackStarted(const PlaybackEvent *event);
  void handlePlaybackPaused(const PlaybackEvent *event);
  void handlePlaybackResumed(const PlaybackEvent *event);
  void handlePlaybackStopped(const PlaybackEvent *event);
  void handleProgressUpdated(const ProgressEvent *event);
  void handleTrackEnded(const PlaybackEvent *event);
  void handleShowingChanged(const ShowingEvent *event);
  void handleAnimationStarted(const AnimationEvent *event);
  void handleAnimationFinished(const AnimationEvent *event);
  void handleTrackListUpdated(const TrackListEvent *event);
  void handleTrackDurationChanged(int *duration);

public:
  OpenPodUIEngine(ILI9341_GFX *disp);
  ~OpenPodUIEngine();

  // Lifecycle
  void begin();
  bool update();
  void hookToEvents(); // Connect to state events

  // User input handlers (these call state methods, not UI methods directly)
  void handleScrollUpInput();
  void handleScrollDownInput();
  void handlePageUpInput();
  void handlePageDownInput();
  void handleSelectInput();
  void handleBackInput();
  void handlePlayPauseInput();
  void handleStopInput();

  // Rendering methods (called by event handlers)
  void renderCurrentState();
  void renderTrackList();
  void renderNowPlaying(int xOffset = 0, int width = SCREEN_WIDTH);
  void renderTrackListArea();

  // Transitions (triggered by events)
  void transitionToNowPlaying();
  void transitionToTrackList();

  // Utility
  float getFPS() const { return animManager.getFPS(); }
  void measurePerformance();

private:
  // Internal helpers
  void updateScrollbar();
  void updateTrackListScroll();
  bool isAnimating() const { return state.getIsAnimating(); }
};

// Static member definition
OpenPodUIEngine *OpenPodUIEngine::instance = nullptr;

// Implementation
OpenPodUIEngine::OpenPodUIEngine(ILI9341_GFX *disp)
    : display(disp), trackList(nullptr, 0), lastDrawnOffset(0) {

  // Set static instance for callback access
  instance = this;
}

OpenPodUIEngine::~OpenPodUIEngine() {
  // Remove event listener on destruction
  state.removeEventListener(onStateEvent);

  // Clear static instance
  if (instance == this) {
    instance = nullptr;
  }
}

void OpenPodUIEngine::begin() {
  display->fillScreen(COLOR_BACKGROUND);
  // Hook up to state events first
  hookToEvents();
  // Initialize components
  trackList.begin();
  updateTrackListScroll();
  updateScrollbar();

  // Render initial state
  renderCurrentState();

  Serial.println(
      "OpenPod UI Engine initialized with event-driven architecture");
}

void OpenPodUIEngine::hookToEvents() {
  // Register for all state change events
  bool success = state.addEventListener(onStateEvent);

  if (success) {
    Serial.println("UI Engine successfully hooked to state events");
  } else {
    Serial.println("Warning: Failed to register UI Engine for state events");
  }
}

bool OpenPodUIEngine::update() {
  // Update animation manager
  bool result = animManager.update();
  return result;
}

// Static event handler - routes events to instance methods
void OpenPodUIEngine::onStateEvent(int eventType, void *eventData,
                                   EventTarget *source) {
  if (!instance) {
    Serial.println(
        "Warning: UI Engine instance not available for event handling");
    return;
  }

  switch (eventType) {
  case EVENT_TRACK_SELECTED:
    instance->handleTrackSelected((TrackSelectedEvent *)eventData);
    break;

  case EVENT_SCROLL_CHANGED:
    instance->handleScrollChanged((ScrollChangedEvent *)eventData);
    break;

  case EVENT_PAGE_CHANGED:
    instance->handlePageChanged((ScrollChangedEvent *)eventData);
    break;

  case EVENT_PLAYBACK_STARTED:
    instance->handlePlaybackStarted((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_PAUSED:
    instance->handlePlaybackPaused((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_RESUMED:
    instance->handlePlaybackResumed((PlaybackEvent *)eventData);
    break;

  case EVENT_PLAYBACK_STOPPED:
    instance->handlePlaybackStopped((PlaybackEvent *)eventData);
    break;

  case EVENT_PROGRESS_UPDATED:
    instance->handleProgressUpdated((ProgressEvent *)eventData);
    break;

  case EVENT_TRACK_ENDED:
    instance->handleTrackEnded((PlaybackEvent *)eventData);
    break;

  case EVENT_UI_STATE_CHANGED:
    instance->handleShowingChanged((ShowingEvent *)eventData);
    break;

  case EVENT_ANIMATION_STARTED:
    instance->handleAnimationStarted((AnimationEvent *)eventData);
    break;

  case EVENT_ANIMATION_FINISHED:
    instance->handleAnimationFinished((AnimationEvent *)eventData);
    break;

  case EVENT_TRACK_LIST_UPDATED:
    instance->handleTrackListUpdated((TrackListEvent *)eventData);
    break;

  case EVENT_TRACK_DURATION_CHANGED:
    instance->handleTrackDurationChanged((int *)eventData);
    break;

  default:
    Serial.print("UI Engine: Unhandled event type: ");
    Serial.println(eventType);
    break;
  }
}

// Event handler implementations
void OpenPodUIEngine::handleTrackSelected(const TrackSelectedEvent *event) {
  Serial.print("UI: Track selected - ");
  Serial.println(event->trackName);

  // Update UI components
  updateTrackListScroll();

  // Re-render if we're in track list view
  if (state.getCurrentShowing() == TRACK_LIST) {
    renderTrackListArea();
  }
}

void OpenPodUIEngine::handleScrollChanged(const ScrollChangedEvent *event) {
  Serial.print("UI: Scroll changed - selected: ");
  Serial.print(event->selectedIndex);
  Serial.print(", top visible: ");
  Serial.println(event->topVisibleIndex);

  updateTrackListScroll();
  updateScrollbar();

  // Handle cache updates for track list
  if (event->topVisibleIndex != event->oldTopVisibleIndex) {
    int scrollDelta = event->topVisibleIndex - event->oldTopVisibleIndex;
    if (scrollDelta > 0) {
      Serial.print("Scrolling down by ");
      Serial.println(scrollDelta);
      trackList.scrollDown(scrollDelta, event->visibleTracks);
    } else {
      trackList.scrollUp(-scrollDelta, event->visibleTracks);
    }
  }

  renderTrackListArea();
}

void OpenPodUIEngine::handlePageChanged(const ScrollChangedEvent *event) {
  Serial.println("UI: Page changed");
  for(int i = 0; i < TRACKS_PER_SCREEN; i++) {
    Serial.print("Track ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(event->visibleTracks[i]);
  }
  updateTrackListScroll();
  trackList.rebuildCache();
  updateScrollbar();
  renderTrackList(); // Full re-render for page changes
}

void OpenPodUIEngine::handlePlaybackStarted(const PlaybackEvent *event) {
  Serial.print("UI: Playback started - ");
  Serial.println(event->trackName);

  // Update now playing component
  nowPlaying.setTrack(event->trackName);
  nowPlaying.setPlayState(true);

  // If we're in now playing view, update display
  if (state.getCurrentShowing() == NOW_PLAYING) {
    renderNowPlaying();
  }

  // Could also trigger transition to now playing view automatically
  // transitionToNowPlaying();
}

void OpenPodUIEngine::handlePlaybackPaused(const PlaybackEvent *event) {
  Serial.println("UI: Playback paused");

  nowPlaying.setPlayState(false);

  if (state.getCurrentShowing() == NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackResumed(const PlaybackEvent *event) {
  Serial.println("UI: Playback resumed");

  nowPlaying.setPlayState(true);

  if (state.getCurrentShowing() == NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackStopped(const PlaybackEvent *event) {
  Serial.println("UI: Playback stopped");

  nowPlaying.setPlayState(false);
  nowPlaying.setProgress(0);

  if (state.getCurrentShowing() == NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleProgressUpdated(const ProgressEvent *event) {
  // Only update if we're showing the now playing screen
  if (state.getCurrentShowing() == NOW_PLAYING) {
    nowPlaying.setProgress(event->position);
    nowPlaying.setTrackLength(event->duration);

    // Only re-render the progress portion to avoid flicker
    // nowPlaying.renderProgressOnly(display);
  }
}

void OpenPodUIEngine::handleTrackEnded(const PlaybackEvent *event) {
  Serial.print("UI: Track ended - ");
  Serial.println(event->trackName);

  // Visual feedback for track end (could show brief message, etc.)
}

void OpenPodUIEngine::handleShowingChanged(const ShowingEvent *event) {
  Serial.print("UI: State changed from ");
  Serial.print(event->oldState);
  Serial.print(" to ");
  Serial.println(event->newState);

  // Handle transitions between states
  if (event->oldState == TRACK_LIST && event->newState == NOW_PLAYING) {
    transitionToNowPlaying();
  } else if (event->oldState == NOW_PLAYING && event->newState == TRACK_LIST) {
    transitionToTrackList();
  } else {
    // Direct state change without transition
    renderCurrentState();
  }
}

void OpenPodUIEngine::handleAnimationStarted(const AnimationEvent *event) {
  Serial.print("UI: Animation started - ID: ");
  Serial.println(event->animationId);
}

void OpenPodUIEngine::handleAnimationFinished(const AnimationEvent *event) {
  Serial.print("UI: Animation finished - ID: ");
  Serial.println(event->animationId);

  // Ensure final render after animation
  renderCurrentState();
}

void OpenPodUIEngine::handleTrackListUpdated(const TrackListEvent *event) {
  Serial.print("UI: Track list updated - ");
  Serial.print(event->totalTracks);
  Serial.println(" tracks");
  trackList.updateTracks(event->tracks, event->totalTracks);
  updateScrollbar();
  // Re-render if we're showing the track list
  if (state.getCurrentShowing() == TRACK_LIST) {
    renderTrackList();
  }
}

void OpenPodUIEngine::handleTrackDurationChanged(int *duration) {
  Serial.print("UI: Track duration changed to ");
  Serial.print(*duration);
  Serial.println(" seconds");

  nowPlaying.setTrackLength(*duration);

  if (state.getCurrentShowing() == NOW_PLAYING) {
    renderNowPlaying();
  }
}

// User input handlers - these call state methods instead of UI methods
void OpenPodUIEngine::handleScrollUpInput() {
  state.scrollUp(); // This will trigger EVENT_SCROLL_CHANGED
}

void OpenPodUIEngine::handleScrollDownInput() {
  state.scrollDown(); // This will trigger EVENT_SCROLL_CHANGED
}

void OpenPodUIEngine::handlePageUpInput() {
  state.pageUp(); // This will trigger EVENT_PAGE_CHANGED
}

void OpenPodUIEngine::handlePageDownInput() {
  state.pageDown(); // This will trigger EVENT_PAGE_CHANGED
}

void OpenPodUIEngine::handleSelectInput() {
  if (state.getCurrentShowing() == TRACK_LIST && !isAnimating()) {
    // This could either start playback or transition to now playing
    state.setShowing(NOW_PLAYING); // This will trigger EVENT_UI_STATE_CHANGED
  }
}

void OpenPodUIEngine::handleBackInput() {
  if (state.getCurrentShowing() == NOW_PLAYING && !isAnimating()) {
    state.setShowing(TRACK_LIST); // This will trigger EVENT_UI_STATE_CHANGED
  }
}

void OpenPodUIEngine::handlePlayPauseInput() {
  if (state.getPlayingTrackIndex() == -1) {
    // No track playing, start playback
    state.startPlayback(); // This will trigger EVENT_PLAYBACK_STARTED
  } else {
    // Toggle current playback
    state.togglePlayback(); // This will trigger EVENT_PLAYBACK_PAUSED/RESUMED
  }
}

void OpenPodUIEngine::handleStopInput() {
  state.stopPlayback(); // This will trigger EVENT_PLAYBACK_STOPPED
}

// Rendering methods (mostly unchanged, but now use state getters)
void OpenPodUIEngine::renderCurrentState() {
  Serial.println("Rendering current state: " +
                 String(state.getCurrentShowing() == TRACK_LIST
                            ? "Track List"
                            : "Now Playing"));

  switch (state.getCurrentShowing()) {
  case TRACK_LIST:
    renderTrackList();
    break;
  case NOW_PLAYING:
    renderNowPlaying();
    break;
  case TRANSITIONING:
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

  const char *trackName = state.getPlayingTrackName();
  if (!trackName) {
    trackName = state.getCurrentTrackName();
  }

  if (trackName) {
    nowPlaying.setTrack(trackName);
  }

  nowPlaying.setTrackLength(state.getTrackDuration());
  nowPlaying.setProgress(state.getPlaybackPosition());
  nowPlaying.setPlayState(state.getIsPlaying());
  nowPlaying.render(display, xOffset, width);
}

void OpenPodUIEngine::renderTrackListArea() {
  // Only re-render the track list portion (not header or scrollbar)
  trackList.renderAllTracks(display, 0, BODY_Y, SCREEN_WIDTH - SCROLLBAR_WIDTH);
  // Update scrollbar to reflect new position
  scrollbar.render(display);
}

void OpenPodUIEngine::updateScrollbar() {
  scrollbar.setScrollData(state.getTotalTracks(), TRACKS_PER_SCREEN,
                          state.getTopVisibleTrackIndex());
}

void OpenPodUIEngine::updateTrackListScroll() {
  trackList.setScroll(state.getSelectedTrackIndex(),
                      state.getTopVisibleTrackIndex());
}

// Transition methods (triggered by UI state change events)
void OpenPodUIEngine::transitionToNowPlaying() {
  // Set transitioning state
  lastDrawnOffset = 0;
  state.setAnimating(true); // Set animating state
  uint32_t animId = animManager.animate(
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
        state.setShowing(NOW_PLAYING); // Update state to now playing
        state.setAnimating(false);     // Animation complete
        // Animation complete - state will be updated via event
        // renderNowPlaying();
      },
      Easing::easeInOutCubic);

  // Store animation ID in state
  // Note: You might want to add this to your State class
}

void OpenPodUIEngine::transitionToTrackList() {
  lastDrawnOffset = 0;
  state.setAnimating(true); // Set animating state
  uint32_t animId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset) {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        if (!width)
          return;

        trackList.renderAllTracks(
            display, SCREEN_WIDTH - width - lastDrawnOffset, BODY_Y, width);
            trackList.red
        display->setScrollOffset(currentOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]() {
        state.setShowing(TRACK_LIST); // Update state to now playing
        state.setAnimating(false);    // Animation complete
        // renderTrackList();
      },
      Easing::easeInOutCubic);
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