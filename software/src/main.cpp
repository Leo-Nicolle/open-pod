#include <Arduino.h>
#include "rendering/ILI9341_GFX.h"
#include "rendering/ILI9341_driver.h"
#include "ui/ui_engine.hpp"

// Display instance
ILI9341_GFX display;
OpenPodUIEngine ui(&display);

void setup() {
    Serial.begin(115200);
    // while(!Serial) {
      // Wait for serial to be ready
      // delay(10);
    // }
    Serial.println("OpenPod UI starting...");
    // Initialize display
    display.begin();
    display.setRotation(0);
    delay(100); // Allow display to initialize
    display.fillRect(0,0,120,120, 0x0000); // Clear the display with black color);
    delay(500); // Allow display to initialize
    
    // Initialize UI
    ui.begin();
    Serial.println("OpenPod UI initialized");
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