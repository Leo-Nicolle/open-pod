#pragma once
#include "state.h"
#include "static_menu.h"

// Application state management

State::State()
    : EventTarget(), elements(nullptr), totalElements(0), playingTrackIndex(-1),
      isPlaying(false), playbackPosition(0), trackDuration(0),
      isAnimating(false), scrollAnimId(0), transitionAnimId(0),
      isRotatedMode(false) {}

void State::startPlayback(int trackIndex) {
  if (trackIndex == -1) {
    trackIndex = selectedIndex;
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
    return elements[selectedIndex];
  }
  return nullptr;
}

const char *State::getPlayingTrackName() const {
  if (playingTrackIndex >= 0 && playingTrackIndex < totalElements) {
    return elements[playingTrackIndex];
  }
  return nullptr;
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
      const MenuItem &item = rootMenu.getItem(i);
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
  topVisibleIndex = 0;
  setElements((const char **)elements, count);
  updateVisibleElements();
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
  for (int i = 0; i < ELEMENTS_PER_SCREEN; i++) {
    if (i + topVisibleIndex < totalElements) {
      visibleElements[i] = elements[i + topVisibleIndex];
    } else {
      visibleElements[i] = nullptr;
    }
  }
}

void State::setElements(const char **elementList, int count) {
  elements = elementList;
  totalElements = count;

  if (selectedIndex >= count) {
    selectedIndex = count > 0 ? count - 1 : 0;
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

void State::goToSelected(MusicLookup &musicLookup) {
  Route_t &currentRoute = router.getCurrentRoute();
  Route_t::RouteType oldRouteType = currentRoute.type;
  router.saveCurrentSelection(selectedIndex, topVisibleIndex);
  Route_t::RouteType newRouteType = currentRoute.type;
  uint32_t entityId = 0;
  const char *entityName = nullptr;

  switch (currentRoute.type) {
  case Route_t::ROOT: {
    const MenuItem &selectedItem = rootMenu.getItem(selectedIndex);
    selectedIndex = 0;
    topVisibleIndex = 0;
    switch (selectedItem.id) {
    case 1: // Tracks - go to all tracks view
      // TODO: Implement all tracks browsing
      break;
    case 2: // Artists
      newRouteType = Route_t::ARTISTS;
      break;
    case 3: // Albums
      newRouteType = Route_t::ALBUMS;
      break;
    case 4: // Genres
      newRouteType = Route_t::GENRES;
      break;
    case 5: // Search
      // TODO: Implement search interface
      break;
    case 6: // Settings
      // TODO: Implement settings menu
      break;
    case 7: // Now Playing
      newRouteType = Route_t::NOW_PLAYING;
      break;
    }
    break;
  }
  case Route_t::ARTISTS: {
    uint32_t artistId = getSelectedEntityId(musicLookup);
    if (artistId > 0) {
      char artistName[64];
      musicLookup.getArtistName(artistId, artistName, sizeof(artistName));
      entityId = artistId;
      entityName = artistName;
      newRouteType = Route_t::ALBUMS;
    }
    break;
  }

  case Route_t::ALBUMS: {
    uint32_t albumId = getSelectedEntityId(musicLookup);
    if (albumId > 0) {
      char albumName[64];
      musicLookup.getAlbumName(albumId, albumName, sizeof(albumName));
      entityId = albumId;
      entityName = albumName;
      newRouteType = Route_t::TRACKS;
    }
    break;
  }

  case Route_t::GENRES: {
    uint32_t genreId = getSelectedEntityId(musicLookup);
    if (genreId > 0) {
      char genreName[64];
      musicLookup.getGenreName(genreId, genreName, sizeof(genreName));
      entityId = genreId;
      entityName = genreName;
      newRouteType = Route_t::ARTISTS;
    }
    break;
  }

  case Route_t::TRACKS: {
    // Start playback of selected track
    startPlayback(selectedIndex);
    newRouteType = Route_t::NOW_PLAYING;
    break;
  }

    // case Route_t::SEARCH_RESULTS:
    // Handle search result selection based on result type
    // TODO: Implement search result navigation
    // break;
  }
  router.pushRoute(newRouteType, entityId, entityName);
  RouteChangedEvent event = {oldRouteType, newRouteType,      entityId, 0, 0,
                             entityName,   router.canGoBack()};
  loadCurrentRouteData(musicLookup);
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

bool State::back(MusicLookup &musicLookup) {
  if (!router.canGoBack()) {
    return false;
  }

  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;
  router.popRoute();
  loadCurrentRouteData(musicLookup);
  restoreSelectionState();
  Route_t &currentRoute = router.getCurrentRoute();
  RouteChangedEvent event = {oldRouteType,
                             currentRoute.type,
                             currentRoute.entityId,
                             currentRoute.topVisibleIndex,
                             currentRoute.selectedIndex,
                             currentRoute.entityName,
                             router.canGoBack()};
  emitEvent(EVENT_ROUTE_CHANGED, &event);

  return true;
}

void State::backToMain(MusicLookup &musicLookup) {
  // Clear the route stack and go back to root
  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;

  while (router.canGoBack()) {
    router.popRoute();
  }

  // Ensure we're at root
  if (router.getCurrentRoute().type != Route_t::ROOT) {
    router.pushRoute(Route_t::ROOT);
  }
  selectedIndex = 0;
  topVisibleIndex = 0;
  Route_t currentRoute = router.getCurrentRoute();
  loadCurrentRouteData(musicLookup);
  RouteChangedEvent event = {oldRouteType,
                             currentRoute.type,
                             0,
                             0,
                             currentRoute.entityId,
                             currentRoute.entityName,
                             router.canGoBack()};
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

uint32_t State::getSelectedEntityId(MusicLookup &musicLookup) {
  Route_t &currentRoute = router.getCurrentRoute();

  switch (currentRoute.type) {
  case Route_t::ARTISTS:
    return musicLookup.getArtistIdAtIndex(selectedIndex);

  case Route_t::ALBUMS:
    // Check if we're in a parent context (albums by artist/genre)
    if (router.getDepth() > 1) {
      Route_t &parentRoute = router.routeStack[router.getDepth() - 2];
      if (parentRoute.type == Route_t::ARTISTS) {
        return musicLookup.getAlbumIdByArtistAtIndex(parentRoute.entityId,
                                                     selectedIndex);
      } else if (parentRoute.type == Route_t::GENRES) {
        return musicLookup.getAlbumIdByGenreAtIndex(parentRoute.entityId,
                                                    selectedIndex);
      }
    }
    // Browsing all albums
    return musicLookup.getAlbumIdAtIndex(selectedIndex);

  case Route_t::GENRES:
    return musicLookup.getGenreIdAtIndex(selectedIndex);

  case Route_t::TRACKS:
    // Tracks always have parent context
    if (router.getDepth() > 1) {
      Route_t &parentRoute = router.routeStack[router.getDepth() - 2];
      if (parentRoute.type == Route_t::ARTISTS) {
        return musicLookup.getTrackIdByArtistAtIndex(parentRoute.entityId,
                                                     selectedIndex);
      } else if (parentRoute.type == Route_t::ALBUMS) {
        return musicLookup.getTrackIdByAlbumAtIndex(parentRoute.entityId,
                                                    selectedIndex);
      } else if (parentRoute.type == Route_t::GENRES) {
        return musicLookup.getTrackIdByGenreAtIndex(parentRoute.entityId,
                                                    selectedIndex);
      }
    }
    break;

  case Route_t::ROOT:
  case Route_t::SEARCH_RESULTS:
  default:
    break;
  }

  return 0;
}

void State::restoreSelectionState() {
  Route_t &currentRoute = router.getCurrentRoute();
  selectedIndex = currentRoute.selectedIndex;
  topVisibleIndex = currentRoute.topVisibleIndex;

  // Clamp to valid ranges
  selectedIndex = constrain(selectedIndex, 0, max(0, totalElements - 1));
  topVisibleIndex = constrain(topVisibleIndex, 0,
                              max(0, totalElements - ELEMENTS_PER_SCREEN));

  updateVisibleElements();
}
// Global state instance
State state;