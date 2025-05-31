#include <Adafruit_GFX.h>
#include "ILI9341_GFX.h"

ILI9341_GFX display;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=== ILI9341 Display Test ===");  
  
  display.begin();
  display.setRotation(2);
  display.fillScreen(0x0000); // Black background
}

void loop() {
  // Once you find the right rotation, use that one
}