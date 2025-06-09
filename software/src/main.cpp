#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "WheelTrace.hpp"
#include "player.h"
#include "rendering/ILI9341_GFX.h"
#include "rendering/ILI9341_driver.h"
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
bool enableTeleplot = false; // Set to false to disable teleplot spam
unsigned long lastUIUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 50; // 20Hz UI updates

void sendTeleplotData() {
  if (!enableTeleplot)
    return;

  wheel.update();

  // Utilisation des noms symboliques pour plus de clarté
  int16_t delta_top = wheel.getDelta(ClickWheel::TOP);
  int16_t delta_right = wheel.getDelta(ClickWheel::RIGHT);
  int16_t delta_bottom = wheel.getDelta(ClickWheel::BOTTOM);
  int16_t delta_left = wheel.getDelta(ClickWheel::LEFT);

  bool wheel_touched = wheel.isWheelTouched();
  bool center_pressed = wheel.isCenterPressed();
  float scroll_degrees = wheel.getScrollDegrees();
  int scroll_direction = wheel.getScrollDirection();

  Serial.print(">delta_top:");
  Serial.println(delta_top);
  Serial.print(">delta_right:");
  Serial.println(delta_right);
  Serial.print(">delta_bottom:");
  Serial.println(delta_bottom);
  Serial.print(">delta_left:");
  Serial.println(delta_left);

  Serial.print(">wheel_touched:");
  Serial.println(wheel_touched ? 200 : 0);
  Serial.print(">scroll_degrees:");
  Serial.println(scroll_degrees);
  Serial.print(">scroll_direction:");
  Serial.println(scroll_direction * 100);
  Serial.print(">center_pressed:");
  Serial.println(center_pressed ? 300 : 0);
}

void handleClickWheelInput() {
  wheel.update();

  if (wheel.wasCenterJustPressed()) {
    Serial.println("🔘 CENTER BUTTON PRESSED!");
    // ui.selectTrack();
  }

  int scrollClicks = wheel.consumeScrollClicks();

  if (scrollClicks != 0) {
    Serial.print("🔄 Scroll: ");
    Serial.print(scrollClicks);
    Serial.print(" clicks (");
    Serial.print(scrollClicks * 30);
    Serial.print("° total) - ");
    Serial.println(scrollClicks > 0 ? "Clockwise (Down)"
                                    : "Counter-clockwise (Up)");

    for (int i = 0; i < abs(scrollClicks); i++) {
      //   scrollClicks > 0 ? ui.scrollDown() : ui.scrollUp();
    }
  }

  static bool wasWheelTouched = false;
  bool wheel_touched = wheel.isWheelTouched();

  if (wheel_touched && !wasWheelTouched) {
    Serial.println("👆 WHEEL TOUCH START");
    wheel.resetScroll();
  }

  if (!wheel_touched && wasWheelTouched) {
    Serial.println("👋 WHEEL TOUCH END");
    float finalScroll = wheel.consumeScrollDegrees();
    if (abs(finalScroll) > 10) {
      Serial.print("📊 Total scroll: ");
      Serial.print(finalScroll, 1);
      Serial.print("° (");
      Serial.print(finalScroll > 0 ? "Clockwise" : "Counter-clockwise");
      Serial.println(")");
    }
  }
  wasWheelTouched = wheel_touched;

  int angle = wheel.getWheelAngle();
  if (angle != -1) {
    trace.add(angle);
  }
}

unsigned long centerHoldStart = 0;
bool isHoldingCenter = false;

void checkRecalibrationTrigger() {
  if (wheel.isCenterPressed()) {
    if (!isHoldingCenter) {
      centerHoldStart = millis();
      isHoldingCenter = true;
    } else if (millis() - centerHoldStart > 2000) {
      Serial.println("🔁 Recalibrating ClickWheel baseline...");
      wheel.takeBaseline();
      isHoldingCenter = false; // Évite les boucles
    }
  } else {
    isHoldingCenter = false;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("=== OpenPod with ClickWheel starting... ===");

  // Initialize player
  player.setup();

  // Initialize display
  display.begin();
  delay(100);
  display.setupScroll(0, SCREEN_WIDTH, 0);

  // Initialize ClickWheel
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
  Serial.println("Keep hands off wheel for 3 seconds for baseline...");
  delay(3000);
  wheel.takeBaseline();

  // Initialize UI
  ui.begin();

  // Demo animations (optional - remove if you want)
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

  Serial.println("🎮 ClickWheel control active!");
  Serial.println("- Scroll wheel to navigate");
  Serial.println("- Press center to select/return");
  Serial.println("- Set enableTeleplot=false to reduce serial spam");
  Serial.println("===========================================");
  //   wheel.startSensitivityCalibration();
}

void loop() {
  unsigned long now = millis();
  if (wheel.isCalibrating()) {
    wheel.updateCalibration();
    return; // Don't do normal wheel processing during calibration
  }
  //   handleClickWheelInput();
  //   ui.update();
  int angle = wheel.getWheelAngle();
  if (angle != -1) {
    trace.add((360 -angle + 90)%360); // Convert to 0-360 range
  }

  display.fillScreen(0xFFFF); // Clear screen to white
  trace.draw(display, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 80);
//   wheel.debugRightQuadrant();
  //   checkRecalibrationTrigger(); // ← ici

  //   if (enableTeleplot && (now - lastUIUpdate) >= UI_UPDATE_INTERVAL) {
  //     sendTeleplotData();
  //     lastUIUpdate = now;
  //   }

  delay(20);
}
