#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "../state/state.h"
#include "../state/fast_scroll.h"
#include "../storage/Music_lookup.h"
#include "header.hpp"
#include "list/list_component.hpp"
#include "nowplaying.hpp"
#include "scrollbar.hpp"
#include "search_view.hpp"
#include "ui_types.h"
#include "../logging.h"
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

  // Fast-scroll letter overlay label (state.getFastScrollActive() says
  // whether it's shown).
  char fastScrollLabelText[4];

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
  void handleFastScrollChanged(const FastScrollEvent *event);

public:
  OpenPodUIEngine(ILI9341_GFX *disp);
  ~OpenPodUIEngine();

  // Lifecycle. begin() draws nothing: the boot screen is still up, and its
  // exit wipe paints the first frame through renderBootChunk().
  void begin();
  // Chunk k (0..7, top to bottom) of the first frame, for the boot wipe.
  void renderBootChunk(int k);
  // Ends the boot wipe: event handlers render normally from here on.
  void endBoot() { booting = false; }
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
  // Points the list at the current overlay label (or none) and sets the
  // scrollbar's wide thumb to match state.getFastScrollActive().
  void updateFastScrollOverlay();
  bool isAnimating() const { return state.getIsAnimating(); }
  bool onSearch() { return state.getCurrentRoute().type == Route_t::SEARCH_RESULTS; }
  // True from begin() until endBoot(): state still flows in, nothing renders.
  bool booting = false;
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
  fastScrollLabelText[0] = '\0';

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
  booting = true;
  LOG_INFO("OpenPod UI Engine initializing...");
  hookToEvents();
  elementList.begin();
  updateTrackListScroll();
  updateScrollbar();
  LOG_INFO("OpenPod UI Engine initialized with event-driven architecture");
}

void OpenPodUIEngine::hookToEvents()
{
  // Register for all state change events
  bool success = state.addEventListener(onStateEvent);

  if (success)
  {
    LOG_CAT(UI, "UI Engine successfully hooked to state events");
  }
  else
  {
    LOG_WARN("Failed to register UI Engine for state events");
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

void OpenPodUIEngine::updateFastScrollOverlay()
{
  bool active = state.getFastScrollActive();
  if (active)
  {
    fastScrollLabel(state.getCurrentTrackName(), state.getTotalElements(),
                    fastScrollLabelText);
  }
  elementList.setOverlayLabel(active ? fastScrollLabelText : nullptr);
  scrollbar.setWide(active);
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
void OpenPodUIEngine::renderBootChunk(int k)
{
  Route_t::RouteType route = state.getCurrentRoute().type;
  if (route == Route_t::NOW_PLAYING || onSearch())
  {
    // No per-chunk path for these: draw them whole on the first step.
    if (k == 0)
    {
      booting = false;
      renderCurrentState();
    }
    return;
  }
  if (k == 0)
  {
    header.render(display, state.getHeaderTitle(), state.getIsPlaying());
    return;
  }
  elementList.renderRows(display, k - 1, k - 1);
  if (k == ELEMENTS_PER_SCREEN)
  {
    scrollbar.render(display);
  }
}

void OpenPodUIEngine::renderCurrentState()
{
  if (booting)
  {
    return;
  }
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

void OpenPodUIEngine::measurePerformance()
{
  uint32_t avg, min, max;
  animManager.getPerformanceStats(avg, min, max);
  LOG_CAT(UI, "Performance: %d FPS, frame times avg %luus, min %luus, max %luus",
          (int)animManager.getFPS(), (unsigned long)avg, (unsigned long)min,
          (unsigned long)max);
}

#include "ui_engine_events.hpp"
#include "ui_engine_transitions.hpp"
