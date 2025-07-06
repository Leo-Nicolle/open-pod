#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "WheelTrace.hpp"
#include "player.h"
#include "rendering/ILI9341_GFX.h"
#include "state/state.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>
#include <Wire.h>

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

// Example track data - replace with your actual tracks
const char *tracks[] = {
    "Track 1 - Artist 1",    "Track 2 - Artist 2",   "Track 3 - Artist 3",
    "Track 4 - Artist 4",    "Track 5 - Artist 5",   "Track 6 - Artist 6",
    "Track 7 - Artist 7",    "Track 8 - Artist 8",   "Track 9 - Artist 9",
    "Track 10 - Artist 10",  "Track 11 - Artist 11", "Track 12 - Artist 12",
    "Track 13 - Artist 13",  "Track 14 - Artist 14", "Track 15 - Artist 15",
    "Track 16 - Artist 16",  "Track 17 - Artist 17", "Track 18 - Artist 18",
    "Track 19 - Artist 19",  "Track 20 - Artist 20", "Track 21 - Artist 21",
    "Track 22 - Artist 22",  "Track 23 - Artist 23", "Track 24 - Artist 24",
    "Track 25 - Artist 25",  "Track 26 - Artist 26", "Track 27 - Artist 27",
    "Track 28 - Artist 28",  "Track 29 - Artist 29", "Track 30 - Artist 30",
    "Track 31 - Artist 31",  "Track 32 - Artist 32", "Track 33 - Artist 33",
    "Track 34 - Artist 34",  "Track 35 - Artist 35", "Track 36 - Artist 36",
    "Track 37 - Artist 37",  "Track 38 - Artist 38", "Track 39 - Artist 39",
    "Track 40 - Artist 40",  "Track 41 - Artist 41", "Track 42 - Artist 42",
    "Track 43 - Artist 43",  "Track 44 - Artist 44", "Track 45 - Artist 45",
    "Track 46 - Artist 46",  "Track 47 - Artist 47", "Track 48 - Artist 48",
    "Track 49 - Artist 49",  "Track 50 - Artist 50", "Track 51 - Artist 51",
    "Track 52 - Artist 52",  "Track 53 - Artist 53", "Track 54 - Artist 54",
    "Track 55 - Artist 55",  "Track 56 - Artist 56", "Track 57 - Artist 57",
    "Track 58 - Artist 58",  "Track 59 - Artist 59", "Track 60 - Artist 60",
    "Track 61 - Artist 61",  "Track 62 - Artist 62", "Track 63 - Artist 63",
    "Track 64 - Artist 64",  "Track 65 - Artist 65", "Track 66 - Artist 66",
    "Track 67 - Artist 67",  "Track 68 - Artist 68", "Track 69 - Artist 69",
    "Track 70 - Artist 70",  "Track 71 - Artist 71", "Track 72 - Artist 72",
    "Track 73 - Artist 73",  "Track 74 - Artist 74", "Track 75 - Artist 75",
    "Track 76 - Artist 76",  "Track 77 - Artist 77", "Track 78 - Artist 78",
    "Track 79 - Artist 79",  "Track 80 - Artist 80", "Track 81 - Artist 81",
    "Track 82 - Artist 82",  "Track 83 - Artist 83", "Track 84 - Artist 84",
    "Track 85 - Artist 85",  "Track 86 - Artist 86", "Track 87 - Artist 87",
    "Track 88 - Artist 88",  "Track 89 - Artist 89", "Track 90 - Artist 90",
    "Track 91 - Artist 91",  "Track 92 - Artist 92", "Track 93 - Artist 93",
    "Track 94 - Artist 94",  "Track 95 - Artist 95", "Track 96 - Artist 96",
    "Track 97 - Artist 97",  "Track 98 - Artist 98", "Track 99 - Artist 99",
    "Track 100 - Artist 100"
  };
const int trackCount = sizeof(tracks) / sizeof(tracks[0]);

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

  display.begin();
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
  // delay(3000);
  wheel.takeBaseline();

  // Apply your calibrated sensitivity values
  wheel.setSensitivities(0.896, 1.0, 0.950, 0.913);

  // Initialize state with track data (you'll need to define your tracks array)
  extern const char *tracks[];
  extern const int trackCount;

  // Initialize UI with event system
  ui.begin(); // This calls hookToEvents() internally
  state.setElements(tracks, trackCount);

  // Optional demo animations
  demoAnimations();

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
    if (state.getCurrentShowing() == TRACK_LIST) {
      // In track list: select track (could start playback or go to now playing)
      ui.handleSelectInput();
    } else if (state.getCurrentShowing() == NOW_PLAYING) {
      // In now playing: toggle play/pause
      ui.handlePlayPauseInput();
    }
  }

  // Long press center button in now playing = go back to list
  static unsigned long centerPressStart = 0;
  static bool centerLongPressHandled = false;

  if (wheel.isCenterPressed() && state.getCurrentShowing() == NOW_PLAYING) {
    if (centerPressStart == 0) {
      centerPressStart = millis();
      centerLongPressHandled = false;
    } else if (!centerLongPressHandled && (millis() - centerPressStart > 500)) {
      // Long press: go back to track list
      Serial.println("🔙 Back to track list");
      ui.handleBackInput();
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
    Serial.println(scrollClicks );
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
          ui.handleScrollDownInput();
        } else {
          ui.handleScrollUpInput();
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
  Serial.print(">selected_track:");
  Serial.println(state.getSelectedTrackIndex());
  Serial.print(">playing_track:");
  Serial.println(state.getPlayingTrackIndex());
  Serial.print(">is_playing:");
  Serial.println(state.getIsPlaying() ? 100 : 0);
  Serial.print(">ui_state:");
  Serial.println(state.getCurrentShowing());
}

void demoAnimations() {
  Serial.println("🎬 Demo: Event-driven transitions...");

  // Demonstrate state-driven transitions
  state.setShowing(NOW_PLAYING); // This triggers transition
  while (state.getIsAnimating()) {
    ui.update();
    delay(10);
  }

  delay(1000);

  state.setShowing(TRACK_LIST); // This triggers return transition
  while (state.getIsAnimating()) {
    ui.update();
    delay(10);
  }

  Serial.println("✅ Demo complete - UI is now event-driven!");
}
