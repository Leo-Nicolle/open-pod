#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "WheelTrace.hpp"
#include "player.h"
#include "rendering/ILI9341_GFX.h"
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

// Function declarations
void handleWheelInput();
void checkRecalibration();
void drawTraceVisualization();
bool shouldUpdateTelemetry();
void sendTelemetry();
void demoAnimations();

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("=== OpenPod with ClickWheel ===");

  // Initialize hardware
  player.setup();
  
  display.begin();
  delay(100);
  display.setupScroll(0, SCREEN_WIDTH, 0);

  Wire3.begin();
  Wire3.setClock(100000);

  if (!cap.begin(0x5A, &Wire3)) {
    Serial.println("❌ MPR121 not found!");
    while (1) delay(1000);
  }

  if (!wheel.begin()) {
    Serial.println("❌ ClickWheel initialization failed!");
    while (1) delay(1000);
  }

  Serial.println("✅ ClickWheel initialized!");
  Serial.println("Keep hands off wheel for baseline...");
  delay(3000);
  wheel.takeBaseline();

  // Apply your calibrated sensitivity values
  wheel.setSensitivities(0.896, 1.0, 0.950, 0.913);

  // Initialize UI
  ui.begin();
  
  // Optional demo animations
  demoAnimations();
  
  Serial.println("🎮 Ready! Controls:");
  Serial.println("- Scroll: Navigate");
  Serial.println("- Center press: Select");
  Serial.println("- Center hold 2s: Recalibrate");
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
  
  // Update UI
  ui.update();
  
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
  // Center button
  if (wheel.wasCenterJustPressed()) {
    Serial.println("🔘 Center pressed");
    ui.selectTrack();
  }

  // Scroll wheel
  int scrollClicks = wheel.consumeScrollClicks();
  if (scrollClicks != 0) {
    Serial.print("🔄 Scroll: ");
    Serial.println(scrollClicks < 0 ? "Down" : "Up");
    
    for (int i = 0; i < abs(scrollClicks); i++) {
      scrollClicks < 0 ? ui.scrollDown() : ui.scrollUp();
    }
  }

  // Touch events
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
      Serial.print("📊 Total: ");
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
      Serial.println("🔁 Recalibrating baseline...");
      wheel.takeBaseline();
      isHoldingCenter = false;
    }
  } else {
    isHoldingCenter = false;
  }
}

void drawTraceVisualization() {
  int angle = wheel.getWheelAngle();
  if (angle != -1) {
    trace.add(angle); // No conversion needed with cleaned ClickWheel!
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
}

void demoAnimations() {
  Serial.println("🎬 Demo: Transitioning to Now Playing...");
  ui.transitionToNowPlaying();
  while (ui.animManager.isActive()) {
    ui.update();
    delay(10);
  }

  Serial.println("🎬 Demo: Returning to Track List...");
  ui.transitionToTrackList();
  while (ui.animManager.isActive()) {
    ui.update();
    delay(10);
  }
}