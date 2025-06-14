#pragma once
#include "../ui/theme.h"
#include "../ui/ui_types.h"
#include <Arduino.h>
#include "event-target.h"

// UI States
enum Showing { TRACK_LIST, NOW_PLAYING, ARTISTS, MENU, GENRE, PODCAST, TRANSITIONING };
// Event types for state changes
enum StateEvent {
  // Navigation events
  EVENT_TRACK_SELECTED = 1000,
  EVENT_SCROLL_CHANGED,
  EVENT_PAGE_CHANGED,
  
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
  EVENT_TRACK_DURATION_CHANGED
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
  const char** visibleTracks; // Current visible tracks after scroll
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

struct TrackListEvent {
  int totalTracks;
  const char** tracks;
  const char** visibleTracks;
};

// Application state management with event system
class State : public EventTarget {
private:
  // Track data
  const char **tracks;
  int totalTracks;
  const char* visibleTracks[TRACKS_PER_SCREEN];

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

  void updateVisibleTracks();

public:
  // Constructor with defaults
  State();

  // Track management
  void setTracks(const char **trackList, int count);
  
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
  
  // Getters
  const char *getCurrentTrackName() const;
  const char *getPlayingTrackName() const;
  bool needsScrollUpdate(int oldSelected, int oldTopVisible) const;
  void getVisibleTrackIndices(int &startIndex, int &endIndex) const;
  int getRelativeSelectedIndex() const;
  
  // Public accessors for UI components
  int getTotalTracks() const { return totalTracks; }
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
};

// Global state instance
extern State state;