#pragma once
#include "Adafruit_MPR121.h"
#include <Arduino.h>
#include <Wire.h>

class ClickWheel {
public:
  // Electrode constants (hardware mapping)
  static const int RIGHT = 1;
  static const int TOP = 2;
  static const int BOTTOM = 3;
  static const int LEFT = 4;
  static const uint8_t CENTER_ELECTRODE = 0;
  
  // Configuration
  bool smoothingEnabled = true;
  static const int16_t DEGREES_PER_TRACK = 10;  // Reduced from 20 - more sensitive
  static const int16_t WHEEL_ANGLE_PER_TICK = 10; // Reduced from 20 - more sensitive

private:
  // Hardware
  Adafruit_MPR121 *mpr121;
  static const uint8_t WHEEL_ELECTRODES = 4;
  
  // Electrode mapping in clockwise order starting from TOP
  uint8_t wheelPads[WHEEL_ELECTRODES] = {TOP, RIGHT, BOTTOM, LEFT};
  
  // Calibration and sensitivity
  float electrodeSensitivity[WHEEL_ELECTRODES] = {1.0, 1.0, 1.0, 1.0};
  bool sensitivityCalibrated = false;
  
  // Touch state
  bool centerPressed = false;
  bool lastCenterPressed = false;
  
  // Wheel state
  int16_t wheelBaseline[12]; // Full baseline array for all 12 electrodes
  int16_t lastWheelAngle = -1;
  bool baselineTaken = false;
  
  // Scroll tracking
  float scrollAccumulator = 0.0;
  
  // Calibration state
  enum CalibState { CALIB_IDLE, CALIB_RUNNING, CALIB_COMPLETE };
  CalibState calibrationState = CALIB_IDLE;
  uint8_t currentCalibElectrode = 0;
  unsigned long calibStartTime = 0;
  int16_t maxCalibValues[WHEEL_ELECTRODES] = {0, 0, 0, 0};
  static const unsigned long CALIB_TIME_PER_ELECTRODE = 2000; // 2 seconds

public:
  ClickWheel(Adafruit_MPR121 *mpr) : mpr121(mpr) {}

  bool begin() {
    if (!mpr121) return false;
    takeBaseline();
    return true;
  }

  void takeBaseline() {
    // Take baseline for all electrodes to avoid array bounds issues
    for (uint8_t i = 0; i < 12; i++) {
      wheelBaseline[i] = mpr121->filteredData(i);
    }
    baselineTaken = true;
    Serial.println("ClickWheel baseline taken");
  }

  // Fix angle mapping to match your coordinate system
  float angleForElectrode(uint8_t electrode) {
    switch (electrode) {
    case TOP:    return 90.0;  // TOP at 90° (12 o'clock)
    case RIGHT:  return 0.0;   // RIGHT at 0° (3 o'clock)  
    case BOTTOM: return 270.0; // BOTTOM at 270° (6 o'clock)
    case LEFT:   return 180.0; // LEFT at 180° (9 o'clock)
    default:     return -1.0;
    }
  }

  int16_t getWheelAngle() {
    if (!baselineTaken) return -1;

    // Calculate compensated deltas
    int16_t wheelDeltas[WHEEL_ELECTRODES];
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      uint8_t electrode = wheelPads[i];
      int16_t rawDelta = abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
      wheelDeltas[i] = sensitivityCalibrated ? 
        (int16_t)(rawDelta * electrodeSensitivity[i]) : rawDelta;
    }

