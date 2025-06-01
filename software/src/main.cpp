#include "ui/ILI9341_GFX.h"
#include "ui/openpod_ui.h"
#include <Arduino.h>

ILI9341_GFX display;
OpenPodUI ui(&display);

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  display.begin();
  ui.begin();
  ui.renderAllTracks();
  delay(500);
  ui.transitionToNowPlayingSimple();
  // for(int i=0; i< 200; i++){
  //   ui.update();
  //   delay(10);
  // }
}

void loop() {

  for (int i = 0; i < 5; i++) {
    ui.scrollDown();
    ui.updateChangedTracks();
    delay(100); // Simulate user scrolling
  }
  for (int i = 0; i < 5; i++) {
    ui.scrollUp();
    ui.updateChangedTracks();
    delay(100); // Simulate user scrolling
  }
}
