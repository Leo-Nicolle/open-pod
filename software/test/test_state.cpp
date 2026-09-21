#include "../src/state/state.h"
#include "../src/state/event-target.h"
#include "../src/storage/Music_lookup.h"
#include "test_helpers.h"
#include <doctest.h>
#include <cstring>  // for strcmp
#include <cmath>    // for abs

// State's public API has changed substantially since this file was written
// (setTracks/selectTrack/startPlayback(index) with no MusicLookup are all
// gone - see state.h). These tests are adapted to the current
// setElements()/scrollUp()/scrollDown()/pageUp()/pageDown()/goToSelected()
// API rather than ported literally. Selection is now only ever changed via
// scrolling or by navigating (there is no direct "select index N").

namespace {
struct {
    bool captured = false;
    int eventType = 0;
    void* eventData = nullptr;
} lastEvent;

int eventCount = 0;

void resetTestState() {
    lastEvent.captured = false;
    lastEvent.eventType = 0;
    lastEvent.eventData = nullptr;
    eventCount = 0;
}

// State builds its event payload structs on the stack and only guarantees
// the pointer handed to emitEvent() is valid for the duration of that
// (synchronous) listener call. These tests inspect the event data *after*
// the call that emitted it has returned, so the callback has to copy the
// payload into storage that outlives the call rather than just stash the
// (about to dangle) pointer.
unsigned char lastEventDataStorage[64];

size_t eventDataSize(int eventType) {
  switch (eventType) {
  case EVENT_TRACK_SELECTED:
    return sizeof(TrackSelectedEvent);
  case EVENT_SCROLL_CHANGED:
  case EVENT_PAGE_CHANGED:
    return sizeof(ScrollChangedEvent);
  case EVENT_PLAYBACK_STARTED:
  case EVENT_PLAYBACK_PAUSED:
  case EVENT_PLAYBACK_RESUMED:
  case EVENT_PLAYBACK_STOPPED:
  case EVENT_TRACK_ENDED:
    return sizeof(PlaybackEvent);
  case EVENT_PROGRESS_UPDATED:
    return sizeof(ProgressEvent);
  case EVENT_ANIMATION_STARTED:
  case EVENT_ANIMATION_FINISHED:
    return sizeof(AnimationEvent);
  case EVENT_TRACK_LIST_UPDATED:
    return sizeof(ListEvent);
  case EVENT_ROUTE_CHANGED:
    return sizeof(RouteChangedEvent);
  case EVENT_VOLUME_CHANGED:
    return sizeof(VolumeEvent);
  case EVENT_SEEK_MODE_CHANGED:
    return sizeof(SeekModeEvent);
  case EVENT_TRACK_DURATION_CHANGED:
    return sizeof(int);
  default:
    return 0;
  }
}

void testEventCallback(int eventType, void* eventData, EventTarget* target) {
    lastEvent.captured = true;
    lastEvent.eventType = eventType;
    if (eventData) {
      size_t sz = eventDataSize(eventType);
      if (sz > sizeof(lastEventDataStorage)) sz = sizeof(lastEventDataStorage);
      memcpy(lastEventDataStorage, eventData, sz);
      lastEvent.eventData = lastEventDataStorage;
    } else {
      lastEvent.eventData = nullptr;
    }
    eventCount++;
}

// Generic element list for tests that only exercise list navigation
// (setElements/scroll*/page*) and don't need a real MusicLookup-backed
// track list.
const char* genericElements[] = {
    "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
    "Item 6", "Item 7", "Item 8", "Item 9", "Item 10"
};
const int genericElementCount = 10;

// Navigates a freshly constructed State from the boot ROOT menu down into
// the Tracks list for "Double Album" (registerStateTestCatalog(), 2 tracks:
// "First Song"/id 1 and "Second Song"/id 2), leaving selectedIndex at 0 (the
// first track) and the current route at TRACKS. Does not start playback -
// callers that need that call state.goToSelected(lookup) once more.
void navigateToTracks(State &state, MusicLookup &lookup) {
  state.loadCurrentRouteData(lookup); // ROOT: load the static root menu
  state.scrollDown();
  state.scrollDown(); // selectedIndex 0 -> 2 ("Albums" root menu item)
  state.goToSelected(lookup); // ROOT -> ALBUMS ("Double Album")
  state.goToSelected(lookup); // ALBUMS -> TRACKS (["First Song", "Second Song"])
}

void setupCatalog() {
  openpod_test::resetAll();
  openpod_test::registerStateTestCatalog();
}
}

