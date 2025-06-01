#include "ILI9341_GFX.h"
#include "openpod_ui.h"

ILI9341_GFX display;
OpenPodUI ui(&display);

void setup() {
  Serial.begin(115200);
  display.begin();
  ui.begin();
  ui.renderAllTracks();
  // for(int i = 0; i < 2; i++) {
  //   ui.scrollDown();
  //   ui.updateChangedTracks();
  //   delay(100); // Simulate user scrolling
  // }

  // Run performance test
  // UIBenchmark::runBenchmark(&display);
}

void loop() {
    for(int i = 0; i < 20; i++) {

    ui.scrollDown();
    ui.updateChangedTracks();
    delay(100); // Simulate user scrolling
  }
  for(int i = 0; i < 20; i++) {
    ui.scrollUp();
    ui.updateChangedTracks();
    delay(100); // Simulate user scrolling
  }
}