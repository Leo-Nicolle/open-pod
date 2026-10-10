#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "../state/state.h"
#include "../storage/Music_lookup.h"
#include "header.hpp"
#include "list/list_component.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
#include "search_view.hpp"
#include "ui_types.h"
#include "../UiVerbose.h"
#include <Arduino.h>

// Forward declaration for global state
extern State state;
extern MusicLookup musicLookup;

class OpenPodUIEngine
{
private:
  ILI9341_GFX *display;
  AnimationManager animManager;

  // UI Components
  HeaderComponent header;
  ScrollbarComponent scrollbar;
  ListComponent elementList;
  NowPlayingComponent nowPlaying;
  SearchView searchView;

  int lastDrawnOffset;
  bool animatingLeftToRight; // Track direction of last transition

  // Volume overlay auto-hide: 0 means no overlay currently shown.
  unsigned long volumeOverlayHideAt = 0;
  static const unsigned long VOLUME_OVERLAY_HIDE_DELAY = 1500; // ms

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
  void handleVolumeChanged(const VolumeEvent *event);
  void handleSeekModeChanged(const SeekModeEvent *event);
  void handleSearchChanged(const SearchEvent *event);

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
  void renderSearch();

  // Transitions (triggered by events)
  void transitionToNowPlaying();
  void transitionToTrackList(bool leftToRight = true);

  // Utility
  float getFPS() const { return animManager.getFPS(); }
  void measurePerformance();

private:
  // Internal helpers
  void updateScrollbar();
  void updateTrackListScroll();
  bool isAnimating() const { return state.getIsAnimating(); }
  bool onSearch() { return state.getCurrentRoute().type == Route_t::SEARCH_RESULTS; }
  // One CHUNK_HEIGHT slice of the body (the list, or the search screen
  // below its header) for the slide transition.
  void renderBodySlice(int x, int chunkY, int width, int height);
  // Resolves trackId -> albumId -> cover location and pushes it into
  // nowPlaying, or clears it if either lookup misses. Shared by
  // handlePlaybackStarted (event-driven) and renderNowPlaying (redraw path,
  // e.g. returning to Now Playing from another screen) - both need it.
  void updateAlbumArtForTrack(int trackId);
};

// Static member definition
OpenPodUIEngine *OpenPodUIEngine::instance = nullptr;

// Implementation
OpenPodUIEngine::OpenPodUIEngine(ILI9341_GFX *disp)
    : display(disp), elementList(nullptr, 0), lastDrawnOffset(0)
{

  // Set static instance for callback access
  instance = this;
}

OpenPodUIEngine::~OpenPodUIEngine()
{
  // Remove event listener on destruction
  state.removeEventListener(onStateEvent);

  // Clear static instance
  if (instance == this)
  {
    instance = nullptr;
  }
}

void OpenPodUIEngine::begin()
{
  display->fillScreen(COLOR_BG);
  Serial.println("OpenPod UI Engine initializing...");
  delay(500);
  hookToEvents();
  Serial.println("2");
  delay(500);
  elementList.begin();
  Serial.println("3");
  delay(500);
  updateTrackListScroll();
  Serial.println("4");
  delay(500);
  updateScrollbar();
  Serial.println("5");
  delay(500);
  renderCurrentState();
  Serial.println(
      "OpenPod UI Engine initialized with event-driven architecture");
}

void OpenPodUIEngine::hookToEvents()
{
  // Register for all state change events
  bool success = state.addEventListener(onStateEvent);

  if (success)
  {
    Serial.println("UI Engine successfully hooked to state events");
  }
  else
  {
    Serial.println("Warning: Failed to register UI Engine for state events");
  }
}

bool OpenPodUIEngine::update()
{
  // Update animation manager
  bool result = animManager.update();

  if (volumeOverlayHideAt != 0 && millis() >= volumeOverlayHideAt)
  {
    volumeOverlayHideAt = 0;
    nowPlaying.setVolumeOverlay(0, false);
    if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
    {
      nowPlaying.renderVolumeOverlay(display);
    }
  }

  return result;
}

