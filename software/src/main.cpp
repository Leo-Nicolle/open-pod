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
  // display.fillRect(0, 0, 120, 120, 0x0000);
  // delay(500);

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
  // for (int y = 0; y < SCREEN_HEIGHT; y += CHUNK_HEIGHT)
  // for (int x = 0; x < SCREEN_WIDTH; x += W) {
  //   ui.trackList.renderAllTracks(&display, x, BODY_Y,
  //                                W);
  //   delay(500);
  // }
  // ui.trackList.renderAllTracks(&display, 0, BODY_Y, 20);
  // delay(500);

  // for (int x = 0; x < SCREEN_WIDTH; x+=CHUNK_HEIGHT) {
  // ui.trackList.renderAllTracks(&display, x, BODY_Y, CHUNK_HEIGHT);
  // }

  Serial.println("Now Playing rendered");
}

void loop() {}