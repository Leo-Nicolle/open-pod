#pragma once
#include "../rendering/ILI9341_GFX.h"
#include "../rendering/animation_manager.h"
#include "../state/state.h"
#include "../state/fast_scroll.h"
#include "../storage/Music_lookup.h"
#include "../bluetooth/BtModel.h"
#include "../state/bt_menu.h"
#include "../state/app_toast.h"
#include "../state/podcast_menu.h"
#include "header.hpp"
#include "list/list_component.hpp"
#include "menu_view.hpp"
#include "nowplaying.hpp"
#include "podcast_view.hpp"
#include "scrollbar.hpp"
#include "search_view.hpp"
#include "ui_types.h"
#include "../logging.h"
#include <Arduino.h>

// Forward declaration for global state
extern State state;
extern MusicLookup musicLookup;
extern BtModel btModel;
extern BtMenu btMenu;
extern PodcastMenu podcastMenu;

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
  MenuView menuView;
  PodcastView podcastView;

  int lastDrawnOffset;
  bool animatingLeftToRight; // Track direction of last transition
  uint32_t transitionAnimId = 0; // the running route slide, if any

  // Volume overlay auto-hide: 0 means no overlay currently shown.
  unsigned long volumeOverlayHideAt = 0;
  static const unsigned long VOLUME_OVERLAY_HIDE_DELAY = 1500; // ms

  // Bluetooth: toast timeout and menu redraw pacing (see updateBluetooth()).
  unsigned long toastHideAt = 0;
  uint8_t toastSerial = 0; // btModel.toastSerial() last shown
  uint8_t appToastSerial = 0; // appToast.serial() last shown
  static const unsigned long TOAST_MS = 2500;
  unsigned long lastMenuRefresh = 0;
  static const unsigned long MENU_REFRESH_MS = 20;

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
  void renderMenu();
  void renderPodcasts();

  // Transitions (triggered by events)
  void transitionToNowPlaying();
  void transitionToTrackList(bool leftToRight = true);

  // Utility
  float getFPS() const { return animManager.getFPS(); }
  // An animation (slide, scroll) still has frames to draw.
  bool isBusy() const { return animManager.busy(); }
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
  // Settings and the Bluetooth screens (drawn by menuView from btMenu).
  bool onMenu() { return BtMenu::isMenuRoute(state.getCurrentRoute().type); }
  // The podcast screens (drawn by podcastView from podcastMenu).
  bool onPodcast()
  {
    return PodcastMenu::isPodcastRoute(state.getCurrentRoute().type);
  }
  // Header status (toast, Bluetooth icon, codec tag) and menu redraws.
  void updateBluetooth();
  // Repaints the header chunk of whatever screen is shown.
  void repaintHeader();
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
    : display(disp), elementList(nullptr, 0), menuView(header),
      podcastView(header),
      lastDrawnOffset(0)
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
  updateBluetooth();

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

void OpenPodUIEngine::renderMenu()
{
  btMenu.sync(state.getCurrentRoute(), millis());
  menuView.renderAll(display, btMenu, state.getIsPlaying());
}

void OpenPodUIEngine::renderPodcasts()
{
  podcastMenu.sync(state.getCurrentRoute());
  podcastView.renderAll(display, podcastMenu, state.getIsPlaying());
}

void OpenPodUIEngine::repaintHeader()
{
  if (onPodcast())
  {
    podcastView.invalidateHeader();
    podcastView.renderChunk(display, podcastMenu, state.getIsPlaying(), 0);
  }
  else if (onMenu())
  {
    menuView.invalidateHeader();
    menuView.renderChunk(display, btMenu, state.getIsPlaying(), 0);
  }
  else if (onSearch())
  {
    searchView.renderChunk(display, state.getSearch(), 0);
  }
  else
  {
    header.render(display, state.getHeaderTitle(), state.getIsPlaying(),
                  state.getCurrentRoute().type == Route_t::NOW_PLAYING);
  }
}

