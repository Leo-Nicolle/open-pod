#pragma once
#include "../storage/Music_lookup.h"
#include "../ui/theme.h"
#include "../ui/ui_types.h"
#include "event-target.h"
#include "podcast_source.h"
#include "podcast_state.h"
#include "router.h"
#include "search.h"
#include <Arduino.h>

// Lists from the music index are loaded as a window of LOOKUP_WINDOW names
// around the visible rows (any length of list); static lists (menus) are
// plain arrays. The window's names live in an 8 KB buffer.
#define ELEMENTS_BUFFER_SIZE 8192
#define MAX_STRING_POINTERS LOOKUP_WINDOW

// Event types for state changes
enum StateEvent {
  // Navigation events
  EVENT_TRACK_SELECTED = 1000,
  EVENT_SCROLL_CHANGED,
  EVENT_PAGE_CHANGED,
  EVENT_ROUTE_CHANGED,

  // Playback events
  EVENT_PLAYBACK_STARTED,
  EVENT_PLAYBACK_PAUSED,
  EVENT_PLAYBACK_RESUMED,
  EVENT_PLAYBACK_STOPPED,
  EVENT_PROGRESS_UPDATED,
  EVENT_TRACK_ENDED,

  // UI events
  EVENT_ANIMATION_STARTED,
  EVENT_ANIMATION_FINISHED,

  // Data events
  EVENT_TRACK_LIST_UPDATED,
  EVENT_TRACK_DURATION_CHANGED,
  EVENT_DATA_LOADED,

  // Now Playing input events
  EVENT_VOLUME_CHANGED,
  EVENT_SEEK_MODE_CHANGED,

  // Search screen
  EVENT_SEARCH_CHANGED,

  // List fast-scroll letter overlay shown / hidden
  EVENT_FAST_SCROLL_CHANGED
};

// Event data structures
struct TrackSelectedEvent {
  int trackIndex;
  const char *trackName;
};

struct ScrollChangedEvent {
  int selectedIndex;
  int topVisibleIndex;
  int oldSelectedIndex;
  int oldTopVisibleIndex;
  const char **visibleElements; // Current visible elements after scroll
};

struct PlaybackEvent {
  int trackIndex; // Row position in the currently displayed track list
  int trackId;    // Global track ID, as used by MusicLookup::getTrackPath()
  const char *trackName;
  bool isPlaying;
  int startAt; // EVENT_PLAYBACK_STARTED: seconds to seek to (podcast resume)
};

struct ProgressEvent {
  int position;     // in seconds
  int duration;     // in seconds
  float percentage; // 0.0 to 1.0
};

struct AnimationEvent {
  uint32_t animationId;
  int animationType;
};

struct ListEvent {
  int totalElements;
  int selectedIndex;
  int topVisibleIndex;
  // The rows on screen (ELEMENTS_PER_SCREEN, nullptr past the end); valid
  // until the list scrolls or changes.
  const char **visibleElements;
};

struct RouteChangedEvent {
  Route_t::RouteType oldRouteType;
  Route_t::RouteType newRouteType;
  uint32_t entityId;
  uint32_t topVisibleIndex;
  uint32_t selectedIndex;
  const char *entityName;
  bool isForward;
  bool canGoBack;
};

struct VolumeEvent {
  int volume; // 0-100
};

struct SeekModeEvent {
  bool active;
};

struct FastScrollEvent {
  bool active;
  uint8_t letters; // the overlay's letter count (0 when hidden)
};

// Which parts of the search screen a change touched, so the UI redraws
// only those strips.
enum SearchDirty : uint8_t {
  SEARCH_DIRTY_HEADER = 1 << 0,    // query text / caret
  SEARCH_DIRTY_RIBBON = 1 << 1,    // ribbon cursor
  SEARCH_DIRTY_ROWS = 1 << 2,      // result rows + scrollbar
  SEARCH_DIRTY_LAYOUT = 1 << 3,    // ribbon shown/dropped: everything
  SEARCH_DIRTY_SELECTION = 1 << 4, // highlight moved from oldSelected
};