TEST_SUITE("State Construction and Basic Properties") {
  TEST_CASE("State - Default Construction") {
    State state;

    CHECK(state.getTotalElements() == 0);
    CHECK(state.getSelectedIndex() == 0);
    CHECK(state.getTopVisibleIndex() == 0);
    CHECK(state.getPlayingTrackIndex() == -1);
    CHECK(state.getIsPlaying() == false);
    CHECK(state.getPlaybackPosition() == 0);
    CHECK(state.getTrackDuration() == 0);
    CHECK(state.getCurrentRoute().type == Route_t::ROOT);
    CHECK(state.getIsAnimating() == false);
    CHECK(state.getIsRotatedMode() == false);
    CHECK(state.getCurrentTrackName() == nullptr);
    CHECK(state.getPlayingTrackName() == nullptr);
  }

  TEST_CASE("State - Set Elements") {
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.setElements(genericElements, genericElementCount);

    CHECK(state.getTotalElements() == genericElementCount);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_TRACK_LIST_UPDATED);

    ListEvent *event = (ListEvent *)lastEvent.eventData;
    CHECK(event->totalElements == genericElementCount);
    CHECK(event->elements == genericElements);
  }

  TEST_CASE("State - Set Elements with Out of Bounds Selection") {
    State state;
    state.setElements(genericElements, genericElementCount);
    for (int i = 0; i < 5; i++) {
      state.scrollDown();
    }
    CHECK(state.getSelectedIndex() == 5);

    const char *smallList[] = {"A", "B", "C"};
    state.setElements(smallList, 3); // Only 3 elements now

    CHECK(state.getSelectedIndex() < 3); // Should be clamped
  }
}

TEST_SUITE("State Navigation Tests") {
  TEST_CASE("State - Scroll Up from Beginning") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.addEventListener(testEventCallback);

    // Already at index 0
    state.scrollUp();

    CHECK(lastEvent.captured == false); // No event at boundary
    CHECK(state.getSelectedIndex() == 0);
  }

  TEST_CASE("State - Scroll Up Normal") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.scrollDown();
    state.scrollDown();
    state.scrollDown(); // selectedIndex -> 3
    state.addEventListener(testEventCallback);

    state.scrollUp();

    CHECK(state.getSelectedIndex() == 2);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_SCROLL_CHANGED);

    ScrollChangedEvent *event = (ScrollChangedEvent *)lastEvent.eventData;
    CHECK(event->selectedIndex == 2);
    CHECK(event->oldSelectedIndex == 3);
  }

  TEST_CASE("State - Scroll Down to End") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    for (int i = 0; i < genericElementCount - 1; i++) {
      state.scrollDown(); // selectedIndex -> last
    }
    state.addEventListener(testEventCallback);

    state.scrollDown();

    CHECK(lastEvent.captured == false); // No event at boundary
    CHECK(state.getSelectedIndex() == genericElementCount - 1);
  }

  TEST_CASE("State - Scroll Down Normal") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.scrollDown();
    state.scrollDown(); // selectedIndex -> 2
    state.addEventListener(testEventCallback);

    state.scrollDown();

    CHECK(state.getSelectedIndex() == 3);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_SCROLL_CHANGED);

    ScrollChangedEvent *event = (ScrollChangedEvent *)lastEvent.eventData;
    CHECK(event->selectedIndex == 3);
    CHECK(event->oldSelectedIndex == 2);
  }

  TEST_CASE("State - Page Down") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.scrollDown();
    state.scrollDown(); // selectedIndex -> 2
    state.addEventListener(testEventCallback);

    state.pageDown();

    int expectedSelected = min(genericElementCount - 1, 2 + ELEMENTS_PER_SCREEN);
    CHECK(state.getSelectedIndex() == expectedSelected);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PAGE_CHANGED);
  }

  TEST_CASE("State - Page Down from Beginning") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.addEventListener(testEventCallback);

    state.pageDown();

    int expectedSelected = min(genericElementCount - 1, ELEMENTS_PER_SCREEN);
    CHECK(state.getSelectedIndex() == expectedSelected);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PAGE_CHANGED);
  }

  TEST_CASE("State - Page Up") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    state.pageDown(); // Move away from 0 first
    int afterPageDown = state.getSelectedIndex();
    state.addEventListener(testEventCallback);

    state.pageUp();

    int expectedSelected = max(0, afterPageDown - ELEMENTS_PER_SCREEN);
    CHECK(state.getSelectedIndex() == expectedSelected);
  }

  TEST_CASE("State - Page Up from Beginning") {
    State state;
    resetTestState();
    state.setElements(genericElements, genericElementCount);
    // Start at index 0
    state.addEventListener(testEventCallback);

    state.pageUp();

    CHECK(state.getSelectedIndex() == 0); // Should stay at 0
    CHECK(lastEvent.captured == false);   // No movement, no event
  }
}

