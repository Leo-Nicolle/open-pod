#pragma once
#include "../ui/theme.h"
#include "../ui/ui_types.h"
#include <Arduino.h>
#include "event-target.h"
#include "../storage/Music_lookup.h"
#include "router.h"

// Buffer sizes for string data
#define ELEMENTS_BUFFER_SIZE 8192
#define MAX_ELEMENTS_PER_SCREEN 20
#define MAX_STRING_POINTERS 200

// UI States
enum Showing { TRACK_LIST, NOW_PLAYING, ARTISTS, MENU, GENRE, PODCAST, TRANSITIONING };

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
  EVENT_UI_STATE_CHANGED,
  EVENT_ANIMATION_STARTED,
  EVENT_ANIMATION_FINISHED,
  
  // Data events
  EVENT_TRACK_LIST_UPDATED,
  EVENT_TRACK_DURATION_CHANGED,
  EVENT_DATA_LOADED
};

// Event data structures
struct TrackSelectedEvent {
  int trackIndex;
  const char* trackName;
};

struct ScrollChangedEvent {
  int selectedIndex;
  int topVisibleIndex;
  int oldSelectedIndex;
  int oldTopVisibleIndex;
  const char** visibleElements; // Current visible elements after scroll
};

struct PlaybackEvent {
  int trackIndex;
  const char* trackName;
  bool isPlaying;
};

struct ProgressEvent {
  int position;        // in seconds
  int duration;        // in seconds
  float percentage;    // 0.0 to 1.0
};

struct ShowingEvent {
  Showing oldState;
  Showing newState;
};

struct AnimationEvent {
  uint32_t animationId;
  int animationType;
};

struct ListEvent {
  int totalElements;
  const char** elements;
  const char** visibleElements;
};

struct RouteChangedEvent {
  Route_t::RouteType oldRouteType;
  Route_t::RouteType newRouteType;
  uint32_t entityId;
  const char* entityName;
  bool canGoBack;
};

// Application state management with event system
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
  bool isPlaying;
  int playbackPosition; // in seconds
  int trackDuration;    // in seconds

  // Navigation state
  int selectedTrackIndex;
  int topVisibleTrackIndex;

  // UI state
  Showing currentShowing;
  Showing targetShowing;

  // Animation state
  bool isAnimating;
  uint32_t scrollAnimId;
  uint32_t transitionAnimId;

  // Rotation state
  bool isRotatedMode;

  // Route_t management
  Router router;

  void updateVisibleTracks();

public:
  // Constructor with defaults
  State();

  // Track management
  void setElements(const char **elementList, int count);
  
  // Navigation
  void selectTrack(int index);
  void scrollUp();
  void scrollDown();
  void pageUp();
  void pageDown();
  
  // Playback control
  void startPlayback(int trackIndex = -1);
  void togglePlayback();
  void stopPlayback();
  void updateProgress(int position);
  void setTrackDuration(int duration);
  void notifyTrackEnded();
  
  // UI state management
  void setShowing(Showing newState);
  void setAnimating(bool animating, uint32_t animId = 0);
  void setRotation(bool rotated);
  
  // Route_t management
  void navigateToRoute(Route_t::RouteType type, uint32_t entityId = 0, const char* entityName = nullptr);
  bool navigateBack();
  void loadCurrentRouteData(MusicLookup& musicLookup);
  
  // Data loading methods
  void loadArtists(MusicLookup& musicLookup, uint32_t offset = 0);
  void loadAlbums(MusicLookup& musicLookup, uint32_t offset = 0);
  void loadGenres(MusicLookup& musicLookup, uint32_t offset = 0);
  void loadTracksByArtist(MusicLookup& musicLookup, uint32_t artistId);
  void loadTracksByAlbum(MusicLookup& musicLookup, uint32_t albumId);
  void loadTracksByGenre(MusicLookup& musicLookup, uint32_t genreId);
  void loadAlbumsByArtist(MusicLookup& musicLookup, uint32_t artistId);
  void loadAlbumsByGenre(MusicLookup& musicLookup, uint32_t genreId);
  
  // Getters
  const char *getCurrentTrackName() const;
  const char *getPlayingTrackName() const;
  bool needsScrollUpdate(int oldSelected, int oldTopVisible) const;
  void getVisibleTrackIndices(int &startIndex, int &endIndex) const;
  int getRelativeSelectedIndex() const;
  
  // Public accessors for UI components
  int getTotalTracks() const { return totalElements; }
  int getSelectedTrackIndex() const { return selectedTrackIndex; }
  int getTopVisibleTrackIndex() const { return topVisibleTrackIndex; }
  int getPlayingTrackIndex() const { return playingTrackIndex; }
  bool getIsPlaying() const { return isPlaying; }
  int getPlaybackPosition() const { return playbackPosition; }
  int getTrackDuration() const { return trackDuration; }
  Showing getCurrentShowing() const { return currentShowing; }
  Showing getTargetShowing() const { return targetShowing; }
  bool getIsAnimating() const { return isAnimating; }
  bool getIsRotatedMode() const { return isRotatedMode; }
  uint32_t getScrollAnimId() const { return scrollAnimId; }
  uint32_t getTransitionAnimId() const { return transitionAnimId; }
  
  // Route_t accessors
  Router& getRouteManager() { return router; }
  Route_t& getCurrentRoute() { return router.getCurrentRoute(); }
  bool canGoBack() { return router.canGoBack(); }
};

// Global state instance
extern State state;