    // Find two most dominant pads
    int16_t firstMaxVal = 0, secondMaxVal = 0;
    int16_t firstMaxIdx = -1, secondMaxIdx = -1;

    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      if (wheelDeltas[i] > firstMaxVal) {
        secondMaxVal = firstMaxVal;
        secondMaxIdx = firstMaxIdx;
        firstMaxVal = wheelDeltas[i];
        firstMaxIdx = i;
      } else if (wheelDeltas[i] > secondMaxVal) {
        secondMaxVal = wheelDeltas[i];
        secondMaxIdx = i;
      }
    }

    // Check for sufficient touch
    if (firstMaxVal < 6) return -1;

    // Single pad active - return center angle
    if (secondMaxVal < 3 || secondMaxIdx == -1) {
      return (int16_t)angleForElectrode(wheelPads[firstMaxIdx]);
    }

    // Interpolate between two dominant pads
    float firstAngle = angleForElectrode(wheelPads[firstMaxIdx]);
    float secondAngle = angleForElectrode(wheelPads[secondMaxIdx]);

    // Handle wrap-around (e.g., between 270° and 90°)
    float angleDiff = secondAngle - firstAngle;
    if (angleDiff > 180) angleDiff -= 360;
    else if (angleDiff < -180) angleDiff += 360;

    // Weighted interpolation
    float totalStrength = firstMaxVal + secondMaxVal;
    float secondWeight = (float)secondMaxVal / totalStrength;
    float interpolatedAngle = firstAngle + (secondWeight * angleDiff);

    // Normalize to [0, 360)
    int16_t finalAngle = (int16_t)interpolatedAngle;
    while (finalAngle < 0) finalAngle += 360;
    while (finalAngle >= 360) finalAngle -= 360;

    return finalAngle;
  }

  int16_t getWheelIncrement() {
    int16_t curWheelAngle = getWheelAngle();

    if (curWheelAngle == -1) {
      lastWheelAngle = curWheelAngle;
      return 0;
    }

    if ((lastWheelAngle == -1) && (curWheelAngle > -1)) {
      lastWheelAngle = curWheelAngle;
      return 0;
    }

    // Handle wrap-around for increment calculation
    int16_t curWheelAngleAdj = curWheelAngle;
    if (curWheelAngleAdj < (lastWheelAngle - 180)) {
      curWheelAngleAdj += 360;
    } else if (curWheelAngleAdj > (lastWheelAngle + 180)) {
      curWheelAngleAdj -= 360;
    }

    int16_t delta = (curWheelAngleAdj - lastWheelAngle) / WHEEL_ANGLE_PER_TICK;
    if (delta != 0) {
      lastWheelAngle = curWheelAngle;
    }

    return delta;
  }

  void update() {
    lastCenterPressed = centerPressed;
    
    // Center button detection
    uint16_t touched = mpr121->touched();
    centerPressed = (touched & (1 << CENTER_ELECTRODE)) != 0;

    // Process wheel increment
    int16_t wheelIncrement = getWheelIncrement();
    if (wheelIncrement != 0) {
      float scrollAmount = wheelIncrement * DEGREES_PER_TRACK;
      
      if (smoothingEnabled) {
        scrollAccumulator = 0.7 * scrollAccumulator + 0.3 * (scrollAccumulator + scrollAmount);
      } else {
        scrollAccumulator += scrollAmount;
      }
    }
  }

  // Calibration methods
  void setSensitivities(float top, float right, float bottom, float left) {
    electrodeSensitivity[0] = top;    // TOP
    electrodeSensitivity[1] = right;  // RIGHT
    electrodeSensitivity[2] = bottom; // BOTTOM
    electrodeSensitivity[3] = left;   // LEFT
    sensitivityCalibrated = true;
  }

  void startSensitivityCalibration() {
    if (!baselineTaken) {
      Serial.println("ERROR: Take baseline first!");
      return;
    }
    
    calibrationState = CALIB_RUNNING;
    currentCalibElectrode = 0;
    calibStartTime = millis();
    
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      maxCalibValues[i] = 0;
    }
    
    Serial.println("=== AUTO SENSITIVITY CALIBRATION ===");
    Serial.println("Touch each electrode FIRMLY when prompted");
    printCurrentCalibInstruction();
  }

  void updateCalibration() {
    if (calibrationState != CALIB_RUNNING) return;

    unsigned long elapsed = millis() - calibStartTime;
    uint8_t electrode = wheelPads[currentCalibElectrode];
    int16_t currentDelta = abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));

    if (currentDelta > maxCalibValues[currentCalibElectrode]) {
      maxCalibValues[currentCalibElectrode] = currentDelta;
    }

    if (elapsed >= CALIB_TIME_PER_ELECTRODE) {
      Serial.print("Max: ");
      Serial.println(maxCalibValues[currentCalibElectrode]);
      
      currentCalibElectrode++;
      calibStartTime = millis();
      
      if (currentCalibElectrode >= WHEEL_ELECTRODES) {
        finishCalibration();
      } else {
        printCurrentCalibInstruction();
      }
    }
  }

  // Public interface
  bool isWheelTouched() const { return const_cast<ClickWheel*>(this)->getWheelAngle() != -1; }
  bool isCenterPressed() const { return centerPressed; }
  bool wasCenterJustPressed() const { return centerPressed && !lastCenterPressed; }
  bool wasCenterJustReleased() const { return !centerPressed && lastCenterPressed; }
  bool isCalibrating() const { return calibrationState == CALIB_RUNNING; }
  
  // Scroll interface
  float getScrollDegrees() const { return scrollAccumulator; }
  int getScrollClicks() const { return (int)(scrollAccumulator / DEGREES_PER_TRACK); }
  float consumeScrollDegrees() { float s = scrollAccumulator; scrollAccumulator = 0.0; return s; }
  int consumeScrollClicks() { int c = getScrollClicks(); scrollAccumulator -= c * DEGREES_PER_TRACK; return c; }
  void resetScroll() { scrollAccumulator = 0.0; }

  // Debug methods
  int16_t getDelta(uint8_t electrode) const {
    if (!baselineTaken || electrode >= 12) return 0;
    return abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
  }

  void printDebugInfo() {
    int16_t angle = getWheelAngle();
    Serial.print("Angle: ");
    Serial.print(angle);
    Serial.print("° Center: ");
    Serial.println(centerPressed ? "PRESSED" : "Released");
  }

private:
  void printCurrentCalibInstruction() {
    const char* names[] = {"TOP", "RIGHT", "BOTTOM", "LEFT"};
    Serial.print("Touch ");
    Serial.print(names[currentCalibElectrode]);
    Serial.println(" electrode NOW!");
  }

  void finishCalibration() {
    calibrationState = CALIB_COMPLETE;
    
    int16_t minValue = maxCalibValues[0];
    for (uint8_t i = 1; i < WHEEL_ELECTRODES; i++) {
      if (maxCalibValues[i] < minValue && maxCalibValues[i] > 0) {
        minValue = maxCalibValues[i];
      }
    }
    
    if (minValue == 0) {
      Serial.println("Calibration failed!");
      calibrationState = CALIB_IDLE;
      return;
    }
    
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      electrodeSensitivity[i] = maxCalibValues[i] > 0 ? 
        (float)minValue / (float)maxCalibValues[i] : 1.0;
    }
    
    sensitivityCalibrated = true;
    
    Serial.println("Calibration complete!");
    Serial.print("wheel.setSensitivities(");
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      Serial.print(electrodeSensitivity[i], 3);
      if (i < WHEEL_ELECTRODES - 1) Serial.print(", ");
    }
    Serial.println(");");
  }
};