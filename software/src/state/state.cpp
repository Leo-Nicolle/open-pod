#include "state.h"
#include "fast_scroll.h"
#include "app_toast.h"
#include "podcast_menu.h"

// Application state management (route navigation and search are in
// state_navigation.cpp)

State::State()
    : EventTarget(), elements(nullptr), totalElements(0), windowLookup(nullptr),
      windowStart(0), windowCount(0), playingTrackIndex(-1),
      playingTrackId(-1), isPlaying(false), playbackPosition(0),
      trackDuration(0), playingListParentType(Route_t::ROOT),
      playingListEntityId(0), playingListTotalTracks(0),
      podcastSource(nullptr), podcastState(nullptr), lastPodcastSave(0),
      volume(60),
      seekModeActive(false), fastScrollLetters(0), selectedIndex(0), topVisibleIndex(0),
      isAnimating(false), scrollAnimId(0),
      transitionAnimId(0), isRotatedMode(false) {
  playingTrackName[0] = '\0';
}

void State::startPlayback(int trackIndex, uint32_t trackId,
                          MusicLookup &musicLookup, int startAt) {
  if (trackIndex == -1) {
    trackIndex = selectedIndex;
  }
  // Bounds-check against the captured source-list size, not whatever list is
  // currently displayed - the user may have browsed elsewhere since this
  // list was loaded (see playingListTotalTracks).
  if (trackIndex >= 0 && trackIndex < playingListTotalTracks) {
    if (isPlayingPodcast() && (uint32_t)playingTrackId != trackId) {
      savePodcastPosition(true); // leaving an episode: remember where
    }
    playingTrackIndex = trackIndex;
    playingTrackId = (int)trackId;
    isPlaying = true;
    playbackPosition = startAt;
    lastPodcastSave = startAt;
    if (!musicLookup.getTrackName(trackId, playingTrackName,
                                  sizeof(playingTrackName))) {
      playingTrackName[0] = '\0';
    }
    PlaybackEvent event = {trackIndex, (int)trackId, getPlayingTrackName(),
                           true, startAt};
    emitEvent(EVENT_PLAYBACK_STARTED, &event);
  }
}

void State::startPodcast(uint32_t episode, bool fromStart,
                         MusicLookup &musicLookup) {
  if (!podcastSource || episode >= podcastSource->podcastEpisodeCount()) {
    return;
  }
  // The episode plays within its folder: next/previous and the series
  // auto-advance walk the folder's own episodes.
  uint32_t folder = podcastSource->podcastEpisodeFolder(episode);
  uint32_t first, end;
  podcastSource->podcastFolderSpan(folder, first, end);
  playingListParentType = Route_t::PODCAST_FOLDER;
  playingListEntityId = folder;
  playingListTotalTracks = (int)podcastSource->podcastOwnEpisodeCount(folder);
  startPlayback((int)(episode - first), podcastTrackId(episode), musicLookup,
                fromStart ? 0 : podcastStartAt(episode));
  goToNowPlaying(musicLookup);
}

int State::podcastStartAt(uint32_t episode) {
  if (!podcastState) {
    return 0;
  }
  PodcastState::Entry e = podcastState->get(episode);
  if (e.mark() != PodcastState::MARK_PROGRESS) {
    return 0;
  }
  char at[16];
  podfmt::duration(e.position, at, sizeof(at));
  appToast.show(BtModel::TOAST_NEUTRAL, "Resumed at ", at);
  return max(0, (int)e.position - RESUME_REWIND_SECONDS);
}

bool State::playPodcastAt(int index, MusicLookup &musicLookup) {
  if (!podcastSource || index < 0 || index >= playingListTotalTracks) {
    return false;
  }
  uint32_t episode =
      podcastSource->podcastOwnEpisodeAt(playingListEntityId, (uint32_t)index);
  if (episode == 0xFFFFFFFFu) {
    return false;
  }
  startPlayback(index, podcastTrackId(episode), musicLookup,
                podcastStartAt(episode));
  return true;
}

void State::savePodcastPosition(bool leaving) {
  if (!isPlayingPodcast() || !podcastState) {
    return;
  }
  podcastState->savePosition(podcastEpisodeOf((uint32_t)playingTrackId),
                             (uint32_t)max(0, playbackPosition),
                             (uint32_t)max(0, trackDuration), leaving);
  lastPodcastSave = playbackPosition;
}