// Static event handler - routes events to instance methods
void OpenPodUIEngine::onStateEvent(int eventType, void *eventData,
                                   EventTarget *source)
{
  if (!instance)
  {
    Serial.println(
        "Warning: UI Engine instance not available for event handling");
    return;
  }

  switch (eventType)
  {
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

  case EVENT_VOLUME_CHANGED:
    instance->handleVolumeChanged((VolumeEvent *)eventData);
    break;

  case EVENT_SEEK_MODE_CHANGED:
    instance->handleSeekModeChanged((SeekModeEvent *)eventData);
    break;

  case EVENT_SEARCH_CHANGED:
    instance->handleSearchChanged((SearchEvent *)eventData);
    break;

  default:
    Serial.print("UI Engine: Unhandled event type: ");
    Serial.println(eventType);
    break;
  }
}

void OpenPodUIEngine::handleTrackSelected(const TrackSelectedEvent *event)
{
  Serial.print("UI: Track selected - ");
  Serial.println(event->trackName);

  // Update UI components
  updateTrackListScroll();

  // Re-render if we're in track list view
  if (state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING)
  {
    renderTrackListArea();
  }
}

void OpenPodUIEngine::handleScrollChanged(const ScrollChangedEvent *event)
{
  if (kUiVerbose)
  {
    Serial.printf("UI: Scroll changed - selected: %d, top visible: %d\n",
                  event->selectedIndex, event->topVisibleIndex);
  }
  elementList.setScroll(event->selectedIndex, event->topVisibleIndex);

  updateScrollbar();

  // Handle cache updates for track list
  if (event->topVisibleIndex != event->oldTopVisibleIndex)
  {
    int scrollDelta = event->topVisibleIndex - event->oldTopVisibleIndex;
    if (scrollDelta > 0)
    {
      elementList.scrollDown(scrollDelta, event->visibleElements);
    }
    else
    {
      elementList.scrollUp(-scrollDelta, event->visibleElements);
    }
  }

  renderTrackListArea();
}

void OpenPodUIEngine::handlePageChanged(const ScrollChangedEvent *event)
{
  if (kUiVerbose)
  {
    Serial.println("UI: Page changed");
    for (int i = 0; i < ELEMENTS_PER_SCREEN; i++)
    {
      Serial.printf("Track %d: %s\n", i, event->visibleElements[i]);
    }
  }
  updateTrackListScroll();
  elementList.rebuildCache();
  updateScrollbar();
  renderTrackList(); // Full re-render for page changes
}

void OpenPodUIEngine::handlePlaybackStarted(const PlaybackEvent *event)
{
  Serial.print("UI: Playback started - ");
  Serial.println(event->trackName);

  // Update now playing component
  nowPlaying.setTrack(event->trackName);
  nowPlaying.setPlayState(true);
  updateAlbumArtForTrack(event->trackId);

  // If we're in now playing view, update display
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }

  // Could also trigger transition to now playing view automatically
  // transitionToNowPlaying();
}

