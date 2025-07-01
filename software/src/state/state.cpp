#pragma once
#include "state.h"
#include "static_menu.h"

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
    } else if (selectedTrackIndex >=
               topVisibleTrackIndex + ELEMENTS_PER_SCREEN) {
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
  endIndex =
      min(topVisibleTrackIndex + ELEMENTS_PER_SCREEN - 1, totalElements - 1);
}

int State::getRelativeSelectedIndex() const {
  return selectedTrackIndex - topVisibleTrackIndex;
}

// Route_t management implementation
void State::navigateToRoute(Route_t::RouteType type, uint32_t entityId,
                            const char *entityName) {
  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;

  router.pushRoute(type, entityId, entityName);

  // Emit route changed event
  RouteChangedEvent event = {oldRouteType, type, entityId, entityName,
                             router.canGoBack()};
  emitEvent(EVENT_ROUTE_CHANGED, &event);

  // Reset selection when navigating to new route
  selectedTrackIndex = 0;
  topVisibleTrackIndex = 0;
}

bool State::navigateBack() {
  if (!router.canGoBack()) {
    return false;
  }

  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;
  router.popRoute();
  Route_t &currentRoute = router.getCurrentRoute();

  // Emit route changed event
  RouteChangedEvent event = {oldRouteType, currentRoute.type,
                             currentRoute.entityId, currentRoute.entityName,
                             router.canGoBack()};
  emitEvent(EVENT_ROUTE_CHANGED, &event);

  // Reset selection when going back
  selectedTrackIndex = 0;
  topVisibleTrackIndex = 0;

  return true;
}

void State::loadCurrentRouteData(MusicLookup &musicLookup) {
  Route_t &currentRoute = router.getCurrentRoute();
  const char *elements[MAX_STRING_POINTERS];
  uint32_t count = 0;

  switch (currentRoute.type) {
  case Route_t::ARTISTS:
    count = musicLookup.getAllArtists(
        elementsBuffer, ELEMENTS_BUFFER_SIZE, (const char **)elements,
        MAX_STRING_POINTERS, currentRoute.currentPage * MAX_STRING_POINTERS);
    currentRoute.hasMore = (count == MAX_STRING_POINTERS);
    break;

  case Route_t::ALBUMS:
    // Check if we're loading albums by artist or genre
    if (router.getDepth() > 1) {
      Route_t &parentRoute = router.routeStack[router.getDepth() - 2];
      if (parentRoute.type == Route_t::ARTISTS) {
        count = musicLookup.getAlbumsByArtist(
            parentRoute.entityId, elementsBuffer, ELEMENTS_BUFFER_SIZE,
            (const char **)elements, MAX_STRING_POINTERS);
        currentRoute.hasMore = false; // Relationship lookups return all results
      } else if (parentRoute.type == Route_t::GENRES) {
        count = musicLookup.getAlbumsByGenre(
            parentRoute.entityId, elementsBuffer, ELEMENTS_BUFFER_SIZE,
            (const char **)elements, MAX_STRING_POINTERS);
        currentRoute.hasMore = false;
      } else {
        count = musicLookup.getAllAlbums(
            elementsBuffer, ELEMENTS_BUFFER_SIZE, (const char **)elements,
            MAX_STRING_POINTERS,
            currentRoute.currentPage * MAX_STRING_POINTERS);
        currentRoute.hasMore = (count == MAX_STRING_POINTERS);
      }
    } else {
      count = musicLookup.getAllAlbums(
          elementsBuffer, ELEMENTS_BUFFER_SIZE, (const char **)elements,
          MAX_STRING_POINTERS, currentRoute.currentPage * MAX_STRING_POINTERS);
      currentRoute.hasMore = (count == MAX_STRING_POINTERS);
    }
    break;

  case Route_t::GENRES:
    count = musicLookup.getAllGenres(
        elementsBuffer, ELEMENTS_BUFFER_SIZE, (const char **)elements,
        MAX_STRING_POINTERS, currentRoute.currentPage * MAX_STRING_POINTERS);
    currentRoute.hasMore = (count == MAX_STRING_POINTERS);
    break;

  case Route_t::TRACKS:
    // Load tracks based on parent entity
    if (router.getDepth() > 1) {
      Route_t &parentRoute = router.routeStack[router.getDepth() - 2];
      if (parentRoute.type == Route_t::ARTISTS) {
        count = musicLookup.getTracksByArtist(
            parentRoute.entityId, elementsBuffer, ELEMENTS_BUFFER_SIZE,
            (const char **)elements, MAX_STRING_POINTERS);
      } else if (parentRoute.type == Route_t::ALBUMS) {
        count = musicLookup.getTracksByAlbum(
            parentRoute.entityId, elementsBuffer, ELEMENTS_BUFFER_SIZE,
            (const char **)elements, MAX_STRING_POINTERS);
      } else if (parentRoute.type == Route_t::GENRES) {
        count = musicLookup.getTracksByGenre(
            parentRoute.entityId, elementsBuffer, ELEMENTS_BUFFER_SIZE,
            (const char **)elements, MAX_STRING_POINTERS);
      }
      currentRoute.hasMore = false; // Relationship lookups return all results
    }
    break;

  case Route_t::ROOT:
    // Use the structured StaticMenu for root menu
    count = rootMenu.getItemCount();
    // Copy root menu elements to our elements array
    int offset = 0;
    for (uint32_t i = 0; i < count && i < MAX_STRING_POINTERS; i++) {
      const MenuItem& item = rootMenu.getItem(i);
      elements[i] = elementsBuffer + offset;
      strcpy(elementsBuffer + offset, item.label);
      offset += strlen(item.label) + 1; // +1 for null terminator
    }
    currentRoute.hasMore = false; // Static menus don't have pagination
    break;
  // case Route_t::SEARCH_RESULTS:
  // Handle root or search results
  // default:
    // return;
  }

  // Update route info and set elements
  currentRoute.totalResults = count;
  topVisibleTrackIndex = 0;
  setElements((const char **)elements, count);
  updateVisibleTracks();
}

