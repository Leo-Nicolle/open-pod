#include "ILI9341_GFX.h"
#include "openpod_ui.h"

ILI9341_GFX display;
OpenPodUI ui(&display);

void setup() {
  Serial.begin(115200);
  while(!Serial)
    delay(10); // Wait for serial to be ready
  display.begin();
  ui.begin();
  ui.renderAllTracks();
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