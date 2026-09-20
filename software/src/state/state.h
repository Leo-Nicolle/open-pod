#pragma once
#include "../storage/Music_lookup.h"
#include "../ui/theme.h"
#include "../ui/ui_types.h"
#include "event-target.h"
#include "router.h"
#include <Arduino.h>

// Buffer sizes for string data
#define ELEMENTS_BUFFER_SIZE 8192
#define MAX_STRING_POINTERS 200

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
  EVENT_SEEK_MODE_CHANGED
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
  const char **elements;
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

class State : public EventTarget {
private:
  // String buffer and pointers for current data
  char elementsBuffer[ELEMENTS_BUFFER_SIZE];
  const char* stringPointers[MAX_STRING_POINTERS];
  
  // Track data (uses stringPointers as backing)
  const char **elements;
  int totalElements;
  const char* visibleElements[ELEMENTS_PER_SCREEN];

  // Current playback state
  int playingTrackIndex;
  int playingTrackId;
  bool isPlaying;
  int playbackPosition; // in seconds
  int trackDuration;    // in seconds
  char playingTrackName[64]; // owned copy - elements[] may point at a
                              // different list by the time this is read

  // Source-list context captured at playback-start time (not re-derived
  // later from the live router stack or the currently-displayed list -
  // both can point elsewhere once the user browses away from Now Playing
  // while a track keeps playing).
  Route_t::RouteType playingListParentType;
  uint32_t playingListEntityId;
  int playingListTotalTracks;

  int volume;          // 0-100 logical volume
  bool seekModeActive; // Now Playing seek-mode (center press to enter)

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

  void updateVisibleElements();
  void restoreSelectionState();
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
  
  // Scroll navigation (still needed for UI)
  void scrollUp();
  void scrollDown();
  void pageUp();
  void pageDown();
  
  // Playback control
  void startPlayback(int trackIndex, uint32_t trackId, MusicLookup &musicLookup);
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

  // Volume control (Now Playing wheel scroll)
  void setVolume(int vol);
  void increaseVolume(int step = 5);
  void decreaseVolume(int step = 5);
  int getVolume() const { return volume; }

  // Seek mode (Now Playing center press)
  void enterSeekMode();
  void exitSeekMode();
  bool getSeekModeActive() const { return seekModeActive; }

  // UI state management
  void setAnimating(bool animating, uint32_t animId = 0);
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
  
  // Route accessors
  Router& getRouteManager() { return router; }
  Route_t& getCurrentRoute() { return router.getCurrentRoute(); }
  bool canGoBack() { return router.canGoBack(); }

  // Breadcrumb title for the header
  const char *getHeaderTitle();
};

extern State state;