// Data loading methods - convenience functions that delegate to
// loadCurrentRouteData
void State::loadArtists(MusicLookup &musicLookup, uint32_t offset) {
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.currentPage = offset / MAX_STRING_POINTERS;
  loadCurrentRouteData(musicLookup);
}

void State::loadAlbums(MusicLookup &musicLookup, uint32_t offset) {
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.currentPage = offset / MAX_STRING_POINTERS;
  loadCurrentRouteData(musicLookup);
}

void State::loadGenres(MusicLookup &musicLookup, uint32_t offset) {
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.currentPage = offset / MAX_STRING_POINTERS;
  loadCurrentRouteData(musicLookup);
}

void State::loadTracksByArtist(MusicLookup &musicLookup, uint32_t artistId) {
  // Set up route context for tracks by artist
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.type = Route_t::TRACKS;
  currentRoute.entityId = artistId;
  loadCurrentRouteData(musicLookup);
}

void State::loadTracksByAlbum(MusicLookup &musicLookup, uint32_t albumId) {
  // Set up route context for tracks by album
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.type = Route_t::TRACKS;
  currentRoute.entityId = albumId;
  loadCurrentRouteData(musicLookup);
}

void State::loadTracksByGenre(MusicLookup &musicLookup, uint32_t genreId) {
  // Set up route context for tracks by genre
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.type = Route_t::TRACKS;
  currentRoute.entityId = genreId;
  loadCurrentRouteData(musicLookup);
}

void State::loadAlbumsByArtist(MusicLookup &musicLookup, uint32_t artistId) {
  // Set up route context for albums by artist
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.type = Route_t::ALBUMS;
  currentRoute.entityId = artistId;
  loadCurrentRouteData(musicLookup);
}

void State::loadAlbumsByGenre(MusicLookup &musicLookup, uint32_t genreId) {
  // Set up route context for albums by genre
  Route_t &currentRoute = router.getCurrentRoute();
  currentRoute.type = Route_t::ALBUMS;
  currentRoute.entityId = genreId;
  loadCurrentRouteData(musicLookup);
}

// Global state instance
State state;