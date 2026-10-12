#include "state.h"

// Application state management (route navigation and search are in
// state_navigation.cpp)

State::State()
    : EventTarget(), elements(nullptr), totalElements(0), windowLookup(nullptr),
      windowStart(0), windowCount(0), playingTrackIndex(-1),
      playingTrackId(-1), isPlaying(false), playbackPosition(0),
      trackDuration(0), playingListParentType(Route_t::ROOT),
      playingListEntityId(0), playingListTotalTracks(0), volume(60),
      seekModeActive(false), fastScrollActive(false), selectedIndex(0), topVisibleIndex(0),
      isAnimating(false), scrollAnimId(0),
      transitionAnimId(0), isRotatedMode(false) {
  playingTrackName[0] = '\0';
}

void State::startPlayback(int trackIndex, uint32_t trackId,
                          MusicLookup &musicLookup) {
  if (trackIndex == -1) {
    trackIndex = selectedIndex;
  }
  // Bounds-check against the captured source-list size, not whatever list is
  // currently displayed - the user may have browsed elsewhere since this
  // list was loaded (see playingListTotalTracks).
  if (trackIndex >= 0 && trackIndex < playingListTotalTracks) {
    playingTrackIndex = trackIndex;
    playingTrackId = (int)trackId;
    isPlaying = true;
    playbackPosition = 0;
    if (!musicLookup.getTrackName(trackId, playingTrackName,
                                  sizeof(playingTrackName))) {
      playingTrackName[0] = '\0';
    }
    PlaybackEvent event = {trackIndex, (int)trackId, getPlayingTrackName(),
                           true};
    emitEvent(EVENT_PLAYBACK_STARTED, &event);
  }
}

void State::togglePlayback() {
  if (playingTrackIndex >= 0) {
    isPlaying = !isPlaying;
    PlaybackEvent event = {playingTrackIndex, playingTrackId,
                           getPlayingTrackName(), isPlaying};
    emitEvent(isPlaying ? EVENT_PLAYBACK_RESUMED : EVENT_PLAYBACK_PAUSED,
              &event);
  }
}

