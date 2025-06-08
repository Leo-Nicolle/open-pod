#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"

// Create I2C3 instance: SDA=PB4, SCL=PA8
TwoWire Wire3(PB4, PA8);
Adafruit_MPR121 cap = Adafruit_MPR121();
ClickWheel wheel(&cap);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== ClickWheel Test with Teleplot ===");
    
    // Initialize I2C3
    Wire3.begin();
    Wire3.setClock(100000);
    
    // Initialize MPR121
    if (!cap.begin(0x5A, &Wire3)) {
        Serial.println("MPR121 not found!");
        while (1) delay(1000);
    }
    
    // Initialize ClickWheel
    if (!wheel.begin()) {
        Serial.println("ClickWheel initialization failed!");
        while (1) delay(1000);
    }
    
    Serial.println("ClickWheel initialized successfully!");
    Serial.println("Touch the wheel and center button to test...");
    Serial.println("Electrodes: 0=Center, 1=Right, 2=Top, 3=Left, 4=Bottom");
    Serial.println("IMPORTANT: Keep hands off wheel for 3 seconds for baseline...");
    Serial.println("Open Teleplot to visualize the data!");
    Serial.println("---------------------------------------------------");
    
    delay(3000); // Wait for baseline
    wheel.takeBaseline(); // Take fresh baseline
}

void loop() {
    // Update wheel readings
    wheel.update();
    
    // Get ClickWheel processed deltas (these should work better!)
    int16_t delta_right = wheel.getDelta(0);   // Right electrode
    int16_t delta_top = wheel.getDelta(1);     // Top electrode  
    int16_t delta_bottom = wheel.getDelta(2);  // Bottom electrode
    int16_t delta_left = wheel.getDelta(3);    // Left electrode
    
    // Also get raw data for comparison
    uint16_t right_raw = cap.filteredData(1);
    uint16_t top_raw = cap.filteredData(2);
    uint16_t left_raw = cap.filteredData(3);
    uint16_t bottom_raw = cap.filteredData(4);
    
    uint16_t right_baseline = wheel.getBaseline(0);
    uint16_t top_baseline = wheel.getBaseline(1);
    uint16_t bottom_baseline = wheel.getBaseline(2);
    uint16_t left_baseline = wheel.getBaseline(3);
    
    // ClickWheel processed data
    float angle = wheel.getAngle();
    bool wheel_touched = wheel.isWheelTouched();
    bool center_pressed = wheel.isCenterPressed();
    
    // Send NEW delta data to Teleplot (from ClickWheel processing)
    Serial.print(">delta_right:");
    Serial.println(delta_right);
    Serial.print(">delta_top:");
    Serial.println(delta_top);
    Serial.print(">delta_left:");
    Serial.println(delta_left);
    Serial.print(">delta_bottom:");
    Serial.println(delta_bottom);
    
    // Send raw values for comparison
    Serial.print(">raw_right:");
    Serial.println(right_raw);
    Serial.print(">raw_top:");
    Serial.println(top_raw);
    Serial.print(">raw_left:");
    Serial.println(left_raw);
    Serial.print(">raw_bottom:");
    Serial.println(bottom_raw);
    
    // Send baselines
    Serial.print(">baseline_right:");
    Serial.println(right_baseline);
    Serial.print(">baseline_top:");
    Serial.println(top_baseline);
    Serial.print(">baseline_left:");
    Serial.println(left_baseline);
    Serial.print(">baseline_bottom:");
    Serial.println(bottom_baseline);
    
    // Processed wheel data
    Serial.print(">wheel_angle:");
    Serial.println(wheel_touched ? angle : -1);
    Serial.print(">wheel_touched:");
    Serial.println(wheel_touched ? 360 : 0);
    Serial.print(">center_pressed:");
    Serial.println(center_pressed ? 360 : 0);
    
    // Handle center button events
    if (wheel.wasCenterJustPressed()) {
        Serial.println("🔘 CENTER BUTTON PRESSED!");
    }
    
    if (wheel.wasCenterJustReleased()) {
        Serial.println("⚪ CENTER BUTTON RELEASED");
    }
    
    // Handle wheel touch events
    static bool wasWheelTouched = false;
    if (wheel_touched && !wasWheelTouched) {
        Serial.println("👆 WHEEL TOUCH START");
    }
    
    if (!wheel_touched && wasWheelTouched) {
        Serial.println("👋 WHEEL TOUCH END");
    }
    wasWheelTouched = wheel_touched;
    
    // Show significant angle changes
    static float lastAngle = -1;
    if (wheel_touched && abs(angle - lastAngle) > 5.0) {
        Serial.print("🎯 Angle: ");
        Serial.print(angle, 1);
        Serial.print("° Deltas: [");
        Serial.print(delta_right); Serial.print(",");
        Serial.print(delta_top); Serial.print(",");
        Serial.print(delta_left); Serial.print(",");
        Serial.print(delta_bottom);
        Serial.println("]");
        lastAngle = angle;
    }
    
    // Debug output every 10 seconds
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 10000) {
        lastDebug = millis();
        Serial.println("\n--- Debug Info ---");
        wheel.printDebugInfo();
        Serial.println("------------------\n");
    }
    
    delay(50); // 20Hz update rate - good for Teleplot
}