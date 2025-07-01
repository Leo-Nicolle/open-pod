#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "../state/state.h"
#include "header.hpp"
#include "list/list_component.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
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
  ListComponent elementList;
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
  void handleNavigationChanged(const RouteChangedEvent *event);
  void handleAnimationStarted(const AnimationEvent *event);
  void handleAnimationFinished(const AnimationEvent *event);
  void handleTrackListUpdated(const ListEvent *event);
  void handleTrackDurationChanged(int *duration);

public:
  OpenPodUIEngine(ILI9341_GFX *disp);
  ~OpenPodUIEngine();

  // Lifecycle
  void begin();
  bool update();
  void hookToEvents(); // Connect to state events

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
    : display(disp), elementList(nullptr, 0), lastDrawnOffset(0) {

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
  elementList.begin();
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

  case EVENT_ROUTE_CHANGED:
    instance->handleNavigationChanged((RouteChangedEvent *)eventData);
    break;

  case EVENT_ANIMATION_STARTED:
    instance->handleAnimationStarted((AnimationEvent *)eventData);
    break;

  case EVENT_ANIMATION_FINISHED:
    instance->handleAnimationFinished((AnimationEvent *)eventData);
    break;

  case EVENT_TRACK_LIST_UPDATED:
    instance->handleTrackListUpdated((ListEvent *)eventData);
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

void OpenPodUIEngine::handleTrackSelected(const TrackSelectedEvent *event) {
  Serial.print("UI: Track selected - ");
  Serial.println(event->trackName);

  // Update UI components
  updateTrackListScroll();

  // Re-render if we're in track list view
  if (state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING) {
    renderTrackListArea();
  }
}

void OpenPodUIEngine::handleScrollChanged(const ScrollChangedEvent *event) {
  Serial.print("UI: Scroll changed - selected: ");
  Serial.print(event->selectedIndex);
  Serial.print(", top visible: ");
  Serial.println(event->topVisibleIndex);
  Serial.print("State selected: ");
  int selectedIndex = state.getSelectedIndex();
  int topVisibleIndex = state.getTopVisibleIndex();
  Serial.print(selectedIndex);
  Serial.print(", top visible: ");
  Serial.println(topVisibleIndex);
  elementList.setScroll(event->selectedIndex, event->topVisibleIndex);

  updateScrollbar();

  // Handle cache updates for track list
  if (event->topVisibleIndex != event->oldTopVisibleIndex) {
    int scrollDelta = event->topVisibleIndex - event->oldTopVisibleIndex;
    if (scrollDelta > 0) {
      Serial.print("Scrolling down by ");
      Serial.println(scrollDelta);
      elementList.scrollDown(scrollDelta, event->visibleElements);
    } else {
      elementList.scrollUp(-scrollDelta, event->visibleElements);
    }
  }

  renderTrackListArea();
}

void OpenPodUIEngine::handlePageChanged(const ScrollChangedEvent *event) {
  Serial.println("UI: Page changed");
  for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
    Serial.print("Track ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(event->visibleElements[i]);
  }
  updateTrackListScroll();
  elementList.rebuildCache();
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
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  }

  // Could also trigger transition to now playing view automatically
  // transitionToNowPlaying();
}

void OpenPodUIEngine::handlePlaybackPaused(const PlaybackEvent *event) {
  Serial.println("UI: Playback paused");

  nowPlaying.setPlayState(false);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackResumed(const PlaybackEvent *event) {
  Serial.println("UI: Playback resumed");

  nowPlaying.setPlayState(true);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackStopped(const PlaybackEvent *event) {
  Serial.println("UI: Playback stopped");

  nowPlaying.setPlayState(false);
  nowPlaying.setProgress(0);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleProgressUpdated(const ProgressEvent *event) {
  // Only update if we're showing the now playing screen
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
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

void OpenPodUIEngine::handleNavigationChanged(const RouteChangedEvent *event) {
  Serial.print("UI: State changed from ");
  Serial.print(event->oldRouteType);
  Serial.print(" to ");
  Serial.println(event->newRouteType);
// return;
  // Handle transitions between states
  if (event->oldRouteType != Route_t::RouteType::NOW_PLAYING &&
      event->newRouteType == Route_t::RouteType::NOW_PLAYING) {
    transitionToNowPlaying();
  } else if (event->oldRouteType == Route_t::RouteType::NOW_PLAYING &&
             event->newRouteType != Route_t::RouteType::NOW_PLAYING) {
    transitionToTrackList();
  } else if (event->oldRouteType != event->newRouteType) {
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

void OpenPodUIEngine::handleTrackListUpdated(const ListEvent *event) {
  Serial.print("UI: Track list updated - ");
  Serial.print(event->totalElements);
  Serial.println(" tracks");
  elementList.setElements(event->elements, event->totalElements);
  updateScrollbar();
  // Re-render if we're showing the track list
  if (state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING) {
    renderTrackList();
  }
}

void OpenPodUIEngine::handleTrackDurationChanged(int *duration) {
  Serial.print("UI: Track duration changed to ");
  Serial.print(*duration);
  Serial.println(" seconds");

  nowPlaying.setTrackLength(*duration);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  }
}
// Rendering methods (mostly unchanged, but now use state getters)
void OpenPodUIEngine::renderCurrentState() {
  if (state.getIsAnimating()) {
    return; // Don't render if animating
  } else if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    renderNowPlaying();
  } else {
    renderTrackList();
  }
}

void OpenPodUIEngine::renderTrackList() {
  header.render(display);
  elementList.renderAllElements(display);
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
  elementList.renderAllElements(display, 0, BODY_Y,
                                SCREEN_WIDTH - SCROLLBAR_WIDTH);
  // Update scrollbar to reflect new position
  scrollbar.render(display);
}

void OpenPodUIEngine::updateScrollbar() {
  scrollbar.setScrollData(state.getTotalElements(), ELEMENTS_PER_SCREEN,
                          state.getTopVisibleIndex());
}

void OpenPodUIEngine::updateTrackListScroll() {
  elementList.setScroll(state.getTotalElements(), state.getTopVisibleIndex());
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
      [this]() { state.setAnimating(false); }, Easing::easeInOutCubic);
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
        int startX = SCREEN_WIDTH - currentOffset;
        if (startX + width > SCROLLBAR_X && startX + width < SCREEN_WIDTH) {
          scrollbar.render(display, startX);
        }

        elementList.renderRect(display, SCREEN_WIDTH - lastDrawnOffset, 0,
                               width, SCREEN_HEIGHT - BODY_Y,
                               SCREEN_WIDTH - currentOffset);
        display->setScrollOffset(currentOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]() { state.setAnimating(false); }, Easing::easeInOutCubic);
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