void State::stopPlayback() {
  if (playingTrackIndex >= 0) {
    int stoppedTrackIndex = playingTrackIndex;
    int stoppedTrackId = playingTrackId;
    const char *stoppedTrackName = getPlayingTrackName();
    isPlaying = false;
    playbackPosition = 0;
    playingTrackIndex = -1;
    playingTrackId = -1;
    PlaybackEvent event = {stoppedTrackIndex, stoppedTrackId, stoppedTrackName,
                           false};
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

void State::notifyTrackEnded(MusicLookup &musicLookup) {
  if (playingTrackIndex >= 0) {
    PlaybackEvent event = {playingTrackIndex, playingTrackId,
                           getPlayingTrackName(), false};
    emitEvent(EVENT_TRACK_ENDED, &event);
    if (playingTrackIndex < playingListTotalTracks - 1) {
      int nextIndex = playingTrackIndex + 1;
      uint32_t nextTrackId = resolveTrackId(musicLookup, nextIndex);
      if (nextTrackId != UINT32_MAX) {
        startPlayback(nextIndex, nextTrackId, musicLookup);
      } else {
        stopPlayback();
      }
    } else {
      stopPlayback();
    }
  }
}

bool State::getNextTrackId(MusicLookup &musicLookup, uint32_t &nextTrackId) {
  if (playingTrackIndex >= 0 &&
      playingTrackIndex < playingListTotalTracks - 1) {
    uint32_t id = resolveTrackId(musicLookup, playingTrackIndex + 1);
    if (id != UINT32_MAX) {
      nextTrackId = id;
      return true;
    }
  }
  return false;
}

bool State::nextTrack(MusicLookup &musicLookup) {
  uint32_t nextTrackId;
  if (!getNextTrackId(musicLookup, nextTrackId)) {
    return false;
  }
  startPlayback(playingTrackIndex + 1, nextTrackId, musicLookup);
  return true;
}

bool State::previousTrack(MusicLookup &musicLookup) {
  if (playingTrackIndex < 0) {
    return false;
  }
  if (playbackPosition <= PREVIOUS_RESTART_SECONDS && playingTrackIndex > 0) {
    uint32_t prevTrackId = resolveTrackId(musicLookup, playingTrackIndex - 1);
    if (prevTrackId != UINT32_MAX) {
      startPlayback(playingTrackIndex - 1, prevTrackId, musicLookup);
      return true;
    }
  }
  // Restart the current track
  startPlayback(playingTrackIndex, (uint32_t)playingTrackId, musicLookup);
  return true;
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
  if (selectedIndex >= 0 && selectedIndex < totalElements) {
    return elementAt(selectedIndex);
  }
  return nullptr;
}

const char *State::elementAt(int i) const {
  if (i < 0 || i >= totalElements) {
    return nullptr;
  }
  if (elements) {
    return elements[i];
  }
  if (i >= windowStart && i < windowStart + windowCount) {
    return stringPointers[i - windowStart];
  }
  return nullptr;
}

void State::ensureWindow(int first, int last) {
  if (elements || !windowLookup || totalElements == 0) {
    return;
  }
  last = min(last, totalElements - 1);
  if (first >= windowStart && last < windowStart + windowCount) {
    return;
  }
  // Centre the visible rows in the window, so scrolling either way has
  // room before the next reload.
  int span = last - first + 1;
  int start = first - (MAX_STRING_POINTERS - span) / 2;
  start = constrain(start, 0, max(0, totalElements - MAX_STRING_POINTERS));
  windowStart = start;
  windowCount = loadWindow(start);
}

void State::setWindowedElements(MusicLookup &musicLookup, int count) {
  elements = nullptr;
  windowLookup = &musicLookup;
  windowStart = 0;
  windowCount = 0;
  setElements(nullptr, count);
}

const char *State::getPlayingTrackName() const {
  if (playingTrackIndex >= 0 && playingTrackName[0] != '\0') {
    return playingTrackName;
  }
  return nullptr;
}

void State::scrollUp() {
  int oldSelected = selectedIndex;
  int oldTopVisible = topVisibleIndex;
  if (selectedIndex > 0) {
    selectedIndex--;
    if (selectedIndex < topVisibleIndex) {
      topVisibleIndex = selectedIndex;
    }
    if (needsScrollUpdate(oldSelected, oldTopVisible)) {
      updateVisibleElements();
      ScrollChangedEvent event = {selectedIndex, topVisibleIndex, oldSelected,
                                  oldTopVisible, visibleElements};
      emitEvent(EVENT_SCROLL_CHANGED, &event);
    }
  }
}

void State::scrollDown() {
  int oldSelected = selectedIndex;
  int oldTopVisible = topVisibleIndex;
  if (selectedIndex < totalElements - 1) {
    selectedIndex++;
    if (selectedIndex >= topVisibleIndex + ELEMENTS_PER_SCREEN) {
      topVisibleIndex = selectedIndex - ELEMENTS_PER_SCREEN + 1;
    }
    if (needsScrollUpdate(oldSelected, oldTopVisible)) {
      updateVisibleElements();
      ScrollChangedEvent event = {selectedIndex, topVisibleIndex, oldSelected,
                                  oldTopVisible, visibleElements};

      emitEvent(EVENT_SCROLL_CHANGED, &event);
    }
  }
}

void State::pageUp() {
  int oldSelected = selectedIndex;
  int oldTopVisible = topVisibleIndex;
  selectedIndex = max(0, selectedIndex - ELEMENTS_PER_SCREEN);
  topVisibleIndex = max(0, topVisibleIndex - ELEMENTS_PER_SCREEN);
  if (needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateVisibleElements();
    ScrollChangedEvent event = {selectedIndex, topVisibleIndex, oldSelected,
                                oldTopVisible, visibleElements};
    emitEvent(EVENT_PAGE_CHANGED, &event);
  }
}

void State::pageDown() {
  int oldSelected = selectedIndex;
  int oldTopVisible = topVisibleIndex;
  selectedIndex = min(totalElements - 1, selectedIndex + ELEMENTS_PER_SCREEN);
  topVisibleIndex = min(max(0, totalElements - ELEMENTS_PER_SCREEN),
                        topVisibleIndex + ELEMENTS_PER_SCREEN);
  if (needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateVisibleElements();
    ScrollChangedEvent event = {selectedIndex, topVisibleIndex, oldSelected,
                                oldTopVisible, visibleElements};
    emitEvent(EVENT_PAGE_CHANGED, &event);
  }
}

void State::updateVisibleElements() {
  ensureWindow(topVisibleIndex, topVisibleIndex + ELEMENTS_PER_SCREEN - 1);
  for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
    const char *e = elementAt(i + topVisibleIndex);
    visibleElements[i] = (i + topVisibleIndex < totalElements) ? (e ? e : "") : nullptr;
  }
}

void State::setElements(const char **elementList, int count) {
  if (elementList) {
    elements = elementList;
    windowLookup = nullptr;
  }
  totalElements = count;

  if (selectedIndex >= count) {
    selectedIndex = count > 0 ? count - 1 : 0;
  }
  topVisibleIndex =
      constrain(topVisibleIndex, 0, max(0, count - ELEMENTS_PER_SCREEN));
  updateVisibleElements();
  ListEvent event = {count, selectedIndex, topVisibleIndex, visibleElements};
  emitEvent(EVENT_TRACK_LIST_UPDATED, &event);
}

bool State::needsScrollUpdate(int oldSelected, int oldTopVisible) const {
  return (selectedIndex != oldSelected) || (topVisibleIndex != oldTopVisible);
}

void State::getVisibleTrackIndices(int &startIndex, int &endIndex) const {
  startIndex = topVisibleIndex;
  endIndex = min(topVisibleIndex + ELEMENTS_PER_SCREEN - 1, totalElements - 1);
}

int State::getRelativeSelectedIndex() const {
  return selectedIndex - topVisibleIndex;
}

void State::setVolume(int vol) {
  vol = constrain(vol, 0, 100);
  if (vol != volume) {
    volume = vol;
    VolumeEvent event = {volume};
    emitEvent(EVENT_VOLUME_CHANGED, &event);
  }
}

void State::increaseVolume(int step) { setVolume(volume + step); }

void State::decreaseVolume(int step) { setVolume(volume - step); }

void State::enterSeekMode() {
  if (!seekModeActive) {
    seekModeActive = true;
    SeekModeEvent event = {true};
    emitEvent(EVENT_SEEK_MODE_CHANGED, &event);
  }
}

void State::exitSeekMode() {
  if (seekModeActive) {
    seekModeActive = false;
    SeekModeEvent event = {false};
    emitEvent(EVENT_SEEK_MODE_CHANGED, &event);
  }
}

void State::setFastScroll(bool active) {
  if (fastScrollActive != active) {
    fastScrollActive = active;
    FastScrollEvent event = {active};
    emitEvent(EVENT_FAST_SCROLL_CHANGED, &event);
  }
}

// Global state instance
State state;
