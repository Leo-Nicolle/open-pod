#include "rendering/ILI9341_GFX.h"
#include "rendering/ILI9341_driver.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>

// Display instance
ILI9341_GFX display;
OpenPodUIEngine ui(&display);
void testDataBus() {
  // Test each bit individually
  uint16_t testPatterns[] = {
    0x0001, 0x0002, 0x0004, 0x0008,  // Test bits 0-3
    0x0010, 0x0020, 0x0040, 0x0080,  // Test bits 4-7  
    0x0100, 0x0200, 0x0400, 0x0800,  // Test bits 8-11
    0x1000, 0x2000, 0x4000, 0x8000   // Test bits 12-15
  };
  
  for(int i = 0; i < 16; i++) {
    display.fillScreen(testPatterns[i]);
    Serial.print("Bit "); Serial.print(i); Serial.println(" test");
    delay(1000);
  }
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
  ui.begin();
  display.setupScroll(0, SCREEN_WIDTH, 0);
  // testDataBus();
  // ui.trackList.renderAllTracks(&display, 0, BODY_Y, 120);
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