void OpenPodUIEngine::handlePlaybackPaused(const PlaybackEvent *event)
{
  Serial.println("UI: Playback paused");

  nowPlaying.setPlayState(false);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackResumed(const PlaybackEvent *event)
{
  Serial.println("UI: Playback resumed");

  nowPlaying.setPlayState(true);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handlePlaybackStopped(const PlaybackEvent *event)
{
  Serial.println("UI: Playback stopped");

  nowPlaying.setPlayState(false);
  nowPlaying.setProgress(0);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleProgressUpdated(const ProgressEvent *event)
{
  // Only update if we're showing the now playing screen
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    nowPlaying.setProgress(event->position);
    nowPlaying.setTrackLength(event->duration);

    // Only redraw the progress bar + time text area, to avoid flicker
    nowPlaying.updateNowPlaying(display);
  }
}

void OpenPodUIEngine::handleTrackEnded(const PlaybackEvent *event)
{
  Serial.print("UI: Track ended - ");
  Serial.println(event->trackName);

  // Visual feedback for track end (could show brief message, etc.)
}

void OpenPodUIEngine::handleNavigationChanged(const RouteChangedEvent *event)
{
  if (kUiVerbose)
  {
    Serial.printf("UI: State changed from %d to %d\n",
                  (int)event->oldRouteType, (int)event->newRouteType);
  }
  nowPlaying.invalidateArt(); // the transition draws over the whole screen
  // return;
  // Handle transitions between states
  if (event->oldRouteType != Route_t::RouteType::NOW_PLAYING &&
      event->newRouteType == Route_t::RouteType::NOW_PLAYING)
  {
    transitionToNowPlaying();
  }
  else if (event->oldRouteType == Route_t::RouteType::NOW_PLAYING &&
           event->newRouteType != Route_t::RouteType::NOW_PLAYING)
  {
    transitionToTrackList(!event->isForward);
  }
  else if (event->oldRouteType != event->newRouteType)
  {
    transitionToTrackList(!event->isForward);
  }
  else
  {
    // Direct state change without transition
    renderCurrentState();
  }
}

void OpenPodUIEngine::handleAnimationStarted(const AnimationEvent *event)
{
  if (kUiVerbose)
  {
    Serial.printf("UI: Animation started - ID: %d\n", (int)event->animationId);
  }
}

void OpenPodUIEngine::handleAnimationFinished(const AnimationEvent *event)
{
  if (kUiVerbose)
  {
    Serial.printf("UI: Animation finished - ID: %d\n", (int)event->animationId);
  }

  // Ensure final render after animation
  renderCurrentState();
}

void OpenPodUIEngine::handleTrackListUpdated(const ListEvent *event)
{
  if (kUiVerbose)
  {
    Serial.printf("UI: Track list updated - %d tracks\n",
                  (int)event->totalElements);
  }
  elementList.setScroll(event->selectedIndex, event->topVisibleIndex);
  elementList.setElements(event->elements, event->totalElements);
  updateScrollbar();
  // Re-render if we're showing the track list
  if (!state.getIsAnimating() && !onSearch())
  {
    renderTrackList();
  }
}

void OpenPodUIEngine::handleTrackDurationChanged(int *duration)
{
  Serial.print("UI: Track duration changed to ");
  Serial.print(*duration);
  Serial.println(" seconds");

  nowPlaying.setTrackLength(*duration);

  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}
void OpenPodUIEngine::handleVolumeChanged(const VolumeEvent *event)
{
  nowPlaying.setVolumeOverlay(event->volume, true);
  volumeOverlayHideAt = millis() + VOLUME_OVERLAY_HIDE_DELAY;
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    nowPlaying.renderVolumeOverlay(display);
  }
}

void OpenPodUIEngine::handleSeekModeChanged(const SeekModeEvent *event)
{
  nowPlaying.setSeekModeActive(event->active);
  if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
}

void OpenPodUIEngine::handleSearchChanged(const SearchEvent *event)
{
  if (state.getIsAnimating() || !onSearch())
    return; // the transition's final render draws it

  const SearchModel &search = state.getSearch();
  uint8_t dirty = event->dirty;
  if (dirty & SEARCH_DIRTY_LAYOUT)
  {
    searchView.renderAll(display, search);
    return;
  }
  if (dirty & SEARCH_DIRTY_HEADER)
    searchView.renderChunk(display, search, 0);
  if ((dirty & SEARCH_DIRTY_RIBBON) && search.getMode() == SearchModel::TYPING)
    searchView.renderChunk(display, search, 1);
  if (dirty & SEARCH_DIRTY_ROWS)
  {
    searchView.renderRows(display, search);
  }
  else if (dirty & SEARCH_DIRTY_SELECTION)
  {
    // first didn't move: only the two rows whose highlight changed
    searchView.renderHitRow(display, search, event->oldSelected);
    searchView.renderHitRow(display, search, search.getSelected());
  }
}

void OpenPodUIEngine::renderSearch()
{
  searchView.renderAll(display, state.getSearch());
}

void OpenPodUIEngine::renderBodySlice(int x, int chunkY, int width, int height)
{
  if (onSearch())
  {
    searchView.renderChunk(display, state.getSearch(),
                           (BODY_Y + chunkY) / CHUNK_HEIGHT, x, width, x,
                           BODY_Y + chunkY);
  }
  else
  {
    elementList.renderRect(display, x, chunkY, width, height, x,
                           BODY_Y + chunkY);
  }
}

// Rendering methods (mostly unchanged, but now use state getters)
void OpenPodUIEngine::renderCurrentState()
{
  if (state.getIsAnimating())
  {
    return; // Don't render if animating
  }
  else if (state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING)
  {
    renderNowPlaying();
  }
  else if (onSearch())
  {
    renderSearch();
  }
  else
  {
    renderTrackList();
  }
}

void OpenPodUIEngine::renderTrackList()
{
  header.render(display, state.getHeaderTitle(), state.getIsPlaying());
  elementList.renderAllElements(display);
  scrollbar.render(display);
}

void OpenPodUIEngine::renderNowPlaying(int xOffset, int width)
{
  header.render(display, state.getHeaderTitle(), state.getIsPlaying());

  const char *trackName = state.getPlayingTrackName();
  if (!trackName)
  {
    trackName = state.getCurrentTrackName();
  }

  if (trackName)
  {
    nowPlaying.setTrack(trackName);
  }
  updateAlbumArtForTrack(state.getPlayingTrackId());

  nowPlaying.setTrackLength(state.getTrackDuration());
  nowPlaying.setProgress(state.getPlaybackPosition());
  nowPlaying.setPlayState(state.getIsPlaying());
  nowPlaying.render(display, xOffset, width);

  if (volumeOverlayHideAt != 0)
  {
    nowPlaying.renderVolumeOverlay(display);
  }
}

void OpenPodUIEngine::updateAlbumArtForTrack(int trackId)
{
  if (trackId < 0)
  {
    nowPlaying.clearAlbumArt();
    nowPlaying.setAlbumName(nullptr);
    nowPlaying.setArtistName(nullptr);
    return;
  }

  uint32_t albumId = musicLookup.getAlbumIdForTrack((uint32_t)trackId);
  album_cover_entry_t cover;
  if (albumId != 0xFFFFFFFFu && musicLookup.getAlbumCoverEntry(albumId, cover))
  {
    nowPlaying.setAlbumArt(cover.offset, cover.length, cover.format);
  }
  else
  {
    nowPlaying.clearAlbumArt();
  }

  // Resolve the album name for the metadata column (best-effort; empty on a
  // miss or when the album_id is out of range).
  char albumName[64];
  if (albumId != 0xFFFFFFFFu &&
      musicLookup.getAlbumName(albumId, albumName, sizeof(albumName)))
  {
    nowPlaying.setAlbumName(albumName);
  }
  else
  {
    nowPlaying.setAlbumName(nullptr);
  }

  // Same for the primary artist (empty on SD cards without
  // track_to_artist.bin).
  uint32_t artistId = musicLookup.getArtistIdForTrack((uint32_t)trackId);
  char artistName[64];
  if (artistId != 0xFFFFFFFFu &&
      musicLookup.getArtistName(artistId, artistName, sizeof(artistName)))
  {
    nowPlaying.setArtistName(artistName);
  }
  else
  {
    nowPlaying.setArtistName(nullptr);
  }
}

void OpenPodUIEngine::renderTrackListArea()
{
  // Only re-render the track list portion (not header or scrollbar)
  elementList.renderAllElements(display, 0, BODY_Y,
                                SCREEN_WIDTH - SCROLLBAR_WIDTH);
  // Update scrollbar to reflect new position
  scrollbar.render(display);
}

void OpenPodUIEngine::updateScrollbar()
{
  scrollbar.setScrollData(state.getTotalElements(), ELEMENTS_PER_SCREEN,
                          state.getTopVisibleIndex());
}

void OpenPodUIEngine::updateTrackListScroll()
{
  elementList.setScroll(state.getSelectedIndex(), state.getTopVisibleIndex());
}

// Transition methods (triggered by UI state change events)
void OpenPodUIEngine::transitionToNowPlaying()
{
  // Set transitioning state
  lastDrawnOffset = 0;
  state.setAnimating(true); // Set animating state

  // Blank the header and scrollbar before the scroll-slide starts, instead
  // of trying to redraw them in place every frame while setScrollOffset()
  // is active. Every GRAM write during the transition races the panel's own
  // free-running refresh (no Tearing Effect line is wired up on this
  // board), and repeatedly re-pushing the same small region each frame to
  // chase the moving scroll offset made that race visible as a persistent
  // shimmer. A one-time blank here has nothing to race - it happens before
  // the scroll offset ever moves - and renderCurrentState() draws the real
  // header (and, for a track-list screen, scrollbar) back once the
  // transition completes and the scroll offset is back at rest.
  display->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_SURFACE);
  // Always blank the scrollbar column, even when neither menu shows a
  // scrollbar: with the hardware scroll it is the seam between the old and
  // new screen that travels across the display, so it must be plain padding.
  display->fillRect(SCROLLBAR_X, BODY_Y, SCREEN_WIDTH - SCROLLBAR_X,
                    BODY_HEIGHT, COLOR_BG);

  uint32_t animId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset)
      {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        // Guard against a non-positive width (e.g. a transient glitch in the
        // eased offset) rather than handing it to the renderer/display -
        // a negative width there can wrap to a huge pixel count and hang.
        if (width <= 0)
          return;
        for (int y = BODY_Y; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT)
        {
          nowPlaying.renderChunk(display, lastDrawnOffset, y, width);
        }
        int scrollOffset = SCREEN_WIDTH - currentOffset;
        display->setScrollOffset(scrollOffset);
        lastDrawnOffset = currentOffset;
      },
      [this]()
      {
        state.setAnimating(false);
      },
      Easing::easeInOutCubic);
}