TEST_SUITE("State Playback Tests") {
  TEST_CASE("State - Start Playback First Track") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    resetTestState();
    state.addEventListener(testEventCallback);

    // At the TRACKS route with selectedIndex 0 ("First Song") - goToSelected
    // now starts playback of the selected track instead of navigating deeper.
    state.goToSelected(lookup);

    CHECK(state.getPlayingTrackIndex() == 0);
    CHECK(state.getIsPlaying() == true);
    CHECK(state.getPlaybackPosition() == 0);
    CHECK(strcmp(state.getPlayingTrackName(), "First Song") == 0);
    CHECK(state.getCurrentRoute().type == Route_t::NOW_PLAYING);
    // goToSelected's TRACKS case fires EVENT_PLAYBACK_STARTED, then an
    // animation event, then EVENT_ROUTE_CHANGED as it transitions to Now
    // Playing - several events, not just one.
    CHECK(eventCount >= 2);
  }

  TEST_CASE("State - Start Playback Second Track") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.scrollDown(); // selectedIndex 0 -> 1 ("Second Song")
    resetTestState();
    state.addEventListener(testEventCallback);

    state.goToSelected(lookup);

    CHECK(state.getPlayingTrackIndex() == 1);
    CHECK(state.getIsPlaying() == true);
    CHECK(strcmp(state.getPlayingTrackName(), "Second Song") == 0);
  }

  TEST_CASE("State - Start Playback With No Active Source List") {
    // startPlayback() bounds-checks against the source-list size captured by
    // a prior goToSelected() navigation into a TRACKS route
    // (playingListTotalTracks); calling it on a fresh State with no such
    // navigation should be rejected. The MusicLookup is never dereferenced
    // in that case, so it doesn't need to be initialized.
    MusicLookup lookup;
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.startPlayback(0, 1, lookup);

    CHECK(state.getPlayingTrackIndex() == -1);
    CHECK(state.getIsPlaying() == false);
    CHECK(lastEvent.captured == false);
  }

  TEST_CASE("State - Toggle Playback Pause") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback of track 0
    resetTestState();
    state.addEventListener(testEventCallback);

    state.togglePlayback(); // Should pause

    CHECK(state.getIsPlaying() == false);
    CHECK(state.getPlayingTrackIndex() == 0); // Still same track
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PLAYBACK_PAUSED);

    PlaybackEvent *event = (PlaybackEvent *)lastEvent.eventData;
    CHECK(event->isPlaying == false);
    CHECK(event->trackIndex == 0);
  }

  TEST_CASE("State - Toggle Playback Resume") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback
    state.togglePlayback();     // Pause first
    resetTestState();
    state.addEventListener(testEventCallback);

    state.togglePlayback(); // Should resume

    CHECK(state.getIsPlaying() == true);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PLAYBACK_RESUMED);

    PlaybackEvent *event = (PlaybackEvent *)lastEvent.eventData;
    CHECK(event->isPlaying == true);
  }

  TEST_CASE("State - Toggle Playback No Track") {
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.togglePlayback(); // No track playing

    CHECK(lastEvent.captured == false); // No event when no track
  }

  TEST_CASE("State - Stop Playback") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback of track 0
    resetTestState();
    state.addEventListener(testEventCallback);

    state.stopPlayback();

    CHECK(state.getIsPlaying() == false);
    CHECK(state.getPlayingTrackIndex() == -1);
    CHECK(state.getPlaybackPosition() == 0);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PLAYBACK_STOPPED);

    PlaybackEvent *event = (PlaybackEvent *)lastEvent.eventData;
    CHECK(event->trackIndex == 0); // Original track index
    CHECK(event->isPlaying == false);
  }

  TEST_CASE("State - Update Progress") {
    State state;
    resetTestState();
    state.setTrackDuration(180); // 3 minutes
    state.addEventListener(testEventCallback);

    state.updateProgress(60); // 1 minute

    CHECK(state.getPlaybackPosition() == 60);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_PROGRESS_UPDATED);

    ProgressEvent *event = (ProgressEvent *)lastEvent.eventData;
    CHECK(event->position == 60);
    CHECK(event->duration == 180);
    CHECK(abs(event->percentage - (60.0f / 180.0f)) <
          0.001f); // Floating point comparison
  }

  TEST_CASE("State - Update Progress Same Value") {
    State state;
    resetTestState();
    state.setTrackDuration(180);
    state.updateProgress(60);
    state.addEventListener(testEventCallback);

    state.updateProgress(60); // Same value again

    CHECK(lastEvent.captured == false); // No event for same value
  }

  TEST_CASE("State - Set Track Duration") {
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.setTrackDuration(240);

    CHECK(state.getTrackDuration() == 240);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_TRACK_DURATION_CHANGED);

    int *duration = (int *)lastEvent.eventData;
    CHECK(*duration == 240);
  }

  TEST_CASE("State - Track Ended Auto Advance") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback of track 0 ("First Song")
    resetTestState();
    state.addEventListener(testEventCallback);

    state.notifyTrackEnded(lookup);

    // Should emit a track-ended event first, then start the next track
    CHECK(eventCount >= 2);
    CHECK(state.getPlayingTrackIndex() == 1); // Advanced to next track
    CHECK(state.getIsPlaying() == true);
    CHECK(strcmp(state.getPlayingTrackName(), "Second Song") == 0);
  }

  TEST_CASE("State - Track Ended at Last Track") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup);        // Start playback of track 0
    state.notifyTrackEnded(lookup);    // Advance to track 1 (last track)
    resetTestState();
    state.addEventListener(testEventCallback);

    state.notifyTrackEnded(lookup); // No more tracks after this one

    CHECK(state.getPlayingTrackIndex() == -1); // Should stop
    CHECK(state.getIsPlaying() == false);
  }

  TEST_CASE("State - Get Next Track Id") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback of track 0 (id 1)

    uint32_t nextTrackId = 0;
    CHECK(state.getNextTrackId(lookup, nextTrackId) == true);
    CHECK(nextTrackId == 2);

    state.notifyTrackEnded(lookup); // Advance to the last track

    CHECK(state.getNextTrackId(lookup, nextTrackId) == false);
  }
}

