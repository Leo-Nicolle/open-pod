#include "state.h"
#include "static_menu.h"

// State: route navigation (loading a route's list, entering/leaving routes,
// resolving the selected entity) and the search screen. The rest of State is
// in state.cpp.

int State::routeListLength(MusicLookup &musicLookup) {
  Route_t &route = router.getCurrentRoute();
  // Scoped lists (an artist's albums, an album's tracks…): route.entityId
  // (set when the route was pushed) is the artist/album/genre id, and
  // route.scope says which kind of id it is.
  bool scoped = router.getDepth() > 0;
  switch (route.type) {
  case Route_t::ARTISTS:
    return musicLookup.getArtistCount();
  case Route_t::ALBUMS:
    if (scoped && route.scope == Route_t::ARTISTS)
      return musicLookup.getAlbumCountForArtist(route.entityId);
    if (scoped && route.scope == Route_t::GENRES)
      return musicLookup.getAlbumCountForGenre(route.entityId);
    return musicLookup.getAlbumCount();
  case Route_t::GENRES:
    return musicLookup.getGenreCount();
  case Route_t::TRACKS:
    if (scoped && route.scope == Route_t::ARTISTS)
      return musicLookup.getTrackCountForArtist(route.entityId);
    if (scoped && route.scope == Route_t::ALBUMS)
      return musicLookup.getTrackCountForAlbum(route.entityId);
    if (scoped && route.scope == Route_t::GENRES)
      return musicLookup.getTrackCountForGenre(route.entityId);
    // Reached directly from ROOT ("Tracks" root menu item): the full
    // track catalog.
    return musicLookup.getTrackCount();
  default:
    return 0;
  }
}

int State::loadWindow(int start) {
  return loadNames(start, elementsBuffer, ELEMENTS_BUFFER_SIZE, stringPointers,
                   MAX_STRING_POINTERS);
}

int State::loadNames(int start, char *buf, uint32_t size, const char **ptrs,
                     uint32_t n) {
  MusicLookup &m = *windowLookup;
  Route_t &route = router.getCurrentRoute();
  bool scoped = router.getDepth() > 0;
  const uint32_t id = route.entityId;
  switch (route.type) {
  case Route_t::ARTISTS:
    return m.getAllArtists(buf, size, ptrs, n, start);
  case Route_t::ALBUMS:
    if (scoped && route.scope == Route_t::ARTISTS)
      return m.getAlbumsByArtist(id, buf, size, ptrs, n, start);
    if (scoped && route.scope == Route_t::GENRES)
      return m.getAlbumsByGenre(id, buf, size, ptrs, n, start);
    return m.getAllAlbums(buf, size, ptrs, n, start);
  case Route_t::GENRES:
    return m.getAllGenres(buf, size, ptrs, n, start);
  case Route_t::TRACKS:
    if (scoped && route.scope == Route_t::ARTISTS)
      return m.getTracksByArtist(id, buf, size, ptrs, n, start);
    if (scoped && route.scope == Route_t::ALBUMS)
      return m.getTracksByAlbum(id, buf, size, ptrs, n, start);
    if (scoped && route.scope == Route_t::GENRES)
      return m.getTracksByGenre(id, buf, size, ptrs, n, start);
    return m.getAllTracks(buf, size, ptrs, n, start);
  default:
    return 0;
  }
}

int State::globalList() {
  if (elements || !windowLookup) {
    return -1;
  }
  Route_t &route = router.getCurrentRoute();
  bool scoped = router.getDepth() > 0;
  bool byArtistOrGenre = scoped && (route.scope == Route_t::ARTISTS ||
                                    route.scope == Route_t::GENRES);
  switch (route.type) {
  case Route_t::ARTISTS:
    return 0;
  case Route_t::ALBUMS:
    return byArtistOrGenre ? -1 : 1;
  case Route_t::GENRES:
    return 2;
  case Route_t::TRACKS:
    return byArtistOrGenre || (scoped && route.scope == Route_t::ALBUMS) ? -1
                                                                          : 3;
  default:
    return -1;
  }
}