void State::togglePlayback() {
  if (playingTrackIndex >= 0) {
    isPlaying = !isPlaying;
    if (!isPlaying) {
      savePodcastPosition(true);
    }
    PlaybackEvent event = {playingTrackIndex, playingTrackId,
                           getPlayingTrackName(), isPlaying};
    emitEvent(isPlaying ? EVENT_PLAYBACK_RESUMED : EVENT_PLAYBACK_PAUSED,
              &event);
  }
}

void State::stopPlayback() {
  if (playingTrackIndex >= 0) {
    savePodcastPosition(true);
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
    if (position < lastPodcastSave) {
      lastPodcastSave = position; // sought back: count from here
    }
    if (isPlayingPodcast() &&
        position - lastPodcastSave >= PODCAST_SAVE_SECONDS) {
      savePodcastPosition(false);
    }
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
  if (isPlayingPodcast()) {
    PlaybackEvent event = {playingTrackIndex, playingTrackId,
                           getPlayingTrackName(), false};
    emitEvent(EVENT_TRACK_ENDED, &event);
    podcastEnded(musicLookup);
    return;
  }
  if (playingTrackIndex >= 0) {
    PlaybackEvent event = {playingTrackIndex, playingTrackId,
                           getPlayingTrackName(), false};
    emitEvent(EVENT_TRACK_ENDED, &event);
    if (repeatTrack) {
      startPlayback(playingTrackIndex, playingTrackId, musicLookup);
    } else if (playingTrackIndex < playingListTotalTracks - 1) {
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

// The episode of the folder numbered right after the playing one, or -1
// (a dated show, or the next number isn't on the card).
int32_t State::nextSeriesEpisode() {
  if (!isPlayingPodcast() || !podcastSource) {
    return -1;
  }
  uint32_t episode = podcastEpisodeOf((uint32_t)playingTrackId);
  uint32_t number = podcastSource->podcastEpisodeNumber(episode) & 0xFFFF;
  uint32_t folder = playingListEntityId;
  if (!number || !podcastSource->podcastFolderIsSeries(folder)) {
    return -1;
  }
  uint32_t n = podcastSource->podcastOwnEpisodeCount(folder);
  for (uint32_t i = 0; i < n; i++) {
    uint32_t e = podcastSource->podcastOwnEpisodeAt(folder, i);
    if ((podcastSource->podcastEpisodeNumber(e) & 0xFFFF) == number + 1) {
      return (int32_t)e;
    }
  }
  return -1;
}

// End of an episode (README §5): it is played. A series goes on with the
// next number; a dated show stops and its list comes back with the cursor
// on the next unfinished episode.
void State::podcastEnded(MusicLookup &musicLookup) {
  uint32_t episode = podcastEpisodeOf((uint32_t)playingTrackId);
  uint32_t folder = playingListEntityId;
  if (podcastState) {
    podcastState->markFinished(episode);
  }
  int32_t next = nextSeriesEpisode();
  if (next >= 0) {
    uint32_t number = podcastSource->podcastEpisodeNumber(episode);
    uint32_t total = number >> 16;
    char done[12], after[32];
    snprintf(done, sizeof(done), "%lu/%lu", (unsigned long)(number & 0xFFFF),
             (unsigned long)total);
    snprintf(after, sizeof(after), " \xB7 playing %lu/%lu",
             (unsigned long)((number & 0xFFFF) + 1), (unsigned long)total);
    appToast.show(BtModel::TOAST_SUCCESS, "Finished ", done, after);
    uint32_t first, end;
    podcastSource->podcastFolderSpan(folder, first, end);
    lastPodcastSave = 0;
    // The finished episode's position is saved by markFinished(): don't
    // let startPlayback() save its (end) position over a later replay.
    playingTrackId = -1;
    int startAt = 0;
    if (podcastState) {
      PodcastState::Entry e = podcastState->get((uint32_t)next);
      if (e.mark() == PodcastState::MARK_PROGRESS)
        startAt = max(0, (int)e.position - RESUME_REWIND_SECONDS);
    }
    startPlayback((int)((uint32_t)next - first), podcastTrackId((uint32_t)next),
                  musicLookup, startAt);
    return;
  }
  appToast.show(BtModel::TOAST_SUCCESS, "Finished \xB7 marked as played");
  stopPlayback(); // saves the end position; the played flag stays
  // Back to the show's list, if Now Playing was opened from it.
  // The current route is routeStack[depth] (ROOT sits at 0).
  int depth = router.getDepth();
  if (router.getCurrentRoute().type == Route_t::NOW_PLAYING && depth >= 2) {
    Route_t &below = router.routeStack[depth - 1];
    bool fromFolder =
        (below.type == Route_t::PODCAST_FOLDER && below.entityId == folder) ||
        (below.type == Route_t::PODCASTS && folder == 0);
    if (fromFolder && podcastSource && podcastState) {
      below.selectedIndex =
          PodcastMenu::initialRow(*podcastSource, *podcastState, folder);
      back(musicLookup);
    }
  }
}

bool State::getNextTrackId(MusicLookup &musicLookup, uint32_t &nextTrackId) {
  if (isPlayingPodcast()) {
    int32_t next = nextSeriesEpisode();
    if (next < 0)
      return false;
    nextTrackId = podcastTrackId((uint32_t)next);
    return true;
  }
  if (repeatTrack && playingTrackIndex >= 0) {
    // Prefetch the start of the same track, so the loop is seamless too.
    nextTrackId = (uint32_t)playingTrackId;
    return true;
  }
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

// Next/previous on a podcast walk the folder's list, as on an album; the end
// of an episode only chains within a series (getNextTrackId).
bool State::nextTrack(MusicLookup &musicLookup) {
  if (isPlayingPodcast()) {
    return playPodcastAt(playingTrackIndex + 1, musicLookup);
  }
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
    if (isPlayingPodcast()) {
      if (playPodcastAt(playingTrackIndex - 1, musicLookup)) {
        return true;
      }
    } else {
      uint32_t prevTrackId =
          resolveTrackId(musicLookup, playingTrackIndex - 1);
      if (prevTrackId != UINT32_MAX) {
        startPlayback(playingTrackIndex - 1, prevTrackId, musicLookup);
        return true;
      }
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

void State::jumpTo(int index) {
  if (totalElements <= 0) {
    return;
  }
  int oldSelected = selectedIndex;
  int oldTopVisible = topVisibleIndex;
  int offset = selectedIndex - topVisibleIndex;
  selectedIndex = constrain(index, 0, totalElements - 1);
  topVisibleIndex = constrain(selectedIndex - offset, 0,
                              max(0, totalElements - ELEMENTS_PER_SCREEN));
  if (needsScrollUpdate(oldSelected, oldTopVisible)) {
    updateVisibleElements();
    ScrollChangedEvent event = {selectedIndex, topVisibleIndex, oldSelected,
                                oldTopVisible, visibleElements};
    emitEvent(EVENT_PAGE_CHANGED, &event);
  }
}

void State::letterJump(int clicks, int letters) {
  int dir = clicks < 0 ? 1 : -1; // negative clicks scroll down
  int list = globalList();
  int pos = selectedIndex;
  for (int i = 0; i < abs(clicks); i++) {
    int next = -1;
    if (list >= 0 && windowLookup) {
      next = windowLookup->letterBucket(list, letters, pos, dir);
    }
    if (next < 0) {
      next = fastScrollJump(pos, dir, letters, totalElements, keyAt, this);
    }
    if (next == pos) {
      break;
    }
    pos = next;
  }
  jumpTo(pos);
}

bool State::keyAt(void *self, int index, int letters, char *out) {
  State *s = (State *)self;
  const char *name = s->elementAt(index);
  if (!name && !s->elements && s->windowLookup) {
    // Outside the window: read just this name, the window stays.
    char buf[256];
    const char *ptr = nullptr;
    if (s->loadNames(index, buf, sizeof(buf), &ptr, 1) == 1) {
      fastScrollKey(ptr, letters, out);
      return true;
    }
  }
  if (!name) {
    return false;
  }
  fastScrollKey(name, letters, out);
  return true;
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

void State::setFastScroll(int letters) {
  uint8_t l = (uint8_t)constrain(letters, 0, 3);
  if (fastScrollLetters != l) {
    fastScrollLetters = l;
    FastScrollEvent event = {l > 0, l};
    emitEvent(EVENT_FAST_SCROLL_CHANGED, &event);
  }
}

// Global state instance
State state;