struct SearchEvent {
  uint8_t dirty;
  int oldSelected;
  int oldFirst;
};

class State : public EventTarget {
private:
  // String buffer and pointers for current data
  char elementsBuffer[ELEMENTS_BUFFER_SIZE];
  const char* stringPointers[MAX_STRING_POINTERS];
  
  // The current list: a static array (menus, setElements()), or, when
  // `elements` is null, a window over the music index: names
  // [windowStart, windowStart + windowCount) in stringPointers, reloaded
  // from windowLookup as the visible rows move (see ensureWindow()).
  const char **elements;
  int totalElements;
  const char* visibleElements[ELEMENTS_PER_SCREEN];
  MusicLookup *windowLookup;
  int windowStart;
  int windowCount;

  // Current playback state
  int playingTrackIndex;
  int playingTrackId;
  bool isPlaying;
  int playbackPosition; // in seconds
  int trackDuration;    // in seconds
  char playingTrackName[128]; // owned copy - elements[] may point at a
                              // different list by the time this is read

  // Source-list context captured at playback-start time (not re-derived
  // later from the live router stack or the currently-displayed list -
  // both can point elsewhere once the user browses away from Now Playing
  // while a track keeps playing).
  Route_t::RouteType playingListParentType;
  uint32_t playingListEntityId;
  int playingListTotalTracks;

  // Podcasts: the tables and the listening state (null without them).
  PodcastSource *podcastSource;
  PodcastState *podcastState;
  int lastPodcastSave; // playback position last saved, seconds

  int volume;          // 0-100 logical volume
  bool seekModeActive; // Now Playing seek-mode (center press to enter)
  bool repeatTrack = false; // see setRepeatTrack()
  uint8_t fastScrollLetters; // list letter overlay: letters shown, 0 = hidden

  // Navigation state - RENAMED for generalization
  int selectedIndex;        // Currently selected item (was selectedTrackIndex)
  int topVisibleIndex;      // Top visible item (was topVisibleTrackIndex)  // Animation state
  bool isAnimating;
  uint32_t scrollAnimId;
  uint32_t transitionAnimId;

  // Rotation state
  bool isRotatedMode;

  // Route management
  Router router;

  // Search screen model (kept across routes, so back() returns to it)
  SearchModel search;
  void emitSearchChanged(uint8_t dirty, int oldSelected, int oldFirst);

  void updateVisibleElements();
  // Makes rows [first, last] available, reloading the window if needed.
  void ensureWindow(int first, int last);
  // Loads up to MAX_STRING_POINTERS names of the current route's list
  // from `start`; returns how many.
  int loadWindow(int start);
  // Names [start, start + n) of the current route's list into buf/ptrs.
  int loadNames(int start, char *buf, uint32_t size, const char **ptrs,
                uint32_t n);
  // Which full list (MusicLookup letter tables: 0 artists, 1 albums, 2
  // genres, 3 tracks) the current route shows, -1 for a scoped or static one.
  int globalList();
  static bool keyAt(void *self, int index, int letters, char *out);
  // Name of row i of the current list, nullptr if not loaded.
  const char *elementAt(int i) const;
  // Length of the current route's list in the music index.
  int routeListLength(MusicLookup &musicLookup);
  // Shows the current route's list (windowed) with `count` rows.
  void setWindowedElements(MusicLookup &musicLookup, int count);
  void restoreSelectionState();
  // Podcast playback (state.cpp).
  int32_t nextSeriesEpisode();
  void podcastEnded(MusicLookup &musicLookup);
  // Resolves the global track ID for a row position in the TRACKS list,
  // given the current route is NOW_PLAYING (TRACKS is one level below it).
  uint32_t resolveTrackId(MusicLookup &musicLookup, int index);
public:
  // Constructor with defaults
  State();