void State::loadCurrentRouteData(MusicLookup &musicLookup) {
  Route_t &currentRoute = router.getCurrentRoute();

  switch (currentRoute.type) {
  case Route_t::ARTISTS:
  case Route_t::ALBUMS:
  case Route_t::GENRES:
  case Route_t::TRACKS: {
    // Callers position selectedIndex/topVisibleIndex first (0 when entering
    // a route, the saved values on back()), so the list event carries them.
    int count = routeListLength(musicLookup);
    currentRoute.totalResults = count;
    currentRoute.hasMore = false;
    setWindowedElements(musicLookup, count);
    return;
  }

  case Route_t::SEARCH_RESULTS:
    // The search screen draws from its own model, which survives the trip
    // into a result and back. setElements() would make the UI paint a list.
    currentRoute.totalResults = search.getHitCount();
    return;

  case Route_t::SETTINGS:
  case Route_t::BT_LIST:
  case Route_t::BT_DEVICE:
  case Route_t::BT_FORGET:
  case Route_t::BT_RENAME:
    // Rows come from BtMenu (state/bt_menu.h), drawn by ui/menu_view.hpp.
    return;

  case Route_t::PODCASTS:
  case Route_t::PODCAST_FOLDER:
  case Route_t::PODCAST_ACTIONS:
    // Rows come from PodcastMenu (state/podcast_menu.h), drawn by
    // ui/podcast_view.hpp.
    return;

  case Route_t::ROOT: {
    // The root menu is a static list.
    uint32_t count = rootMenu.getItemCount();
    int offset = 0;
    for (uint32_t i = 0; i < count && i < MAX_STRING_POINTERS; i++) {
      const MenuItem &item = rootMenu.getItem(i);
      stringPointers[i] = elementsBuffer + offset;
      strcpy(elementsBuffer + offset, item.label);
      offset += strlen(item.label) + 1; // +1 for null terminator
    }
    currentRoute.hasMore = false; // Static menus don't have pagination
    currentRoute.totalResults = count;
    setElements(stringPointers, count);
    return;
  }

  default:
    return;
  }
}

