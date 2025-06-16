#pragma once
#include "state.h"

// Application state management

State::State()
    : EventTarget(), elements(nullptr), totalElements(0), playingTrackIndex(-1),
      isPlaying(false), playbackPosition(0), trackDuration(0),
      selectedTrackIndex(0), topVisibleTrackIndex(0),
      currentShowing(TRACK_LIST), targetShowing(TRACK_LIST), isAnimating(false),
      scrollAnimId(0), transitionAnimId(0), isRotatedMode(false) {}

void State::updateVisibleTracks() {
  for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
    if (i + topVisibleTrackIndex < totalElements) {
      visibleElements[i] = elements[i + topVisibleTrackIndex];
    } else {
      visibleElements[i] = nullptr;
    }
  }
}

void State::setElements(const char **elementList, int count) {
  elements = elementList;
  totalElements = count;

  if (selectedTrackIndex >= count) {
    selectedTrackIndex = count > 0 ? count - 1 : 0;
  }
  for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
    if (i < count) {
      visibleElements[i] = elementList[i];
    } else {
      visibleElements[i] = nullptr;
    }
  }
  ListEvent event = {count, elementList, visibleElements};
  emitEvent(EVENT_TRACK_LIST_UPDATED, &event);
}

void State::selectTrack(int index) {
  if (index >= 0 && index < totalElements && index != selectedTrackIndex) {
    int oldIndex = selectedTrackIndex;
    selectedTrackIndex = index;
    if (selectedTrackIndex < topVisibleTrackIndex) {
      topVisibleTrackIndex = selectedTrackIndex;
    } else if (selectedTrackIndex >= topVisibleTrackIndex + ELEMENTS_PER_SCREEN) {
      topVisibleTrackIndex = selectedTrackIndex - ELEMENTS_PER_SCREEN + 1;
    }
    TrackSelectedEvent event = {index, getCurrentTrackName()};
    emitEvent(EVENT_TRACK_SELECTED, &event);
  }
}

void State::scrollUp() {
  int oldSelected = selectedTrackIndex;
  int oldTopVisible = topVisibleTrackIndex;
  if (selectedTrackIndex > 0) {
    selectedTrackIndex--;
    if (selectedTrackIndex < topVisibleTrackIndex) {
      topVisibleTrackIndex = selectedTrackIndex;
    }
    if (needsScrollUpdate(oldSelected, oldTopVisible)) {
      updateVisibleTracks();
      ScrollChangedEvent event = {selectedTrackIndex, topVisibleTrackIndex,
                                  oldSelected, oldTopVisible, visibleElements};
      emitEvent(EVENT_SCROLL_CHANGED, &event);
    }
  }
}

void State::scrollDown() {
  int oldSelected = selectedTrackIndex;
  int oldTopVisible = topVisibleTrackIndex;
  if (selectedTrackIndex < totalElements - 1) {
    selectedTrackIndex++;
    if (selectedTrackIndex >= topVisibleTrackIndex + ELEMENTS_PER_SCREEN) {
      topVisibleTrackIndex = selectedTrackIndex - ELEMENTS_PER_SCREEN + 1;
    }
    if (needsScrollUpdate(oldSelected, oldTopVisible)) {
      updateVisibleTracks();
      ScrollChangedEvent event = {selectedTrackIndex, topVisibleTrackIndex,
                                  oldSelected, oldTopVisible, visibleElements};
      emitEvent(EVENT_SCROLL_CHANGED, &event);
    }
  }
}

void State::pageUp() {
  int oldSelected = selectedTrackIndex;
  int oldTopVisible = topVisibleTrackIndex;
  selectedTrackIndex = max(0, selectedTrackIndex - ELEMENTS_PER_SCREEN);
  topVisibleTrackIndex = max(0, topVisibleTrackIndex - ELEMENTS_PER_SCREEN);
  if (needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateVisibleTracks();
    ScrollChangedEvent event = {selectedTrackIndex, topVisibleTrackIndex,
                                oldSelected, oldTopVisible, visibleElements};
    emitEvent(EVENT_PAGE_CHANGED, &event);
  }
}

