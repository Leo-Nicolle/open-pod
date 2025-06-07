#include "rendering/ILI9341_GFX.h"
#include "rendering/ILI9341_driver.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>

// Display instance
ILI9341_GFX display;
#define COLOR_BLUE 0x001F
OpenPodUIEngine ui(&display);
uint16_t lineBuffer[240];

void addBlueLine() {
  display.writeCommand(0x2A); // CASET
  display.writeData2x8(0);
  display.writeData2x8(239);

  display.writeCommand(0x2B); // PASET
  display.writeData2x8(SCREEN_HEIGHT - 1);
  display.writeData2x8(SCREEN_HEIGHT - 1);

  display.writeCommand(0x2C); // RAMWR
  for (int i = 0; i < 240; i++) {
    lineBuffer[i] = COLOR_BLUE;
  }
  display.pushPixels(lineBuffer, 240);
}

void setup() {
  Serial.begin(115200);
  // while(!Serial) {
  // Wait for serial to be ready
  // delay(10);
  // }
  Serial.println("OpenPod UI starting...");
  // Initialize display
  display.begin();
  delay(100); // Allow display to initialize
  display.fillRect(0, 0, 120, 120, 0x0000);
  delay(500);

  // Initialize UI
  ui.begin();
  display.setupScroll(0, SCREEN_WIDTH, 0);

  ui.trackList.renderAllTracks(&display, 0, BODY_Y, 120);
  ui.transitionToNowPlaying();
  while (ui.animManager.isActive()) {
    ui.update();
    delay(10); // Small delay to allow animations to progress
  }
  ui.transitionToTrackList();
  while (ui.animManager.isActive()) {
    ui.update();
    delay(10); // Small delay to allow animations to progress
  }
  // int x = 30;
  // for (int x = 0; x < SCREEN_WIDTH; x += CHUNK_HEIGHT) {
  //   ui.trackList.renderAllTracks(&display, SCREEN_WIDTH - x, BODY_Y,
  //                                CHUNK_HEIGHT);
  //   delay(500);
  // }
  // ui.trackList.renderAllTracks(&display, 0, BODY_Y, 20);
  // delay(500);

  // for (int x = 0; x < SCREEN_WIDTH; x+=CHUNK_HEIGHT) {
  // ui.trackList.renderAllTracks(&display, x, BODY_Y, CHUNK_HEIGHT);
  // }

  Serial.println("Now Playing rendered");
  // for(int i = 0; i < 200; i++) {
  //     ui.update();
  // }
  // ui.transitionToTrackList();
  // for(int i = 0; i < 100; i++) {
  //     ui.update();
  //     delay(10); // Small delay to allow animations to progress
  // }

  // int progress= 0;
  // ui.nowPlaying.setProgress(progress);
  // ui.renderNowPlaying();
  // ui.update();

  // for(int i = 0; i < 500; i++) {
  //   progress++;
  //   ui.nowPlaying.setProgress(progress);
  //   ui.nowPlaying.updateNowPlaying(&display);
  //   delay(1000);
  // }
  // Serial.println("OpenPod UI initialized");
}

void loop() {
  // Update animations
  // ui.update();

  // ui.scrollDown();
  // for(int i = 0; i < 20; i++) {
  //     ui.update();
  // }

  // // Handle input (example with simple button reading)
  // static unsigned long lastInput = 0;
  // if (millis() - lastInput > 200) { // Debounce

  //     // Example button handling
  //     if (digitalRead(UP_BUTTON) == LOW) {
  //         ui.scrollUp();
  //         lastInput = millis();
  //     }

  //     if (digitalRead(DOWN_BUTTON) == LOW) {
  //         ui.scrollDown();
  //         lastInput = millis();
  //     }

  //     if (digitalRead(SELECT_BUTTON) == LOW) {
  //         ui.selectTrack();
  //         lastInput = millis();
  //     }

  //     if (digitalRead(BACK_BUTTON) == LOW) {
  //         ui.returnToList();
  //         lastInput = millis();
  //     }

  //     if (digitalRead(PAGE_UP_BUTTON) == LOW) {
  //         ui.pageUp();
  //         lastInput = millis();
  //     }

  //     if (digitalRead(PAGE_DOWN_BUTTON) == LOW) {
  //         ui.pageDown();
  //         lastInput = millis();
  //     }
  // }

  // Performance monitoring (optional)
  // static unsigned long lastPerf = 0;
  // if (millis() - lastPerf > 5000) {
  //     ui.measurePerformance();
  //     lastPerf = millis();
  // }

  // delay(1); // Small delay to prevent overwhelming the system
}