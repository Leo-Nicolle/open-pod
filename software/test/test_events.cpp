#include <doctest.h>
#include "../src/state/event-target.h"
#include "../src/state/state.h"
#include "../src/storage/Music_lookup.h"
#include <cstring>

// Test event capture system
struct CapturedEvent {
  int eventType;
  void *eventData;
  EventTarget *source;
  bool captured;

  CapturedEvent()
      : eventType(0), eventData(nullptr), source(nullptr), captured(false) {}

  void reset() {
    eventType = 0;
    eventData = nullptr;
    source = nullptr;
    captured = false;
  }
};

CapturedEvent lastEvent;
int eventCount = 0;

// State builds its event payload structs on the stack and passes emitEvent()
// a pointer to them; that pointer is only valid for the duration of the
// (synchronous) listener call. Since these tests inspect the event data
// *after* the call that emitted it has already returned, the callback has to
// copy the payload out into storage that outlives the call, not just stash
// the (about to dangle) pointer.
namespace {
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
} // namespace

// Test event callback
void testEventCallback(int eventType, void *eventData, EventTarget *source) {
  lastEvent.eventType = eventType;
  if (eventData) {
    size_t sz = eventDataSize(eventType);
    if (sz > sizeof(lastEventDataStorage)) sz = sizeof(lastEventDataStorage);
    memcpy(lastEventDataStorage, eventData, sz);
    lastEvent.eventData = lastEventDataStorage;
  } else {
    lastEvent.eventData = nullptr;
  }
  lastEvent.source = source;
  lastEvent.captured = true;
  eventCount++;
}

// Additional callback for multiple listener tests
int secondCallbackCount = 0;
void secondTestCallback(int eventType, void *eventData, EventTarget *source) {
  secondCallbackCount++;
}

// Test fixture setup/teardown helpers
void resetTestState() {
  lastEvent.reset();
  eventCount = 0;
  secondCallbackCount = 0;
}

TEST_SUITE("EventTarget Tests") {
  TEST_CASE("EventTarget - Basic Construction") {
    EventTarget target;

    CHECK(target.getListenerCount() == 0);
  }

  TEST_CASE("EventTarget - Add Single Listener") {
    EventTarget target;
    resetTestState();

    bool result = target.addEventListener(testEventCallback);

    CHECK(result == true);
    CHECK(target.getListenerCount() == 1);
  }

  TEST_CASE("EventTarget - Add Multiple Listeners") {
    EventTarget target;
    resetTestState();

    bool result1 = target.addEventListener(testEventCallback);
    bool result2 = target.addEventListener(secondTestCallback);

    CHECK(result1 == true);
    CHECK(result2 == true);
    CHECK(target.getListenerCount() == 2);
  }

  TEST_CASE("EventTarget - Add Duplicate Listener") {
    EventTarget target;
    resetTestState();

    bool result1 = target.addEventListener(testEventCallback);
    bool result2 = target.addEventListener(testEventCallback); // Same callback

    CHECK(result1 == true);
    CHECK(result2 == false); // Should reject duplicate
    CHECK(target.getListenerCount() == 1);
  }

  TEST_CASE("EventTarget - Add Null Listener") {
    EventTarget target;

    bool result = target.addEventListener(nullptr);

    CHECK(result == false);
    CHECK(target.getListenerCount() == 0);
  }

  TEST_CASE("EventTarget - Remove Listener") {
    EventTarget target;
    resetTestState();

    target.addEventListener(testEventCallback);
    target.addEventListener(secondTestCallback);

    CHECK(target.getListenerCount() == 2);

    bool removed = target.removeEventListener(testEventCallback);

    CHECK(removed == true);
    CHECK(target.getListenerCount() == 1);
  }

  TEST_CASE("EventTarget - Remove Non-existent Listener") {
    EventTarget target;
    resetTestState();

    target.addEventListener(testEventCallback);

    bool removed = target.removeEventListener(secondTestCallback); // Not added

    CHECK(removed == false);
    CHECK(target.getListenerCount() == 1);
  }

  TEST_CASE("EventTarget - Clear All Listeners") {
    EventTarget target;
    resetTestState();

    target.addEventListener(testEventCallback);
    target.addEventListener(secondTestCallback);

    CHECK(target.getListenerCount() == 2);

    target.clearAllListeners();

    CHECK(target.getListenerCount() == 0);
  }

  TEST_CASE("EventTarget - Max Listeners Limit") {
    EventTarget target;
    resetTestState();

    // Add listeners up to the limit
    void (*callbacks[MAX_EVENT_LISTENERS + 2])() = {
        (void (*)())testEventCallback,
        (void (*)())secondTestCallback,
        (void (*)())testEventCallback, // This will be rejected as duplicate
        nullptr, // Fill remaining slots with unique addresses
        nullptr,
        nullptr};

    int successCount = 0;
    for (int i = 0; i < MAX_EVENT_LISTENERS + 1; i++) {
      if (target.addEventListener((EventCallback)(callbacks[i] + i))) {
        successCount++;
      }
    }

    CHECK(successCount == MAX_EVENT_LISTENERS);
    CHECK(target.getListenerCount() == MAX_EVENT_LISTENERS);
  }
}

