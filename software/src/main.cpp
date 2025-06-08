#include "player.h"
#include "rendering/ILI9341_GFX.h"
#include "rendering/ILI9341_driver.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>
// Display instance

PodPlayer player;
ILI9341_GFX display;
OpenPodUIEngine ui(&display);
void testDataBus() {
  // Test each bit individually
  uint16_t testPatterns[] = {
      0x0001, 0x0002, 0x0004, 0x0008, // Test bits 0-3
      0x0010, 0x0020, 0x0040, 0x0080, // Test bits 4-7
      0x0100, 0x0200, 0x0400, 0x0800, // Test bits 8-11
      0x1000, 0x2000, 0x4000, 0x8000  // Test bits 12-15
  };
  char headerTitle[20];
  for (int i = 0; i < 16; i++) {
    Serial.print("Bit ");
    Serial.print(i);
    Serial.println(" test");
    display.fillScreen(testPatterns[i]);

    u_int16_t *buffer = g_buffers.getCurrentBuffer();
    for (int i = 0; i < SCREEN_WIDTH * HEADER_HEIGHT; i++) {
      buffer[i] = COLOR_PRIMARY;
    }
    fontRenderer.setBuffer(buffer, SCREEN_WIDTH, HEADER_HEIGHT);
    sprintf(headerTitle, "Bit %d Test", i);
    fontRenderer.renderText(headerTitle, 5, 10, IBMPlexSans16Bold,
                            COLOR_BACKGROUND, COLOR_PRIMARY);
    display.setWindow(0, 0, SCREEN_WIDTH - 1, HEADER_HEIGHT - 1);
    display.pushPixels(buffer, SCREEN_WIDTH * HEADER_HEIGHT);
    display.drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, COLOR_SECONDARY);
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    // Wait for serial to be ready
    delay(10);
  }
  Serial.println("OpenPod UI starting...");
  player.setup();
  // Initialize display
  display.begin();
  delay(100); // Allow display to initialize
  display.setupScroll(0, SCREEN_WIDTH, 0);
  // testDataBus();
  ui.begin();
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

void loop() { player.loop(); }