#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"

class ClickWheel {
private:
    Adafruit_MPR121* mpr121;
    
    // Electrode assignments
    static const uint8_t ELECTRODE_COUNT = 4;
    static const uint8_t CENTER_ELECTRODE = 0;
    uint8_t wheelElectrodes[ELECTRODE_COUNT] = {1, 2, 3, 4}; // Right, Top, Left, Bottom
    
    // Angle positions for each electrode (degrees)
    float electrodeAngles[ELECTRODE_COUNT] = {90.0, 0.0, 270.0, 180.0}; // Right, Top, Left, Bottom
    
    // Baseline tracking (like the GitHub example)
    uint16_t wheelBaseline[ELECTRODE_COUNT];
    bool baselineTaken = false;
    
    // Touch state tracking
    uint16_t lastTouched = 0;
    uint16_t currentTouched = 0;
    bool centerPressed = false;
    bool lastCenterPressed = false;
    
    // Touch values for interpolation (deltas from baseline)
    int16_t touchDeltas[ELECTRODE_COUNT];
    bool hasValidTouch = false;
    float currentAngle = 0.0;
    
    // Filtering and smoothing
    static const int FILTER_SIZE = 3;
    float angleHistory[FILTER_SIZE];
    int historyIndex = 0;
    bool historyFilled = false;
    
    // Sensitivity settings
    int16_t touchThreshold = 3; // Much lower threshold like the GitHub example

public:
    ClickWheel(Adafruit_MPR121* mpr) : mpr121(mpr) {
        // Initialize angle history
        for(int i = 0; i < FILTER_SIZE; i++) {
            angleHistory[i] = 0.0;
        }
    }
    
    // Initialize the click wheel
    bool begin() {
        if(!mpr121) return false;
        
        // Take initial baseline
        takeBaseline();
        
        return true;
    }
    
    // Take baseline measurement (call when no touch is happening)
    void takeBaseline() {
        for(int i = 0; i < ELECTRODE_COUNT; i++) {
            wheelBaseline[i] = mpr121->filteredData(wheelElectrodes[i]);
        }
        baselineTaken = true;
        Serial.println("ClickWheel baseline taken");
    }
    
    // Update touch readings - call this in your main loop
    void update() {
        if(!baselineTaken) {
            takeBaseline();
            return;
        }
        
        lastTouched = currentTouched;
        lastCenterPressed = centerPressed;
        
        // Center button uses standard touch detection
        currentTouched = mpr121->touched();
        centerPressed = (currentTouched & (1 << CENTER_ELECTRODE)) != 0;
        
        // Calculate deltas for wheel electrodes (like GitHub example)
        bool anyTouched = false;
        int16_t maxDelta = 0;
        
        for(int i = 0; i < ELECTRODE_COUNT; i++) {
            uint16_t current = mpr121->filteredData(wheelElectrodes[i]);
            touchDeltas[i] = wheelBaseline[i] - current; // Capacitive touch = value goes DOWN
            
            if(touchDeltas[i] < 0) touchDeltas[i] = 0; // No negative deltas
            
            if(touchDeltas[i] > touchThreshold) {
                anyTouched = true;
                if(touchDeltas[i] > maxDelta) maxDelta = touchDeltas[i];
            }
        }
        
        hasValidTouch = anyTouched;
        
        if(hasValidTouch) {
            calculateAngle();
        }
    }
    