void OpenPodUIEngine::transitionToTrackList(bool leftToRight)
{
  lastDrawnOffset = 0;
  animatingLeftToRight = leftToRight;
  state.setAnimating(true);

  // Temporary: reset per-frame timing stats so completeFunc's
  // measurePerformance() call reports numbers for just THIS transition, to
  // find out empirically whether leftToRight=true (back navigation) really
  // renders slower per frame than leftToRight=false, or whether it's a
  // one-time hitch (e.g. State::back()'s extra restoreSelectionState() call)
  // before the animation starts. Remove once that's answered.
  animManager.resetPerformanceStats();

  // Blank the header and scrollbar before the scroll-slide starts - see the
  // matching comment in transitionToNowPlaying() for why (no Tearing Effect
  // line on this board, so repeatedly re-pushing them each frame to chase
  // the moving scroll offset raced the panel's own refresh and shimmered).
  // renderCurrentState() draws the real ones back once this completes.
  display->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_SURFACE);
  // Always blank the scrollbar column, even when neither menu shows a
  // scrollbar: with the hardware scroll it is the seam between the old and
  // new screen that travels across the display, so it must be plain padding.
  display->fillRect(SCROLLBAR_X, BODY_Y, SCREEN_WIDTH - SCROLLBAR_X,
                    BODY_HEIGHT, COLOR_BG);

  uint32_t animId = animManager.animate(
      ANIM_CUSTOM, 0, SCREEN_WIDTH, 800,
      [this](float offset)
      {
        int currentOffset = (int)offset;
        int width = currentOffset - lastDrawnOffset;
        // Guard against a non-positive width (e.g. a transient glitch in the
        // eased offset) rather than handing it to the renderer/display -
        // a negative width there can wrap to a huge pixel count and hang.
        if (width <= 0)
          return;

        // Fixed coordinate calculations
        int x = animatingLeftToRight ? SCREEN_WIDTH - currentOffset
                                     : lastDrawnOffset;
        // g_buffers' scratch buffers are sized for one CHUNK_HEIGHT-tall
        // strip (SCREEN_WIDTH * CHUNK_HEIGHT pixels), same as every other
        // renderer in this codebase respects. Rendering the full BODY_HEIGHT
        // in one renderRect call (as this used to) overflows that buffer
        // whenever a frame's width grows past ~46px - which normally only
        // takes ~7px/frame, but does happen when a slow tick (e.g. the audio
        // pipeline's PSRAM refill while a track is playing) delays ui.update()
        // and several frames' worth of movement land in a single call.
        for (int chunkY = 0; chunkY < BODY_HEIGHT; chunkY += CHUNK_HEIGHT)
        {
          int chunkHeight = min(CHUNK_HEIGHT, BODY_HEIGHT - chunkY);
          renderBodySlice(x, chunkY, width, chunkHeight);
        }

        // Fixed scroll offset
        int scrollOffset = animatingLeftToRight ? currentOffset
                                                : SCREEN_WIDTH - currentOffset;
        display->setScrollOffset(scrollOffset);
        lastDrawnOffset = currentOffset;
      },
      [this, leftToRight]()
      {
        state.setAnimating(false);
        if (kUiVerbose)
        {
          measurePerformance();
        }
      },
      Easing::easeInOutCubic);
}
void OpenPodUIEngine::measurePerformance()
{
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
}