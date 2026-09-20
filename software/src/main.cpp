#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "WheelTrace.hpp"
#include "rendering/ILI9341_GFX.h"
#include "sound/player.h"
#include "state/state.h"
#include "storage/AlbumArt.h"
#include "storage/Music_index.h"
#include "storage/Music_lookup.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>
#include <Wire.h>
MusicIndex musicIndex;
MusicLookup musicLookup;
AlbumArt albumArt;
// Display and UI instances
ILI9341_GFX display;
PodPlayer player;
OpenPodUIEngine ui(&display);

// ClickWheel instances
TwoWire Wire3(PB4, PA8);
Adafruit_MPR121 cap = Adafruit_MPR121();
ClickWheel wheel(&cap);
WheelTrace trace;

// Control settings
bool enableTeleplot = false;
bool enableTrace = false;

// Tracks whether a route-transition animation is currently playing, kept in
// sync via EVENT_ANIMATION_STARTED/FINISHED (see onNavigationStateEvent
// below) rather than polled from state.getIsAnimating() each tick.
static bool navigationBusy = false;

void onNavigationStateEvent(int eventType, void *eventData,
                            EventTarget *source) {
  if (eventType == EVENT_ANIMATION_STARTED) {
    navigationBusy = true;
  } else if (eventType == EVENT_ANIMATION_FINISHED) {
    navigationBusy = false;
  }
}
unsigned long lastUIUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 50; // 20Hz

// Center button two-stage hold: short hold jumps to Now Playing, longer
// hold (unchanged threshold) recalibrates the wheel. Both fire once per
// physical hold.
unsigned long centerHoldStart = 0;
bool centerHoldActive = false;
bool jumpToNowPlayingFired = false;
bool recalibFired = false;
const unsigned long NOWPLAYING_JUMP_HOLD_TIME = 800; // 0.8 seconds
const unsigned long RECALIB_HOLD_TIME = 2000;        // 2 seconds

// Now Playing seek-mode: active while the countdown below hasn't elapsed.
unsigned long seekModeLastInteraction = 0;
const unsigned long SEEK_MODE_DELAY = 3000; // 3 seconds
const int SEEK_SECONDS_PER_CLICK = 3;

// Scrolling while seeking only previews a target position (moves the
// displayed time/progress bar); the actual file seek is deferred until
// scrolling pauses, so a long scrub doesn't re-seek the file on every tick.
bool seekPending = false;
int pendingSeekTarget = 0; // seconds, valid only while seekPending
unsigned long lastSeekScrollTime = 0;
const unsigned long SEEK_COMMIT_DELAY = 400; // ms of no scrolling

// Audio progress tracking
unsigned long lastProgressUpdate = 0;
const unsigned long PROGRESS_UPDATE_INTERVAL = 1000; // 1 second

// Function declarations
void handleWheelInput();
void checkCenterHold();
void checkSeekModeTimeout();
void commitPendingSeek();
void drawTraceVisualization();
bool shouldUpdateTelemetry();
void sendTelemetry();
void demoAnimations();
void updateAudioProgress();

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=== OpenPod with ClickWheel (Event-Driven) ===");

  state.addEventListener(onNavigationStateEvent);

  // Initialize hardware
  player.setup();
  musicLookup.init();
  albumArt.init(musicLookup.getSD());
  display.begin();
  Serial.println("✅ Display initialized!");
  delay(100);
  display.setupScroll(0, SCREEN_WIDTH, 0);

  Wire3.begin();
  Wire3.setClock(100000);

  if (!cap.begin(0x5A, &Wire3)) {
    Serial.println("❌ MPR121 not found!");
    while (1)
      delay(1000);
  }

  if (!wheel.begin()) {
    Serial.println("❌ ClickWheel initialization failed!");
    while (1)
      delay(1000);
  }

  Serial.println("✅ ClickWheel initialized!");
  Serial.println("Keep hands off wheel for baseline...");
  wheel.takeBaseline();
  wheel.setSensitivities(0.896, 1.0, 0.950, 0.913);
  // --- MusicIndex initialization and artist/track search ---
  if (!musicIndex.init(musicLookup.getLastPSRAMAddress())) {
    Serial.println("❌ Failed to initialize MusicIndex!");
  }
  delay(100);
  ui.begin();
  state.loadCurrentRouteData(musicLookup);

  Serial.println("🎮 Ready! Controls:");
  Serial.println("- Scroll: Navigate list / adjust volume / seek");
  Serial.println("- Center press: Select / enter Now Playing seek-mode");
  Serial.println("- Bottom press: Play/pause (Now Playing)");
  Serial.println("- Top press: Back / exit seek-mode");
  Serial.println("- Center hold 0.8s: Jump to Now Playing");
  Serial.println("- Center hold 2s: Recalibrate wheel");
  Serial.println("=====================================");
}