void State::pageDown() {
  int oldSelected = selectedTrackIndex;
  int oldTopVisible = topVisibleTrackIndex;
  selectedTrackIndex =
      min(totalElements - 1, selectedTrackIndex + ELEMENTS_PER_SCREEN);
  topVisibleTrackIndex = min(max(0, totalElements - ELEMENTS_PER_SCREEN),
                             topVisibleTrackIndex + ELEMENTS_PER_SCREEN);
  if (needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateVisibleTracks();
    ScrollChangedEvent event = {selectedTrackIndex, topVisibleTrackIndex,
                                oldSelected, oldTopVisible, visibleElements};
    emitEvent(EVENT_PAGE_CHANGED, &event);
  }
}

void State::startPlayback(int trackIndex) {
  if (trackIndex == -1) {
    trackIndex = selectedTrackIndex;
  }
  if (trackIndex >= 0 && trackIndex < totalElements) {
    playingTrackIndex = trackIndex;
    isPlaying = true;
    playbackPosition = 0;
    PlaybackEvent event = {trackIndex, getPlayingTrackName(), true};
    emitEvent(EVENT_PLAYBACK_STARTED, &event);
  }
}

void State::togglePlayback() {
  if (playingTrackIndex >= 0) {
    isPlaying = !isPlaying;
    PlaybackEvent event = {playingTrackIndex, getPlayingTrackName(), isPlaying};
    emitEvent(isPlaying ? EVENT_PLAYBACK_RESUMED : EVENT_PLAYBACK_PAUSED,
              &event);
  }
}

void State::stopPlayback() {
  if (playingTrackIndex >= 0) {
    int stoppedTrackIndex = playingTrackIndex;
    const char *stoppedTrackName = getPlayingTrackName();
    isPlaying = false;
    playbackPosition = 0;
    playingTrackIndex = -1;
    PlaybackEvent event = {stoppedTrackIndex, stoppedTrackName, false};
    emitEvent(EVENT_PLAYBACK_STOPPED, &event);
  }
}

void State::updateProgress(int position) {
  if (position != playbackPosition && trackDuration > 0) {
    playbackPosition = position;
    ProgressEvent event = {position, trackDuration,
                           trackDuration > 0 ? (float)position / trackDuration
                                             : 0.0f};
    emitEvent(EVENT_PROGRESS_UPDATED, &event);
  }
}

void State::setTrackDuration(int duration) {
  if (duration != trackDuration) {
    trackDuration = duration;
    emitEvent(EVENT_TRACK_DURATION_CHANGED, &duration);
  }
}

void State::notifyTrackEnded() {
  if (playingTrackIndex >= 0) {
    PlaybackEvent event = {playingTrackIndex, getPlayingTrackName(), false};
    emitEvent(EVENT_TRACK_ENDED, &event);
    if (playingTrackIndex < totalElements - 1) {
      startPlayback(playingTrackIndex + 1);
    } else {
      stopPlayback();
    }
  }
}

void State::setShowing(Showing newState) {
  if (newState != currentShowing) {
    Showing oldState = currentShowing;
    currentShowing = newState;
    ShowingEvent event = {oldState, newState};
    emitEvent(EVENT_UI_STATE_CHANGED, &event);
  }
}

void State::setAnimating(bool animating, uint32_t animId) {
  if (animating != isAnimating) {
    isAnimating = animating;
    AnimationEvent event = {animId, animating ? 1 : 0};
    emitEvent(animating ? EVENT_ANIMATION_STARTED : EVENT_ANIMATION_FINISHED,
              &event);
  }
}

void State::setRotation(bool rotated) { isRotatedMode = rotated; }

const char *State::getCurrentTrackName() const {
  if (selectedTrackIndex >= 0 && selectedTrackIndex < totalElements) {
    return elements[selectedTrackIndex];
  }
  return nullptr;
}

const char *State::getPlayingTrackName() const {
  if (playingTrackIndex >= 0 && playingTrackIndex < totalElements) {
    return elements[playingTrackIndex];
  }
  return nullptr;
}

bool State::needsScrollUpdate(int oldSelected, int oldTopVisible) const {
  return (selectedTrackIndex != oldSelected) ||
         (topVisibleTrackIndex != oldTopVisible);
}

void State::getVisibleTrackIndices(int &startIndex, int &endIndex) const {
  startIndex = topVisibleTrackIndex;
  endIndex = min(topVisibleTrackIndex + ELEMENTS_PER_SCREEN - 1, totalElements - 1);
}

int State::getRelativeSelectedIndex() const {
  return selectedTrackIndex - topVisibleTrackIndex;
}

// Global state instance
State state;