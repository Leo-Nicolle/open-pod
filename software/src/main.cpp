#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "WheelTrace.hpp"
#include "rendering/ILI9341_GFX.h"
#include "sound/player.h"
#include "state/state.h"
#include "storage/Music_index.h"
#include "storage/Music_lookup.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>
#include <Wire.h>
MusicIndex musicIndex;
MusicLookup musicLookup;
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
unsigned long lastUIUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 50; // 20Hz

// Center button hold for recalibration
unsigned long centerHoldStart = 0;
bool isHoldingCenter = false;
const unsigned long RECALIB_HOLD_TIME = 2000; // 2 seconds

// Audio progress tracking
unsigned long lastProgressUpdate = 0;
const unsigned long PROGRESS_UPDATE_INTERVAL = 1000; // 1 second

// Function declarations
void handleWheelInput();
void checkRecalibration();
void drawTraceVisualization();
bool shouldUpdateTelemetry();
void sendTelemetry();
void demoAnimations();
void setupAudioCallbacks();
void updateAudioProgress();

// Audio system callbacks
void onTrackStarted(const char *trackName, int duration);
void onTrackEnded();
void onPlaybackStateChanged(bool isPlaying);
void onProgressChanged(int position);

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=== OpenPod with ClickWheel (Event-Driven) ===");

  // Initialize hardware
  player.setup();
  setupAudioCallbacks();
  musicLookup.init();
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
  Serial.println("HERE");
  // state.navigateToRoute(Route_t::ROOT);
  state.loadCurrentRouteData(musicLookup);
  state.scrollDown();
  state.goToSelected(musicLookup);
  while (state.getIsAnimating()) {
    delay(20);
    ui.update();
  }
  state.scrollDown();
  delay(20);
  state.goToSelected(musicLookup);
  // state.back(musicLookup);
  while (state.getIsAnimating()) {
    delay(20);
    ui.update();
  }
  state.goToSelected(musicLookup);
  while (state.getIsAnimating()) {
    delay(20);
    ui.update();
  }
  state.scrollDown();
  state.scrollDown();
  delay(20);

  state.goToSelected(musicLookup);
  while (state.getIsAnimating()) {
    delay(20);
    ui.update();
  }
  state.back(musicLookup);
  while (state.getIsAnimating()) {
    delay(20);
    ui.update();
  }
  // state.back(musicLookup);
  // while (state.getIsAnimating()) {
  //   delay(20);
  //   ui.update();
  // }
  // state.back(musicLookup);
  // while (state.getIsAnimating()) {
  //   delay(20);
  //   ui.update();
  // }
  // state.back(musicLookup);
  // while (state.getIsAnimating()) {
  //   delay(20);
  //   ui.update();
  // }
  // delay(1000);
  // state.goToSelected(musicLookup);

  // state.loadTracksByArtist(musicLookup, 0);

  // Optional demo animations
  // demoAnimations();

  Serial.println("🎮 Ready! Controls:");
  Serial.println("- Scroll: Navigate tracks");
  Serial.println("- Center press: Play/Pause or Select");
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
  checkRecalibration();

  // Update UI (handles animations and event processing)
  ui.update();

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
  // Center button press
  if (wheel.wasCenterJustPressed()) {
    Serial.println("🔘 Center pressed");
    return;
    if (state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING) {
      // In track list: select track (could start playback or go to now playing)
      state.goToSelected(musicLookup);
    } else if (state.getCurrentRoute().type ==
               Route_t::RouteType::NOW_PLAYING) {
      // In now playing: toggle play/pause
      state.togglePlayback();
    }
  }

  // Long press center button in now playing = go back to list
  static unsigned long centerPressStart = 0;
  static bool centerLongPressHandled = false;

  if (wheel.isCenterPressed() &&
      state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING) {
    if (centerPressStart == 0) {
      centerPressStart = millis();
      centerLongPressHandled = false;
    } else if (!centerLongPressHandled && (millis() - centerPressStart > 500)) {
      // Long press: go back to track list
      Serial.println("🔙 Back to track list");
      state.back(musicLookup);
      centerLongPressHandled = true;
    }
  } else {
    centerPressStart = 0;
    centerLongPressHandled = false;
  }

  // Scroll wheel navigation
  int scrollClicks = wheel.consumeScrollClicks();
  if (scrollClicks != 0) {
    Serial.print("🔄 Scroll: ");
    // Serial.println(scrollClicks < 0 ? "Down" : "Up");
    Serial.println(scrollClicks);
    if (abs(scrollClicks) > 1) {
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

void checkRecalibration() {
  if (wheel.isCenterPressed()) {
    if (!isHoldingCenter) {
      centerHoldStart = millis();
      isHoldingCenter = true;
    } else if (millis() - centerHoldStart > RECALIB_HOLD_TIME) {
      Serial.println("🔁 Recalibrating wheel baseline...");
      wheel.takeBaseline();
      isHoldingCenter = false;
    }
  } else {
    isHoldingCenter = false;
  }
}

void setupAudioCallbacks() {
  // Set up audio system callbacks to update state
  // These would be implemented in your PodPlayer class

  // Example callback setup (adjust based on your audio library):
  /*
  player.onTrackStarted([](const char* trackName, int duration) {
    onTrackStarted(trackName, duration);
  });

  player.onTrackEnded([]() {
    onTrackEnded();
  });

  player.onPlaybackStateChanged([](bool isPlaying) {
    onPlaybackStateChanged(isPlaying);
  });
  */
}

void updateAudioProgress() {
  // Update progress periodically if playing
  if (state.getIsPlaying()) {
    unsigned long now = millis();
    if (now - lastProgressUpdate >= PROGRESS_UPDATE_INTERVAL) {
      lastProgressUpdate = now;

      // Get current position from audio player
      // int currentPosition = player.getCurrentPosition(); // You'll need to
      // implement this state.updateProgress(currentPosition); // This triggers
      // EVENT_PROGRESS_UPDATED
    }
  }
}

// Audio system event handlers
void onTrackStarted(const char *trackName, int duration) {
  Serial.print("♪ Track started: ");
  Serial.println(trackName);

  state.setTrackDuration(duration);
  // Note: The actual playback start should be triggered by
  // state.startPlayback() This callback just confirms the audio system has
  // started
}

void onTrackEnded() {
  Serial.println("♪ Track ended");
  state.notifyTrackEnded(); // This will auto-advance or stop
}

void onPlaybackStateChanged(bool isPlaying) {
  Serial.println(isPlaying ? "♪ Playback started" : "⏸ Playback paused");
  // The state should already be updated via UI actions, but this confirms it
}

void onProgressChanged(int position) {
  // This could be called by audio system for more frequent updates
  state.updateProgress(position);
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