void loop() {
  // Handle calibration mode
  if (wheel.isCalibrating()) {
    wheel.updateCalibration();
    return;
  }

  // Update wheel state
  wheel.update();

  // Handle input
  handleWheelInput();
  checkCenterHold();
  checkSeekModeTimeout();

  // Update UI (handles animations and event processing)
  ui.update();

  // Feed audio data to the VS1053 (this is what actually makes sound)
  player.loop();

  // Update audio progress periodically
  updateAudioProgress();

  // Optional trace visualization
  if (enableTrace) {
    drawTraceVisualization();
  }

  // Optional telemetry
  if (enableTeleplot && shouldUpdateTelemetry()) {
    sendTelemetry();
  }

  delay(20);
}

void handleWheelInput() {
  // A route change (goToSelected/back) kicks off an 800ms slide animation
  // that owns shared state (OpenPodUIEngine::lastDrawnOffset) for its whole
  // duration. Starting a second one before the first finishes runs two
  // animations against that same shared state at once and corrupts the
  // display buffer / can hang the MCU - so drop navigation input while a
  // transition is still playing (tracked via EVENT_ANIMATION_STARTED/
  // FINISHED, see onNavigationStateEvent), rather than queuing/stacking it.

  bool onNowPlaying =
      state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING;

  // Center press: enter menu / navigate forward, or enter seek-mode on Now
  // Playing (play/pause moved to the bottom button, see below).
  if (wheel.wasCenterJustPressed()) {
    Serial.println("🔘 Center pressed");
    if (onNowPlaying) {
      if (!state.getSeekModeActive()) {
        state.enterSeekMode();
        seekModeLastInteraction = millis();
        seekPending = false; // start each seek session from the real position
      }
    } else if (!navigationBusy) {
      state.goToSelected(musicLookup);
    }
  }

  // Bottom press: play/pause on Now Playing
  if (wheel.wasBottomJustPressed()) {
    Serial.println("⬇️ Bottom pressed");
    if (onNowPlaying) {
      state.togglePlayback();
    }
  }

  // Top press: exit seek-mode if active, otherwise go back one level
  if (wheel.wasTopJustPressed()) {
    Serial.println("⬆️ Top pressed - back");
    if (onNowPlaying && state.getSeekModeActive()) {
      commitPendingSeek(); // don't lose an in-flight scrub on exit
      state.exitSeekMode();
    } else if (!navigationBusy) {
      state.back(musicLookup);
    }
  }

  // Scroll wheel: seek (seek-mode) / volume (Now Playing) / list navigation
  int scrollClicks = wheel.consumeScrollClicks();
  if (scrollClicks != 0) {
    Serial.print("🔄 Scroll: ");
    Serial.println(scrollClicks);
    if (onNowPlaying && state.getSeekModeActive()) {
      // Scroll direction is reversed vs. list navigation: scrolling "up"
      // seeks backward, "down" seeks forward.
      int base = seekPending ? pendingSeekTarget
                             : (int)player.audioPlayer.getDecodeTime();
      int duration = (int)player.audioPlayer.getDurationSeconds();
      pendingSeekTarget = constrain(
          base - scrollClicks * SEEK_SECONDS_PER_CLICK, 0, duration);
      seekPending = true;
      state.updateProgress(pendingSeekTarget); // preview only, no file seek yet
      seekModeLastInteraction = millis();
      lastSeekScrollTime = millis();
    } else if (onNowPlaying) {
      // Reversed vs. list navigation, same as seeking above.
      for (int i = 0; i < abs(scrollClicks); i++) {
        if (scrollClicks < 0) {
          state.increaseVolume();
        } else {
          state.decreaseVolume();
        }
      }
    } else if (abs(scrollClicks) > 1) {
      if (scrollClicks < 0) {
        state.pageDown(); // Fast scroll down
      } else {
        state.pageUp(); // Fast scroll up
      }
    } else {

      // Handle multiple clicks at once for smooth scrolling
      for (int i = 0; i < abs(scrollClicks); i++) {
        if (scrollClicks < 0) {
          state.scrollDown();
        } else {
          state.scrollUp();
        }
      }
    }
  }

  // Touch events for visual feedback
  static bool wasWheelTouched = false;
  bool wheelTouched = wheel.isWheelTouched();

  if (wheelTouched && !wasWheelTouched) {
    Serial.println("👆 Wheel touch start");
    wheel.resetScroll();
  }

  if (!wheelTouched && wasWheelTouched) {
    Serial.println("👋 Wheel touch end");
    float totalScroll = wheel.consumeScrollDegrees();
    if (abs(totalScroll) > 10) {
      Serial.print("📊 Total scroll: ");
      Serial.print(totalScroll, 1);
      Serial.println("°");
    }
  }

  wasWheelTouched = wheelTouched;
}