TEST_SUITE("State UI and Animation Tests") {
  TEST_CASE("State - Go To Now Playing") {
    MusicLookup lookup; // goToNowPlaying() never dereferences it
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.goToNowPlaying(lookup);

    CHECK(state.getCurrentRoute().type == Route_t::NOW_PLAYING);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_ROUTE_CHANGED);

    RouteChangedEvent *event = (RouteChangedEvent *)lastEvent.eventData;
    CHECK(event->oldRouteType == Route_t::ROOT);
    CHECK(event->newRouteType == Route_t::NOW_PLAYING);
  }

  TEST_CASE("State - Go To Now Playing When Already There") {
    MusicLookup lookup;
    State state;
    state.goToNowPlaying(lookup);
    resetTestState();
    state.addEventListener(testEventCallback);

    state.goToNowPlaying(lookup); // Already there - should no-op

    CHECK(lastEvent.captured == false);
  }

  TEST_CASE("State - Set Animation State") {
    State state;
    resetTestState();
    state.addEventListener(testEventCallback);

    state.setAnimating(true, 12345);

    CHECK(state.getIsAnimating() == true);
    CHECK(lastEvent.captured == true);
    CHECK(lastEvent.eventType == EVENT_ANIMATION_STARTED);

    AnimationEvent *event = (AnimationEvent *)lastEvent.eventData;
    CHECK(event->animationId == 12345);
    CHECK(event->animationType == 1);
  }

  TEST_CASE("State - Set Rotation") {
    State state;

    state.setRotation(true);

    CHECK(state.getIsRotatedMode() == true);

    state.setRotation(false);

    CHECK(state.getIsRotatedMode() == false);
  }
}