void State::goToSelected(MusicLookup &musicLookup) {
  endTransition();
  Route_t &currentRoute = router.getCurrentRoute();
  Route_t::RouteType oldRouteType = currentRoute.type;
  router.saveCurrentSelection(selectedIndex, topVisibleIndex);
  Route_t::RouteType newRouteType = currentRoute.type;
  // What kind of id entityId is: the route we come from, except when search
  // opens an artist/album/genre directly.
  Route_t::RouteType scope = currentRoute.type;
  uint32_t entityId = 0;
  const char *entityName = nullptr;
  char name[64];

  switch (currentRoute.type) {
  case Route_t::ROOT: {
    const MenuItem &selectedItem = rootMenu.getItem(selectedIndex);
    switch (selectedItem.id) {
    case 1: // Tracks - browse the full track catalog
      newRouteType = Route_t::TRACKS;
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
    case 8: // Podcasts
      newRouteType = Route_t::PODCASTS;
      break;
    case 5: // Search
      search.reset();
      newRouteType = Route_t::SEARCH_RESULTS;
      break;
    case 6: // Settings
      newRouteType = Route_t::SETTINGS;
      break;
    case 7: // Now Playing
      newRouteType = Route_t::NOW_PLAYING;
      break;
    }
    break;
  }
  case Route_t::ARTISTS: {
    uint32_t artistId = getSelectedEntityId(musicLookup);
    if (artistId == UINT32_MAX)
      break;
    musicLookup.getArtistName(artistId, name, sizeof(name));
    entityId = artistId;
    entityName = name;
    newRouteType = Route_t::ALBUMS;
    break;
  }

  case Route_t::ALBUMS: {
    uint32_t albumId = getSelectedEntityId(musicLookup);
    if (albumId == UINT32_MAX)
      break;

    musicLookup.getAlbumName(albumId, name, sizeof(name));
    entityId = albumId;
    entityName = name;
    newRouteType = Route_t::TRACKS;
    break;
  }

  case Route_t::GENRES: {
    uint32_t genreId = getSelectedEntityId(musicLookup);
    if (genreId == UINT32_MAX)
      break;
    musicLookup.getGenreName(genreId, name, sizeof(name));
    entityId = genreId;
    entityName = name;
    // Genre -> Albums (scoped via getAlbumsByGenre/getAlbumIdByGenreAtIndex),
    // mirroring the Artist -> Albums step. Genre -> Artists is not wired up
    // (genre_to_artists is never loaded from SD - see MusicLookup::init()),
    // so routing here used to silently drop the genre scope and fall through
    // to the unfiltered "all artists" list.
    newRouteType = Route_t::ALBUMS;
    break;
  }

  case Route_t::SEARCH_RESULTS: {
    const SearchModel::Hit *hit = search.getSelectedHit();
    if (!hit)
      break;
    switch (hit->type) {
    case SEARCH_ARTIST:
      musicLookup.getArtistName(hit->id, name, sizeof(name));
      newRouteType = Route_t::ALBUMS;
      scope = Route_t::ARTISTS;
      break;
    case SEARCH_GENRE:
      musicLookup.getGenreName(hit->id, name, sizeof(name));
      newRouteType = Route_t::ALBUMS;
      scope = Route_t::GENRES;
      break;
    case SEARCH_ALBUM:
      musicLookup.getAlbumName(hit->id, name, sizeof(name));
      newRouteType = Route_t::TRACKS;
      scope = Route_t::ALBUMS;
      break;
    case SEARCH_TRACK: {
      // Play it within its album, so next/previous carry on through the
      // album like they do when it's picked from the album's track list.
      uint32_t albumId = musicLookup.getAlbumIdForTrack(hit->id);
      if (albumId == UINT32_MAX)
        break;
      int index = musicLookup.getTrackIndexInAlbum(albumId, hit->id);
      if (index < 0)
        break;
      playingListEntityId = albumId;
      playingListTotalTracks = (int)musicLookup.getTrackCountForAlbum(albumId);
      playingListParentType = Route_t::ALBUMS;
      startPlayback(index, hit->id, musicLookup);
      newRouteType = Route_t::NOW_PLAYING;
      break;
    }
    }
    if (newRouteType != Route_t::NOW_PLAYING &&
        newRouteType != Route_t::SEARCH_RESULTS) {
      entityId = hit->id;
      entityName = name;
    }
    break;
  }

  case Route_t::TRACKS: {
    // Start playback of selected track
    uint32_t trackId = getSelectedEntityId(musicLookup);
    if (trackId == UINT32_MAX)
      break;
    // Capture the source-list context now, while currentRoute is still the
    // TRACKS route being played from - this is the only point it's reliably
    // available and must not be re-derived later (see notifyTrackEnded /
    // resolveTrackId).
    playingListEntityId = currentRoute.entityId;
    playingListTotalTracks = totalElements;
    playingListParentType = (router.getDepth() > 0)
        ? currentRoute.scope
        : Route_t::ROOT;
    startPlayback(selectedIndex, trackId, musicLookup);
    newRouteType = Route_t::NOW_PLAYING;
    break;
  }

  default:
    // Settings and the Bluetooth screens navigate with pushMenuRoute().
    break;
  }

  // Unimplemented menu items (Tracks/Search/Settings from ROOT) and failed
  // entity lookups leave newRouteType unchanged. Nothing pushed a route in
  // that case, so there is no transition animation to end this call's
  // setAnimating(true) - firing it anyway would leave isAnimating stuck true
  // forever and lock out all further navigation.
  if (newRouteType == oldRouteType) {
    return;
  }

  if (newRouteType != Route_t::NOW_PLAYING) {
    selectedIndex = 0;
    topVisibleIndex = 0;
  }
  router.pushRoute(newRouteType, entityId, entityName, scope);
  RouteChangedEvent event = {
      oldRouteType, newRouteType, entityId,          0, 0,
      entityName,   true,         router.canGoBack()};
  setAnimating(true);
  if (newRouteType != Route_t::NOW_PLAYING) {
    loadCurrentRouteData(musicLookup);
  }
  fastScrollLetters = 0;
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

bool State::back(MusicLookup &musicLookup) {
  endTransition();
  if (!router.canGoBack()) {
    return false;
  }

  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;
  router.popRoute();
  setAnimating(true);
  if(oldRouteType != Route_t::NOW_PLAYING) {
    // Restore the saved scroll position *before* loading: setElements()
    // emits EVENT_TRACK_LIST_UPDATED, and the UI builds its row cache from
    // that event's topVisibleIndex. Restoring afterwards left the UI showing
    // the top of the list while the state pointed further down.
    Route_t &restoredRoute = router.getCurrentRoute();
    selectedIndex = restoredRoute.selectedIndex;
    topVisibleIndex = restoredRoute.topVisibleIndex;
    loadCurrentRouteData(musicLookup);
    restoreSelectionState(); // clamps to the loaded list
  }
  Route_t &currentRoute = router.getCurrentRoute();
  RouteChangedEvent event = {oldRouteType,
                             currentRoute.type,
                             currentRoute.entityId,
                             currentRoute.topVisibleIndex,
                             currentRoute.selectedIndex,
                             currentRoute.entityName,
                             false,
                             router.canGoBack()};
  fastScrollLetters = 0;
  emitEvent(EVENT_ROUTE_CHANGED, &event);

  return true;
}

bool State::backTo(Route_t::RouteType type, MusicLookup &musicLookup) {
  endTransition();
  // Drop the routes in between, then back() into `type` with one transition.
  while (router.stackDepth > 1 &&
         router.routeStack[router.stackDepth - 1].type != type) {
    router.popRoute();
  }
  return back(musicLookup);
}

void State::pushMenuRoute(Route_t::RouteType type, uint32_t entityId,
                          const char *title, int selected) {
  endTransition();
  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;
  // The current menu route already holds its own selection (BtMenu::sync).
  router.pushRoute(type, entityId, title);
  Route_t &route = router.getCurrentRoute();
  route.selectedIndex = selected;
  route.topVisibleIndex = 0;
  RouteChangedEvent event = {oldRouteType, type,  entityId,
                             0,            (uint32_t)selected,
                             title,        true,  router.canGoBack()};
  setAnimating(true);
  fastScrollLetters = 0;
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

void State::backToMain(MusicLookup &musicLookup) {
  endTransition();
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
  setAnimating(true);
  loadCurrentRouteData(musicLookup);
  RouteChangedEvent event = {oldRouteType,
                             currentRoute.type,
                             0,
                             0,
                             currentRoute.entityId,
                             currentRoute.entityName,
                             false,
                             router.canGoBack()};
  fastScrollLetters = 0;
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

void State::goToNowPlaying(MusicLookup &musicLookup) {
  Route_t::RouteType oldRouteType = router.getCurrentRoute().type;
  if (oldRouteType == Route_t::NOW_PLAYING) {
    return; // Already there - no-op, same guard style as goToSelected.
  }
  endTransition();
  router.saveCurrentSelection(selectedIndex, topVisibleIndex);
  router.pushRoute(Route_t::NOW_PLAYING);
  RouteChangedEvent event = {oldRouteType, Route_t::NOW_PLAYING, 0, 0, 0,
                             nullptr,      true,                router.canGoBack()};
  setAnimating(true);
  fastScrollLetters = 0;
  emitEvent(EVENT_ROUTE_CHANGED, &event);
}

uint32_t State::getSelectedEntityId(MusicLookup &musicLookup) {
  Route_t &currentRoute = router.getCurrentRoute();

  switch (currentRoute.type) {
  case Route_t::ARTISTS:
    return musicLookup.getArtistIdAtIndex(selectedIndex);

  case Route_t::ALBUMS:
    // Check if we're in a parent context (albums by artist/genre).
    // currentRoute.entityId is the artist/genre id this ALBUMS route was
    // scoped to; its scope tells us which kind of id it is.
    if (router.getDepth() > 0) {
      Route_t::RouteType scope = currentRoute.scope;
      if (scope == Route_t::ARTISTS) {
        return musicLookup.getAlbumIdByArtistAtIndex(currentRoute.entityId,
                                                     selectedIndex);
      } else if (scope == Route_t::GENRES) {
        return musicLookup.getAlbumIdByGenreAtIndex(currentRoute.entityId,
                                                    selectedIndex);
      }
    }
    // Browsing all albums
    return musicLookup.getAlbumIdAtIndex(selectedIndex);

  case Route_t::GENRES:
    return musicLookup.getGenreIdAtIndex(selectedIndex);

  case Route_t::TRACKS:
    // Tracks always have parent context. currentRoute.entityId is the
    // artist/album/genre id this TRACKS route was scoped to; its
    // scope tells us which kind of id it is.
    if (router.getDepth() > 0) {
      Route_t::RouteType scope = currentRoute.scope;
      if (scope == Route_t::ARTISTS) {
        return musicLookup.getTrackIdByArtistAtIndex(currentRoute.entityId,
                                                     selectedIndex);
      } else if (scope == Route_t::ALBUMS) {
        return musicLookup.getTrackIdByAlbumAtIndex(currentRoute.entityId,
                                                    selectedIndex);
      } else if (scope == Route_t::GENRES) {
        return musicLookup.getTrackIdByGenreAtIndex(currentRoute.entityId,
                                                    selectedIndex);
      }
      // Reached directly from ROOT ("Tracks" root menu item) - selectedIndex
      // indexes the flat track catalog, not a relationship-scoped subset.
      return musicLookup.getTrackIdAtIndex(selectedIndex);
    }
    break;

  case Route_t::SEARCH_RESULTS: {
    const SearchModel::Hit *hit = search.getSelectedHit();
    return hit ? hit->id : UINT32_MAX;
  }

  case Route_t::ROOT:
  default:
    break;
  }

  return UINT32_MAX; // Not found - 0 is a valid id, can't mean "not found"
}

uint32_t State::resolveTrackId(MusicLookup &musicLookup, int index) {
  // Uses the source-list context captured at playback-start time (see
  // goToSelected's TRACKS case), not the live router stack - the user may
  // have navigated elsewhere since the track started playing.
  switch (playingListParentType) {
  case Route_t::ARTISTS:
    return musicLookup.getTrackIdByArtistAtIndex(playingListEntityId, index);
  case Route_t::ALBUMS:
    return musicLookup.getTrackIdByAlbumAtIndex(playingListEntityId, index);
  case Route_t::GENRES:
    return musicLookup.getTrackIdByGenreAtIndex(playingListEntityId, index);
  case Route_t::ROOT:
    // Played from the flat "Tracks" root menu list - index is absolute into
    // the full track catalog, not scoped to any artist/album/genre.
    return musicLookup.getTrackIdAtIndex(index);
  default:
    return UINT32_MAX; // Not found - 0 is a valid id, can't mean "not found"
  }
}

const char *State::getHeaderTitle() {
  Route_t &route = router.getCurrentRoute();
  if (route.entityName[0] != '\0') {
    return route.entityName;
  }
  switch (route.type) {
  case Route_t::ROOT:
    return "Menu";
  case Route_t::ARTISTS:
    return "Artists";
  case Route_t::ALBUMS:
    return "Albums";
  case Route_t::GENRES:
    return "Genres";
  case Route_t::TRACKS:
    return "Tracks";
  case Route_t::SEARCH_RESULTS:
    return "Search";
  case Route_t::NOW_PLAYING:
    return "Now Playing";
  case Route_t::SETTINGS:
    return "Settings";
  case Route_t::BT_LIST:
  case Route_t::BT_DEVICE:
  case Route_t::BT_FORGET:
  case Route_t::BT_RENAME:
    return "Bluetooth";
  case Route_t::PODCASTS:
  case Route_t::PODCAST_FOLDER:
  case Route_t::PODCAST_ACTIONS:
    return "Podcasts";
  }
  return "OpenPod";
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

void State::emitSearchChanged(uint8_t dirty, int oldSelected, int oldFirst) {
  SearchEvent event = {dirty, oldSelected, oldFirst};
  emitEvent(EVENT_SEARCH_CHANGED, &event);
}

void State::searchScroll(int clicks, MusicLookup &musicLookup) {
  // Negative clicks scroll lists down, so they move the ribbon cursor
  // forward (A -> Z) and the highlight down.
  int oldSelected = search.getSelected();
  int oldFirst = search.getFirst();
  if (search.getMode() == SearchModel::TYPING) {
    if (search.moveCursor(-clicks))
      emitSearchChanged(SEARCH_DIRTY_RIBBON, oldSelected, oldFirst);
  } else if (search.scroll(-clicks, musicLookup)) {
    uint8_t dirty = SEARCH_DIRTY_SELECTION;
    if (search.getFirst() != oldFirst)
      dirty |= SEARCH_DIRTY_ROWS;
    emitSearchChanged(dirty, oldSelected, oldFirst);
  }
}

void State::searchAppend(MusicLookup &musicLookup) {
  if (search.append(musicLookup))
    emitSearchChanged(SEARCH_DIRTY_HEADER | SEARCH_DIRTY_ROWS, 0, 0);
}

void State::searchBackspace(MusicLookup &musicLookup) {
  if (search.backspace(musicLookup))
    emitSearchChanged(SEARCH_DIRTY_HEADER | SEARCH_DIRTY_ROWS, 0, 0);
}

void State::searchSpace(MusicLookup &musicLookup) {
  if (search.space(musicLookup))
    emitSearchChanged(SEARCH_DIRTY_HEADER | SEARCH_DIRTY_ROWS, 0, 0);
}

void State::searchEnterResults() {
  if (search.enterResults())
    emitSearchChanged(SEARCH_DIRTY_LAYOUT, 0, 0);
}

void State::searchExitResults() {
  if (search.exitResults())
    emitSearchChanged(SEARCH_DIRTY_LAYOUT, 0, 0);
}