  // Track management
  void setElements(const char **elementList, int count);
  
  // SIMPLIFIED NAVIGATION - only 3 core methods
  void goToSelected(MusicLookup& musicLookup);  // Navigate into selected item
  bool back(MusicLookup& musicLookup);          // Go back one level
  void backToMain(MusicLookup& musicLookup);    // Go back to root menu
  void goToNowPlaying(MusicLookup& musicLookup); // Jump to Now Playing from anywhere
  // Back to the route of type `type` below the current one, in one step
  // (e.g. from the forget confirmation straight to the device list).
  bool backTo(Route_t::RouteType type, MusicLookup &musicLookup);
  // Opens a Settings/Bluetooth screen (their rows come from BtMenu) with
  // the cursor on row `selected`.
  void pushMenuRoute(Route_t::RouteType type, uint32_t entityId,
                     const char *title, int selected);
  
  // Scroll navigation (still needed for UI)
  void scrollUp();
  void scrollDown();
  void pageUp();
  void pageDown();
  // Moves the highlight to `index`, keeping its row on screen where it was;
  // one EVENT_PAGE_CHANGED.
  void jumpTo(int index);
  // Fast scroll: |clicks| jumps to the next (clicks < 0, down) or previous
  // `letters`-letter bucket (state/fast_scroll.h), then one jumpTo().
  void letterJump(int clicks, int letters);
  
  // Playback control. startAt: seconds to start from.
  void startPlayback(int trackIndex, uint32_t trackId, MusicLookup &musicLookup,
                     int startAt = 0);
  void togglePlayback();
  void stopPlayback();
  void updateProgress(int position);
  void setTrackDuration(int duration);
  void notifyTrackEnded(MusicLookup &musicLookup);
  // Returns true and sets nextTrackId to the track after the one currently
  // playing in the active play list (same source-list context as
  // resolveTrackId/notifyTrackEnded); false at end of list or if nothing is
  // playing. Used to prefetch the next track's data ahead of time instead of
  // only resolving it reactively once the current track ends.
  bool getNextTrackId(MusicLookup &musicLookup, uint32_t &nextTrackId);
  // Skip to the next track in the active play list; false (no-op) at end of
  // list or if nothing is playing.
  bool nextTrack(MusicLookup &musicLookup);
  // iPod-style previous: restarts the current track if it has played for
  // more than PREVIOUS_RESTART_SECONDS (or is the first one), otherwise goes
  // to the previous track. Since a restart resets the position to 0, two
  // quick calls (double-click) go to the previous track. False if nothing
  // is playing.
  bool previousTrack(MusicLookup &musicLookup);
  static const int PREVIOUS_RESTART_SECONDS = 1;

  // Podcasts (state/podcast_menu.h). Without them the Podcasts screen
  // shows its empty hint.
  void setPodcasts(PodcastSource *source, PodcastState *listening) {
    podcastSource = source;
    podcastState = listening;
  }
  PodcastSource *getPodcastSource() { return podcastSource; }
  PodcastState *getPodcastState() { return podcastState; }
  // Plays an episode within its folder, from its saved position (minus
  // RESUME_REWIND_SECONDS) unless fromStart, and opens Now Playing.
  void startPodcast(uint32_t episode, bool fromStart, MusicLookup &musicLookup);
  // Whether the playing track is a podcast episode.
  bool isPlayingPodcast() const {
    return playingTrackIndex >= 0 && isPodcastTrack((uint32_t)playingTrackId);
  }
  // Saves the playing episode's position (leaving: paused, stopped or
  // switched, so a position in its tail counts as played, see
  // PodcastState::playedTail). Called on those and every
  // PODCAST_SAVE_SECONDS while playing; call it before powering off too.
  void savePodcastPosition(bool leaving);
  // Where an episode starts: a started one resumes RESUME_REWIND_SECONDS
  // before its saved position (with a toast), anything else from the top.
  int podcastStartAt(uint32_t episode);
  // Plays the playing folder's own episode at `index` (next/previous).
  bool playPodcastAt(int index, MusicLookup &musicLookup);
  static const int RESUME_REWIND_SECONDS = 5;
  static const int PODCAST_SAVE_SECONDS = 30;