void OpenPodUIEngine::updateBluetooth()
{
  unsigned long now = millis();
  uikit::HeaderStatus &hs = uikit::headerStatus;
  bool headerDirty = false;

  if (btModel.toastSerial() != toastSerial)
  {
    toastSerial = btModel.toastSerial();
    hs.toast = &btModel.lastToast();
    hs.toastActive = true;
    toastHideAt = now + TOAST_MS;
    headerDirty = true;
  }
  else if (appToast.serial() != appToastSerial)
  {
    appToastSerial = appToast.serial();
    hs.toast = &appToast.last();
    hs.toastActive = true;
    toastHideAt = now + TOAST_MS;
    headerDirty = true;
  }
  else if (hs.toastActive && (long)(now - toastHideAt) >= 0)
  {
    hs.toastActive = false;
    headerDirty = true;
  }

  BtModel::Icon icon = btModel.icon();
  bool blinkOn = icon == BtModel::ICON_CONNECTING && (now / 500) % 2 == 0;
  bool iconDirty = icon != hs.icon || blinkOn != hs.blinkOn;
  hs.icon = icon;
  hs.blinkOn = blinkOn;

  // Codec tag on Now Playing: the codec in use, hidden when not connected.
  const char *codec = nullptr;
  int slot = btModel.linkSlot();
  if (btModel.link() == BtModel::LINK_CONNECTED && slot >= 0 &&
      btModel.device(slot).codecInUse != BtModel::CODEC_AUTO)
    codec = BtModel::codecName(btModel.device(slot).codecInUse);
  if (codec != hs.codec)
  {
    hs.codec = codec;
    if (state.getCurrentRoute().type == Route_t::NOW_PLAYING)
      headerDirty = true;
  }

  // Mid-transition the final render draws the current state anyway.
  if (booting || state.getIsAnimating())
    return;

  if (onPodcast())
  {
    // Rows change with the cursor and the listening state: redraw what moved.
    if (headerDirty || now - lastMenuRefresh >= MENU_REFRESH_MS)
    {
      lastMenuRefresh = now;
      podcastMenu.sync(state.getCurrentRoute());
      if (headerDirty)
        podcastView.invalidateHeader();
      podcastView.renderChanged(display, podcastMenu, state.getIsPlaying());
    }
  }
  else if (onMenu())
  {
    if (headerDirty || now - lastMenuRefresh >= MENU_REFRESH_MS)
    {
      lastMenuRefresh = now;
      btMenu.sync(state.getCurrentRoute(), now);
      if (headerDirty)
        menuView.invalidateHeader();
      menuView.renderChanged(display, btMenu, state.getIsPlaying());
    }
  }
  else if (headerDirty)
  {
    repaintHeader();
  }
  if (iconDirty && !headerDirty)
    header.renderIconBox(display);
}

void OpenPodUIEngine::renderBodySlice(int x, int chunkY, int width, int height)
{
  if (onSearch())
  {
    searchView.renderChunk(display, state.getSearch(),
                           (BODY_Y + chunkY) / CHUNK_HEIGHT, x, width, x,
                           BODY_Y + chunkY);
  }
  else if (onMenu())
  {
    menuView.renderChunk(display, btMenu, state.getIsPlaying(),
                         (BODY_Y + chunkY) / CHUNK_HEIGHT, x, width, x,
                         BODY_Y + chunkY);
  }
  else if (onPodcast())
  {
    podcastView.renderChunk(display, podcastMenu, state.getIsPlaying(),
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
  else if (onMenu())
  {
    renderMenu();
  }
  else if (onPodcast())
  {
    renderPodcasts();
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
  header.render(display, state.getHeaderTitle(), state.getIsPlaying(), true);

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
  nowPlaying.setPodcastMode(false);
  if (trackId < 0)
  {
    nowPlaying.clearAlbumArt();
    nowPlaying.setAlbumName(nullptr);
    nowPlaying.setArtistName(nullptr);
    return;
  }
  if (isPodcastTrack((uint32_t)trackId))
  {
    // Show art, the show (its folder) and, for a series, the show folder
    // above it; for a dated episode its full date.
    uint32_t episode = podcastEpisodeOf((uint32_t)trackId);
    uint32_t folder = musicLookup.podcastEpisodeFolder(episode);
    nowPlaying.setPodcastMode(true);
    album_cover_entry_t cover;
    if (musicLookup.getPodcastCoverEntry(folder, cover))
      nowPlaying.setAlbumArt(cover.offset, cover.length, cover.format, true);
    else
      nowPlaying.clearAlbumArt();
    char show[64];
    nowPlaying.setArtistName(
        musicLookup.podcastFolderName(folder, show, sizeof(show)) ? show
                                                                  : nullptr);
    char third[64] = "";
    uint32_t parent = musicLookup.podcastFolderParent(folder);
    if (musicLookup.podcastFolderIsSeries(folder) &&
        parent != PodcastSource::NONE && parent != 0)
      musicLookup.podcastFolderName(parent, third, sizeof(third));
    else
      podfmt::longDate(musicLookup.podcastEpisodeDate(episode), third,
                       sizeof(third));
    nowPlaying.setAlbumName(third[0] ? third : nullptr);
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