    // Calculate interpolated angle based on touch deltas (inspired by GitHub code)
    void calculateAngle() {
        // Find the electrode with maximum delta
        int16_t maxVal = 0;
        int maxIdx = -1;
        
        for(int i = 0; i < ELECTRODE_COUNT; i++) {
            if(touchDeltas[i] > maxVal) {
                maxVal = touchDeltas[i];
                maxIdx = i;
            }
        }
        
        if(maxIdx == -1 || maxVal < touchThreshold) {
            hasValidTouch = false;
            return;
        }
        
        // Get neighboring values (with wraparound)
        int v1Idx = (maxIdx - 1 + ELECTRODE_COUNT) % ELECTRODE_COUNT;
        int v3Idx = (maxIdx + 1) % ELECTRODE_COUNT;
        
        int16_t v1 = touchDeltas[v1Idx]; // Left neighbor
        int16_t v2 = touchDeltas[maxIdx]; // Center (max)
        int16_t v3 = touchDeltas[v3Idx]; // Right neighbor
        
        float sum = v1 + v2 + v3;
        if(sum == 0) {
            currentAngle = electrodeAngles[maxIdx];
            return;
        }
        
        // Calculate offset using proportional method from GitHub
        float offset = 0;
        if(v1 > v3) {
            offset = -(float)v1 / sum; // Move toward left neighbor
        } else if(v3 > v1) {
            offset = (float)v3 / sum;  // Move toward right neighbor
        }
        
        // Calculate interpolated position
        float offsetPad = (float)maxIdx + offset;
        
        // Handle wraparound
        if(offsetPad < 0) offsetPad += ELECTRODE_COUNT;
        if(offsetPad >= ELECTRODE_COUNT) offsetPad -= ELECTRODE_COUNT;
        
        // Convert to degrees
        float newAngle = (offsetPad / ELECTRODE_COUNT) * 360.0;
        
        // Apply the angle mapping based on our electrode positions
        // We need to map our electrode array indices to actual angles
        newAngle = electrodeAngles[maxIdx];
        
        // Apply interpolation offset
        float angleDiff = 360.0 / ELECTRODE_COUNT; // 90 degrees between electrodes
        if(v1 > v3) {
            // Move toward previous electrode
            float prevAngle = electrodeAngles[v1Idx];
            float diff = newAngle - prevAngle;
            if(diff > 180) diff -= 360;
            if(diff < -180) diff += 360;
            newAngle = newAngle - (abs(offset) * abs(diff));
        } else if(v3 > v1) {
            // Move toward next electrode
            float nextAngle = electrodeAngles[v3Idx];
            float diff = nextAngle - newAngle;
            if(diff > 180) diff -= 360;
            if(diff < -180) diff += 360;
            newAngle = newAngle + (abs(offset) * abs(diff));
        }
        
        // Normalize to 0-360
        while(newAngle < 0) newAngle += 360;
        while(newAngle >= 360) newAngle -= 360;
        
        // Apply smoothing
        smoothAngle(newAngle);
    }
    
    // Smooth the angle using a moving average
    void smoothAngle(float newAngle) {
        // Handle angle wraparound in smoothing
        if(historyFilled) {
            float lastAngle = angleHistory[(historyIndex - 1 + FILTER_SIZE) % FILTER_SIZE];
            float diff = newAngle - lastAngle;
            
            // Detect wraparound and adjust
            if(diff > 180.0) {
                newAngle -= 360.0;
            } else if(diff < -180.0) {
                newAngle += 360.0;
            }
        }
        
        angleHistory[historyIndex] = newAngle;
        historyIndex = (historyIndex + 1) % FILTER_SIZE;
        
        if(historyIndex == 0) historyFilled = true;
        
        // Calculate smoothed angle
        float sum = 0.0;
        int count = historyFilled ? FILTER_SIZE : historyIndex;
        
        for(int i = 0; i < count; i++) {
            sum += angleHistory[i];
        }
        
        currentAngle = sum / count;
        
        // Normalize final result
        while(currentAngle >= 360.0) currentAngle -= 360.0;
        while(currentAngle < 0) currentAngle += 360.0;
    }
    
    // Public interface methods
    bool isWheelTouched() const { return hasValidTouch; }
    bool isCenterPressed() const { return centerPressed; }
    bool wasCenterJustPressed() const { return centerPressed && !lastCenterPressed; }
    bool wasCenterJustReleased() const { return !centerPressed && lastCenterPressed; }
    
    // Get current angle (0-360 degrees)
    float getAngle() const { return hasValidTouch ? currentAngle : -1.0; }
    
    // Get angle as integer (0-360)
    int getAngleInt() const { return hasValidTouch ? (int)round(currentAngle) : -1; }
    
    // Get raw delta values for debugging
    int16_t getDelta(int electrode) const {
        if(electrode < 0 || electrode >= ELECTRODE_COUNT) return 0;
        return touchDeltas[electrode];
    }
    
    // Get baseline values for debugging
    uint16_t getBaseline(int electrode) const {
        if(electrode < 0 || electrode >= ELECTRODE_COUNT) return 0;
        return wheelBaseline[electrode];
    }
    
    // Configuration methods
    void setThreshold(int16_t threshold) {
        touchThreshold = threshold;
    }
    
    // Debug information
    void printDebugInfo() const {
        Serial.print("Wheel: ");
        if(hasValidTouch) {
            Serial.print("Angle="); Serial.print(currentAngle, 1);
            Serial.print("° Deltas=[");
            for(int i = 0; i < ELECTRODE_COUNT; i++) {
                Serial.print(touchDeltas[i]);
                if(i < ELECTRODE_COUNT - 1) Serial.print(",");
            }
            Serial.print("]");
        } else {
            Serial.print("No touch");
        }
        
        Serial.print(" Center=");
        Serial.println(centerPressed ? "PRESSED" : "Released");
        
        Serial.print("Baselines=[");
        for(int i = 0; i < ELECTRODE_COUNT; i++) {
            Serial.print(wheelBaseline[i]);
            if(i < ELECTRODE_COUNT - 1) Serial.print(",");
        }
        Serial.println("]");
    }
};