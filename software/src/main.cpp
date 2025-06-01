#include "openpod_ui.h"

ILI9341_GFX display;
OpenPodUI ui(&display);

void setup() {
  display.begin();
  ui.begin();


  delay(1000); // Allow display to initialize
  ui.scrollUp();
  for(int i = 0; i < 10; i++) {
    delay(100); // Allow scrolling to complete
    ui.scrollDown();
  }

  delay(500); // Allow scrolling to complete
  ui.scrollDown();
  delay(500); // Allow scrolling to complete
  ui.selectTrack(); // Simulate selecting the first track
  delay(2500); // Allow selection to complete
  ui.returnToList();
}

void loop() {

}