// The tests below exercise State (which derives from EventTarget) rather
// than EventTarget directly. State's API has moved on a lot since these were
// written (setTracks/selectTrack/startPlayback(index) are all gone - see
// state.h), so they're adapted to the current setElements/scroll*/goTo*
// API instead of a literal port.

namespace {
const char *genericElements[] = {"Item 1", "Item 2", "Item 3", "Item 4",
                                  "Item 5", "Item 6", "Item 7", "Item 8",
                                  "Item 9", "Item 10"};
const int genericElementCount = 10;
} // namespace

TEST_CASE("Multiple Listeners Receive Events") {
  State state;
  resetTestState();
  state.addEventListener(testEventCallback);
  state.addEventListener(secondTestCallback);

  state.setVolume(50); // Any state-changing call will do; volume needs no
                        // MusicLookup or navigation setup.

  CHECK(lastEvent.captured == true); // First callback triggered
  CHECK(secondCallbackCount == 1);   // Second callback triggered
}

TEST_CASE("Event Data Integrity") {
  State state;
  resetTestState();
  state.setElements(genericElements, genericElementCount);
  state.addEventListener(testEventCallback);

  state.scrollDown(); // selectedIndex 0 -> 1

  CHECK(lastEvent.eventType == EVENT_SCROLL_CHANGED);
  CHECK(lastEvent.source == &state);

  ScrollChangedEvent *event = (ScrollChangedEvent *)lastEvent.eventData;
  CHECK(event->selectedIndex == 1);
  CHECK(event->oldSelectedIndex == 0);
}

TEST_CASE("Complex Navigation Sequence") {
  State state;
  MusicLookup lookup; // goToNowPlaying() doesn't touch the MusicLookup it's
                       // handed, so no fake filesystem/catalog is needed here.
  resetTestState();
  state.addEventListener(testEventCallback);

  // Sequence: load a list -> scroll -> page -> change volume -> animate ->
  // navigate to Now Playing. Each step should fire its own event.
  state.setElements(genericElements, genericElementCount);
  int eventCount1 = eventCount;

  state.scrollDown();
  int eventCount2 = eventCount;

  state.pageDown();
  int eventCount3 = eventCount;

  state.setVolume(state.getVolume() + 5);
  int eventCount4 = eventCount;

  state.setAnimating(true, 42);
  int eventCount5 = eventCount;

  state.goToNowPlaying(lookup);
  int eventCount6 = eventCount;

  CHECK(eventCount1 >= 1);          // At least the list-updated event
  CHECK(eventCount2 > eventCount1); // Scroll event added
  CHECK(eventCount3 > eventCount2); // Page event added
  CHECK(eventCount4 > eventCount3); // Volume changed
  CHECK(eventCount5 > eventCount4); // Animation started
  CHECK(eventCount6 > eventCount5); // Route changed (Now Playing)
}
