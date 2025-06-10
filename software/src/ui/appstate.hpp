#pragma once
#include "ui_types.h"
#include "theme.h"
#include <Arduino.h>
// Application state management
struct AppState {
  // Track data
  const char **tracks;
  int totalTracks;
  
  // Current playback state
  int playingTrackIndex;
  bool isPlaying;
  int playbackPosition; // in seconds
  int trackDuration;    // in seconds
  
  // Navigation state
  int selectedTrackIndex;
  int topVisibleTrackIndex;
  
  // UI state
  UIState currentUIState;
  UIState targetUIState;
  bool isRotatedMode;
  
  // Animation state
  bool isAnimating;
  uint32_t scrollAnimId;
  uint32_t transitionAnimId;
  
  // Constructor with defaults
  AppState() : 
    tracks(nullptr),
    totalTracks(0),
    playingTrackIndex(-1),
    isPlaying(false),
    playbackPosition(0),
    trackDuration(0),
    selectedTrackIndex(0),
    topVisibleTrackIndex(0),
    currentUIState(STATE_TRACK_LIST),
    targetUIState(STATE_TRACK_LIST),
    isRotatedMode(false),
    isAnimating(false),
    scrollAnimId(0),
    transitionAnimId(0) {}
  
  // Helper methods
  void setTracks(const char **trackList, int count) {
    tracks = trackList;
    totalTracks = count;
    
    // Ensure indices are within bounds
    selectedTrackIndex = constrain(selectedTrackIndex, 0, totalTracks - 1);
    topVisibleTrackIndex = constrain(topVisibleTrackIndex, 0, max(0, totalTracks - TRACKS_PER_SCREEN));
    if (playingTrackIndex >= totalTracks) {
      playingTrackIndex = -1;
    }
  }
  
  void selectTrack(int index) {
    if (index < 0 || index >= totalTracks) return;
    
    selectedTrackIndex = index;
    
    // Auto-scroll to keep selection visible
    if (selectedTrackIndex < topVisibleTrackIndex) {
      topVisibleTrackIndex = selectedTrackIndex;
    } else if (selectedTrackIndex >= topVisibleTrackIndex + TRACKS_PER_SCREEN) {
      topVisibleTrackIndex = selectedTrackIndex - TRACKS_PER_SCREEN + 1;
    }
    
    // Constrain top visible track
    topVisibleTrackIndex = constrain(topVisibleTrackIndex, 0, max(0, totalTracks - TRACKS_PER_SCREEN));
  }
  
  void scrollUp() {
    if (selectedTrackIndex > 0) {
      selectTrack(selectedTrackIndex - 1);
    }
  }
  
  void scrollDown() {
    if (selectedTrackIndex < totalTracks - 1) {
      selectTrack(selectedTrackIndex + 1);
    }
  }
  
  void pageUp() {
    selectTrack(max(0, selectedTrackIndex - TRACKS_PER_SCREEN));
  }
  
  void pageDown() {
    selectTrack(min(totalTracks - 1, selectedTrackIndex + TRACKS_PER_SCREEN));
  }
  
  void startPlayback(int trackIndex = -1) {
    if (trackIndex == -1) {
      trackIndex = selectedTrackIndex;
    }
    
    if (trackIndex >= 0 && trackIndex < totalTracks) {
      playingTrackIndex = trackIndex;
      isPlaying = true;
      playbackPosition = 0;
      // trackDuration would be set by the audio system
    }
  }
  
  void togglePlayback() {
    if (playingTrackIndex >= 0) {
      isPlaying = !isPlaying;
    }
  }
  
  void stopPlayback() {
    isPlaying = false;
    playbackPosition = 0;
  }
  
  const char* getCurrentTrackName() const {
    if (tracks && selectedTrackIndex >= 0 && selectedTrackIndex < totalTracks) {
      return tracks[selectedTrackIndex];
    }
    return nullptr;
  }
  
  const char* getPlayingTrackName() const {
    if (tracks && playingTrackIndex >= 0 && playingTrackIndex < totalTracks) {
      return tracks[playingTrackIndex];
    }
    return nullptr;
  }
  
  bool needsScrollUpdate(int oldSelected, int oldTopVisible) const {
    return (selectedTrackIndex != oldSelected || topVisibleTrackIndex != oldTopVisible);
  }
  
  // Get visible track indices for the track list
  void getVisibleTrackIndices(int &startIndex, int &endIndex) const {
    startIndex = topVisibleTrackIndex;
    endIndex = min(totalTracks - 1, topVisibleTrackIndex + TRACKS_PER_SCREEN - 1);
  }
  
  int getRelativeSelectedIndex() const {
    return selectedTrackIndex - topVisibleTrackIndex;
  }
};