TEST_SUITE("State Utility Tests") {
  TEST_CASE("State - Get Current Track Name") {
    State state;
    state.setElements(genericElements, genericElementCount);
    state.scrollDown();
    state.scrollDown();
    state.scrollDown(); // selectedIndex -> 3

    const char *name = state.getCurrentTrackName();

    CHECK(name != nullptr);
    CHECK(strcmp(name, "Item 4") == 0);
  }

  TEST_CASE("State - Get Current Track Name No Tracks") {
    State state;

    const char *name = state.getCurrentTrackName();

    CHECK(name == nullptr);
  }

  TEST_CASE("State - Get Playing Track Name") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup);
    state.goToSelected(lookup); // Start playback of track 0

    const char *name = state.getPlayingTrackName();

    CHECK(name != nullptr);
    CHECK(strcmp(name, "First Song") == 0);
  }

  TEST_CASE("State - Get Playing Track Name Not Playing") {
    setupCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());

    State state;
    navigateToTracks(state, lookup); // Reaches TRACKS but never starts playback

    const char *name = state.getPlayingTrackName();

    CHECK(name == nullptr);
  }

  TEST_CASE("State - Get Visible Track Indices") {
    State state;
    state.setElements(genericElements, genericElementCount);
    for (int i = 0; i < 7; i++) {
      state.scrollDown(); // Push topVisibleIndex forward
    }

    int start, end;
    state.getVisibleTrackIndices(start, end);

    CHECK(start >= 0);
    CHECK(end < genericElementCount);
    CHECK(end >= start);
    CHECK((end - start + 1) <= ELEMENTS_PER_SCREEN);
  }

  TEST_CASE("State - Get Relative Selected Index") {
    State state;
    state.setElements(genericElements, genericElementCount);
    for (int i = 0; i < 7; i++) {
      state.scrollDown();
    }

    int relative = state.getRelativeSelectedIndex();

    CHECK(relative >= 0);
    CHECK(relative < ELEMENTS_PER_SCREEN);
    CHECK(relative == (state.getSelectedIndex() - state.getTopVisibleIndex()));
  }

  TEST_CASE("State - Needs Scroll Update") {
    State state;
    state.setElements(genericElements, genericElementCount);

    bool needsUpdate1 = state.needsScrollUpdate(0, 0); // Same values
    CHECK(needsUpdate1 == false);

    bool needsUpdate2 = state.needsScrollUpdate(1, 0); // Different selected
    CHECK(needsUpdate2 == true);

    bool needsUpdate3 = state.needsScrollUpdate(0, 1); // Different top visible
    CHECK(needsUpdate3 == true);
  }
}

TEST_SUITE("State Navigation Tests") {
  // Regression test: selecting "Tracks" from the ROOT menu used to be an
  // unimplemented TODO that left the route unchanged (see goToSelected's
  // ROOT case) - selecting it did nothing.
  TEST_CASE("State - Root Tracks Menu Item Browses Full Catalog") {
    setupCatalog();
    State state;
    MusicLookup lookup;
    REQUIRE(lookup.init());

    state.loadCurrentRouteData(lookup); // ROOT menu, selectedIndex 0 ("Tracks")
    state.goToSelected(lookup);         // ROOT -> TRACKS (all tracks)

    CHECK(state.getCurrentRoute().type == Route_t::TRACKS);
    CHECK(state.getTotalElements() == 2);
    CHECK(std::strcmp(state.getVisibleElements()[0], "First Song") == 0);
    CHECK(std::strcmp(state.getVisibleElements()[1], "Second Song") == 0);

    // Selecting a track here plays it, and auto-advance to the next track
    // (resolveTrackId) must also work with no artist/album/genre scoping.
    state.goToSelected(lookup); // TRACKS -> NOW_PLAYING, plays "First Song"
    CHECK(state.getPlayingTrackIndex() == 0);
    CHECK(state.getIsPlaying() == true);

    state.notifyTrackEnded(lookup);
    CHECK(state.getPlayingTrackIndex() == 1);
    CHECK(std::strcmp(state.getPlayingTrackName(), "Second Song") == 0);
  }

  // Regression test: selecting a Genre used to route to ARTISTS, but the
  // genre->artists relation is never loaded (see MusicLookup::init()), so
  // the ARTISTS list came back unfiltered and any artist picked from it led
  // to that artist's albums regardless of genre - in effect, "select a
  // genre" showed albums from every genre. Genre now routes straight to
  // ALBUMS, which is genre-scoped.
  TEST_CASE("State - Genre Selection Scopes Albums To That Genre") {
    openpod_test::resetAll();
    openpod_test::registerMultiGenreTestCatalog();
    State state;
    MusicLookup lookup;
    REQUIRE(lookup.init());

    state.loadCurrentRouteData(lookup); // ROOT menu
    state.scrollDown();
    state.scrollDown();
    state.scrollDown();                 // selectedIndex 0 -> 3 ("Genres")
    state.goToSelected(lookup);         // ROOT -> GENRES (["Gipsy Jazz", "Hip Hop"])
    CHECK(state.getCurrentRoute().type == Route_t::GENRES);
    CHECK(state.getTotalElements() == 2);

    state.goToSelected(lookup); // GENRES -> ALBUMS, scoped to "Gipsy Jazz"

    CHECK(state.getCurrentRoute().type == Route_t::ALBUMS);
    CHECK(state.getTotalElements() == 1);
    CHECK(std::strcmp(state.getVisibleElements()[0], "Djangology") == 0);
  }
}