void checkCenterHold() {
  if (wheel.isCenterPressed()) {
    if (!centerHoldActive) {
      centerHoldActive = true;
      centerHoldStart = millis();
      jumpToNowPlayingFired = false;
      recalibFired = false;
    }
    unsigned long held = millis() - centerHoldStart;
    if (!jumpToNowPlayingFired && held >= NOWPLAYING_JUMP_HOLD_TIME) {
      jumpToNowPlayingFired = true;
      if (!navigationBusy) {
        Serial.println("⏭️ Jumping to Now Playing");
        state.goToNowPlaying(musicLookup);
      }
    }
    if (!recalibFired && held >= RECALIB_HOLD_TIME) {
      recalibFired = true;
      Serial.println("🔁 Recalibrating wheel baseline...");
      wheel.takeBaseline();
    }
  } else {
    centerHoldActive = false;
  }
}

void commitPendingSeek() {
  if (seekPending) {
    player.audioPlayer.seekToSeconds((uint32_t)pendingSeekTarget);
    seekPending = false;
  }
}

void checkSeekModeTimeout() {
  if (!state.getSeekModeActive()) {
    return;
  }
  // Commit the actual file seek once scrolling has paused, rather than on
  // every scroll tick, so a long scrub doesn't repeatedly re-seek the file.
  if (seekPending && millis() - lastSeekScrollTime >= SEEK_COMMIT_DELAY) {
    commitPendingSeek();
  }
  if (millis() - seekModeLastInteraction > SEEK_MODE_DELAY) {
    commitPendingSeek(); // don't lose an in-flight scrub on idle-exit
    state.exitSeekMode();
  }
}

void updateAudioProgress() {
  // Update progress periodically if playing. Skip while seek-mode is
  // active: the scroll handler already previews the pending target
  // position (bar + timestamp), and this periodic poll would otherwise
  // fight it - overwriting the preview with the real, not-yet-committed
  // decode time every second and making the bar jump back and forth.
  if (state.getIsPlaying() && !state.getSeekModeActive()) {
    unsigned long now = millis();
    if (now - lastProgressUpdate >= PROGRESS_UPDATE_INTERVAL) {
      lastProgressUpdate = now;

      int currentPosition = player.audioPlayer.getDecodeTime();
      state.updateProgress(currentPosition); // This triggers
                                              // EVENT_PROGRESS_UPDATED
    }
  }
}

void drawTraceVisualization() {
  int angle = wheel.getWheelAngle();
  if (angle != -1) {
    trace.add(angle);
  }

  display.fillScreen(0x0000); // Black background
  trace.draw(display, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 80);
}

bool shouldUpdateTelemetry() {
  unsigned long now = millis();
  if (now - lastUIUpdate >= UI_UPDATE_INTERVAL) {
    lastUIUpdate = now;
    return true;
  }
  return false;
}

void sendTelemetry() {
  // Send telemetry data for debugging
  Serial.print(">delta_top:");
  Serial.println(wheel.getDelta(ClickWheel::TOP));
  Serial.print(">delta_right:");
  Serial.println(wheel.getDelta(ClickWheel::RIGHT));
  Serial.print(">delta_bottom:");
  Serial.println(wheel.getDelta(ClickWheel::BOTTOM));
  Serial.print(">delta_left:");
  Serial.println(wheel.getDelta(ClickWheel::LEFT));

  Serial.print(">wheel_touched:");
  Serial.println(wheel.isWheelTouched() ? 200 : 0);
  Serial.print(">center_pressed:");
  Serial.println(wheel.isCenterPressed() ? 300 : 0);
  Serial.print(">scroll_degrees:");
  Serial.println(wheel.getScrollDegrees());

  // Add state telemetry
  // Serial.print(">selected_track:");
  // Serial.println(state.getSelectedIndex());
  Serial.print(">playing_track:");
  Serial.println(state.getPlayingTrackIndex());
  Serial.print(">is_playing:");
  Serial.println(state.getIsPlaying() ? 100 : 0);
  Serial.print(">ui_state:");
  // Serial.println(state.getCurrentShowing());
}

void demoAnimations() {
  Serial.println("🎬 Demo: Event-driven transitions...");

  Serial.println("✅ Demo complete - UI is now event-driven!");
}