  // Volume control (Now Playing wheel scroll)
  void setVolume(int vol);
  void increaseVolume(int step = 2);
  void decreaseVolume(int step = 2);
  int getVolume() const { return volume; }

  // Seek mode (Now Playing center press)
  // Repeat the playing track instead of moving on at its end (music only;
  // podcasts keep their series chaining). The test bench loops its
  // background track this way.
  void setRepeatTrack(bool on) { repeatTrack = on; }
  bool getRepeatTrack() const { return repeatTrack; }

  void enterSeekMode();
  void exitSeekMode();
  bool getSeekModeActive() const { return seekModeActive; }

  // Fast-scroll letter overlay on lists, showing `letters` (1-3) letters of
  // the highlighted title, 0 = hidden. Emits EVENT_FAST_SCROLL_CHANGED on
  // change only. Route changes clear it without an event (the transition
  // redraws the whole screen anyway).
  void setFastScroll(int letters);
  bool getFastScrollActive() const { return fastScrollLetters > 0; }
  int getFastScrollLetters() const { return fastScrollLetters; }

  // UI state management
  void setAnimating(bool animating, uint32_t animId = 0);
  // Navigating while a route transition still plays cuts it short: the
  // EVENT_ANIMATION_FINISHED lets the UI snap to the screen it was heading to
  // before this route changes again, so the new transition starts from rest.
  void endTransition() { setAnimating(false); }
  void setRotation(bool rotated);
  
  // Data loading (internal use)
  void loadCurrentRouteData(MusicLookup& musicLookup);
  
  // Helper methods for goToSelected
  uint32_t getSelectedEntityId(MusicLookup& musicLookup);
  
  // Getters
  const char *getCurrentTrackName() const;
  const char *getPlayingTrackName() const;
  // getter for visible elements
  const char* const* getVisibleElements() const { return visibleElements; }
  bool needsScrollUpdate(int oldSelected, int oldTopVisible) const;
  void getVisibleTrackIndices(int &startIndex, int &endIndex) const;
  int getRelativeSelectedIndex() const;
  
  // Public accessors for UI components - UPDATED NAMES
  int getTotalElements() const { return totalElements; }
  int getSelectedIndex() const { return selectedIndex; }
  int getTopVisibleIndex() const { return topVisibleIndex; }
  int getPlayingTrackIndex() const { return playingTrackIndex; }
  int getPlayingTrackId() const { return playingTrackId; }
  bool getIsPlaying() const { return isPlaying; }
  int getPlaybackPosition() const { return playbackPosition; }
  int getTrackDuration() const { return trackDuration; }
  bool getIsAnimating() const { return isAnimating; }
  bool getIsRotatedMode() const { return isRotatedMode; }
  uint32_t getScrollAnimId() const { return scrollAnimId; }
  uint32_t getTransitionAnimId() const { return transitionAnimId; }
  
  // Search screen input. Scroll moves the ribbon cursor while typing and
  // the highlight while browsing results.
  SearchModel &getSearch() { return search; }
  void searchScroll(int clicks, MusicLookup &musicLookup);
  void searchAppend(MusicLookup &musicLookup);
  void searchBackspace(MusicLookup &musicLookup);
  void searchSpace(MusicLookup &musicLookup);
  void searchEnterResults();
  void searchExitResults();

  // Route accessors
  Router& getRouteManager() { return router; }
  Route_t& getCurrentRoute() { return router.getCurrentRoute(); }
  bool canGoBack() { return router.canGoBack(); }

  // Breadcrumb title for the header
  const char *getHeaderTitle();
};

extern State state;