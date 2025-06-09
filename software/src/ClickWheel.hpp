#pragma once
#include "Adafruit_MPR121.h"
#include <Arduino.h>
#include <Wire.h>

class ClickWheel {
public:
  static const int RIGHT = 1;  // Right electrode
  static const int TOP = 2;    // Top electrode
  static const int BOTTOM = 3; // Bottom electrode
  static const int LEFT = 4;   // Left electrode
  // Center button
  static const uint8_t CENTER_ELECTRODE = 0;
  bool smoothingEnabled = true; // Public if you want to toggle

  // Wheel electrodes (using correct board mapping)
private:
  Adafruit_MPR121 *mpr121;
  static const uint8_t WHEEL_ELECTRODES = 4;
  uint8_t wheelPads[WHEEL_ELECTRODES] = {TOP, RIGHT, BOTTOM, LEFT};
  // Add these to your ClickWheel class:
  float electrodeSensitivity[WHEEL_ELECTRODES] = {0.896, 1.0,0.950, 0.913};
  bool sensitivityCalibrated = false;
  // Calibration state
  enum CalibState { CALIB_IDLE, CALIB_RUNNING, CALIB_COMPLETE };
  CalibState calibrationState = CALIB_IDLE;
  uint8_t currentCalibElectrode = 0;
  unsigned long calibStartTime = 0;
  int16_t maxCalibValues[WHEEL_ELECTRODES] = {0, 0, 0, 0};
  static const unsigned long CALIB_TIME_PER_ELECTRODE =
      5000;

public:
  // Touch state tracking
  bool centerPressed = false;
  bool lastCenterPressed = false;

  // Wheel baseline and state (implementing GitHub algorithm ourselves)
  int16_t wheelBaseline[WHEEL_ELECTRODES];
  int16_t lastWheelAngle = -1;
  bool baselineTaken = false;

  // Scroll tracking
  float scrollAccumulator = 0.0;
  static const int16_t DEGREES_PER_TRACK = 20;    // Reasonable sensitivity
  static const int16_t WHEEL_ANGLE_PER_TICK = 20; // au lieu de 30
  ClickWheel(Adafruit_MPR121 *mpr) : mpr121(mpr) {}

  bool begin() {
    if (!mpr121)
      return false;
    takeBaseline();
    return true;
  }

  void takeBaseline() {
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      wheelBaseline[wheelPads[i]] = mpr121->filteredData(wheelPads[i]);
    }
    baselineTaken = true;
    Serial.println("ClickWheel baseline taken (custom implementation)");
  }

  float angleForElectrode(uint8_t electrode) {
    switch (electrode) {
    case TOP:
      return 0.0;
    case RIGHT:
      return 90.0;
    case BOTTOM:
      return 180.0;
    case LEFT:
      return 270.0;
    default:
      return -1.0;
    }
  }

  // Method 3: Set all sensitivities at once
  void setSensitivities(float top, float right, float bottom, float left) {
    electrodeSensitivity[0] = top;    // TOP
    electrodeSensitivity[1] = right;  // RIGHT
    electrodeSensitivity[2] = bottom; // BOTTOM
    electrodeSensitivity[3] = left;   // LEFT
    sensitivityCalibrated = true;
  }

  // Updated getWheelAngle with sensitivity compensation
  int16_t getWheelAngle() {
    if (!baselineTaken)
      return -1;

    // 1. Calculate absolute deltas with sensitivity compensation
    int16_t wheelDeltas[WHEEL_ELECTRODES];
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      uint8_t electrode = wheelPads[i];
      int16_t rawDelta =
          abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));

      // Apply sensitivity scaling
      if (sensitivityCalibrated) {
        wheelDeltas[i] = (int16_t)(rawDelta * electrodeSensitivity[i]);
      } else {
        wheelDeltas[i] = rawDelta;
      }
    }

    // 2. Find the two most dominant pads
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

    // 3. Check if we have sufficient touch strength
    if (firstMaxVal < 6)
      return -1;

    // 4. If only one pad is significantly active, use its center angle
    if (secondMaxVal < 3 || secondMaxIdx == -1) {
      uint8_t electrode = wheelPads[firstMaxIdx];
      return (int16_t)angleForElectrode(electrode);
    }

    // 5. Interpolate between the two dominant pads
    float firstAngle = angleForElectrode(wheelPads[firstMaxIdx]);
    float secondAngle = angleForElectrode(wheelPads[secondMaxIdx]);

    // Handle wrap-around
    float angleDiff = secondAngle - firstAngle;
    if (angleDiff > 180)
      angleDiff -= 360;
    else if (angleDiff < -180)
      angleDiff += 360;

    // Calculate interpolation weight
    float totalStrength = firstMaxVal + secondMaxVal;
    float firstWeight = (float)firstMaxVal / totalStrength;
    float secondWeight = (float)secondMaxVal / totalStrength;

    // Interpolate angle
    float interpolatedAngle = firstAngle + (secondWeight * angleDiff);

    // Normalize to [0, 360)
    int16_t finalAngle = (int16_t)interpolatedAngle;
    if (finalAngle < 0)
      finalAngle += 360;
    if (finalAngle >= 360)
      finalAngle -= 360;

    return finalAngle;
  }

  // Debug method to see raw vs compensated values
  void printSensitivityDebug() {
    Serial.println("Electrode sensitivity debug:");
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      uint8_t electrode = wheelPads[i];
      int16_t rawDelta =
          abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
      int16_t compensated = (int16_t)(rawDelta * electrodeSensitivity[i]);

      Serial.print("Pad ");
      Serial.print(i);
      Serial.print(": raw=");
      Serial.print(rawDelta);
      Serial.print(", compensated=");
      Serial.print(compensated);
      Serial.print(", sensitivity=");
      Serial.println(electrodeSensitivity[i], 3);
    }
  }

  int16_t getWheelIncrement() {
    int16_t curWheelAngle = getWheelAngle();

    // Case 1: Wheel is not being touched
    if (curWheelAngle == -1) {
      lastWheelAngle = curWheelAngle;
      return 0;
    }

    // Case 2: Wheel has just started being touched
    if ((lastWheelAngle == -1) && (curWheelAngle > -1)) {
      lastWheelAngle = curWheelAngle;
      return 0;
    }

    // Case 3: Wheel was touched previously, and is currently being touched
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

    // Get wheel increment using our implementation
    int16_t wheelIncrement = getWheelIncrement();

    if (wheelIncrement != 0) {
      float scrollAmount = wheelIncrement * DEGREES_PER_TRACK;

      if (smoothingEnabled) {
        scrollAccumulator =
            0.7 * scrollAccumulator + 0.3 * (scrollAccumulator + scrollAmount);
      } else {
        scrollAccumulator += scrollAmount;
      }

      Serial.print("Wheel increment: ");
      Serial.print(wheelIncrement);
      Serial.print(" (");
      Serial.print(scrollAmount, 1);
      Serial.println("°)");
    }
  }

  // Public interface
  bool isWheelTouched() const {
    return const_cast<ClickWheel *>(this)->getWheelAngle() != -1;
  }

  bool isCenterPressed() const { return centerPressed; }
  bool wasCenterJustPressed() const {
    return centerPressed && !lastCenterPressed;
  }
  bool wasCenterJustReleased() const {
    return !centerPressed && lastCenterPressed;
  }

  // Scroll interface
  float getScrollDegrees() const { return scrollAccumulator; }

  int getScrollClicks() const {
    return (int)(scrollAccumulator / DEGREES_PER_TRACK);
  }

  float consumeScrollDegrees() {
    float scroll = scrollAccumulator;
    scrollAccumulator = 0.0;
    return scroll;
  }

  int consumeScrollClicks() {
    int clicks = getScrollClicks();
    scrollAccumulator -= clicks * DEGREES_PER_TRACK;
    return clicks;
  }

  int getScrollDirection() const {
    if (abs(scrollAccumulator) < 5.0)
      return 0;
    return scrollAccumulator > 0 ? 1 : -1;
  }

  void resetScroll() { scrollAccumulator = 0.0; }

  int16_t getDelta(uint8_t electrode) const {
    if (!baselineTaken || electrode > 11)
      return 0;
    return wheelBaseline[electrode] - mpr121->filteredData(electrode);
  }

  uint16_t getBaseline(int electrode) const {
    if (electrode < 0 || electrode >= WHEEL_ELECTRODES)
      return 0;
    return baselineTaken ? wheelBaseline[electrode] : 0;
  }

  // Method 1: Manual calibration - set specific scaling factors
  void setSensitivity(uint8_t electrodeIndex, float sensitivity) {
    if (electrodeIndex < WHEEL_ELECTRODES) {
      electrodeSensitivity[electrodeIndex] = sensitivity;
      sensitivityCalibrated = true;
    }
  }

  // Start auto-calibration process
  void startSensitivityCalibration() {
    if (!baselineTaken) {
      Serial.println("ERROR: Take baseline first before calibration!");
      return;
    }

    calibrationState = CALIB_RUNNING;
    currentCalibElectrode = 0;
    calibStartTime = millis();

    // Reset max values
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      maxCalibValues[i] = 0;
    }

    Serial.println("=== AUTO SENSITIVITY CALIBRATION ===");
    Serial.println("Touch each electrode FIRMLY when prompted");
    Serial.println("Each electrode gets 1 second");
    Serial.println();

    printCurrentCalibInstruction();
  }

  // Call this in your main loop during calibration
  void updateCalibration() {
    if (calibrationState != CALIB_RUNNING)
      return;

    unsigned long elapsed = millis() - calibStartTime;

    // Sample current electrode
    uint8_t electrode = wheelPads[currentCalibElectrode];
    int16_t currentDelta =
        abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));

    // Track maximum value for this electrode
    if (currentDelta > maxCalibValues[currentCalibElectrode]) {
      maxCalibValues[currentCalibElectrode] = currentDelta;
    }

    // Print progress every 200ms
    if (elapsed % 200 < 50) { // Rough timing for progress updates
      Serial.print(".");
    }

    // Move to next electrode after 1 second
    if (elapsed >= CALIB_TIME_PER_ELECTRODE) {
      Serial.println();
      Serial.print("Max value recorded: ");
      Serial.println(maxCalibValues[currentCalibElectrode]);
      Serial.println();

      currentCalibElectrode++;
      calibStartTime = millis();

      if (currentCalibElectrode >= WHEEL_ELECTRODES) {
        // Calibration complete
        finishCalibration();
      } else {
        // Next electrode
        printCurrentCalibInstruction();
      }
    }
  }

  // Add this debug method to your ClickWheel class:
void debugRightQuadrant() {
  if (!baselineTaken) return;
  
  Serial.println("=== RIGHT QUADRANT DEBUG ===");
  
  // // Raw deltas for all electrodes
  // Serial.print("Raw deltas: ");
  // for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
  //   uint8_t electrode = wheelPads[i];
  //   int16_t rawDelta = abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
  //   Serial.print(rawDelta);
  //   Serial.print(" ");
  // }
  // Serial.println();
  
  // // Compensated deltas
  // Serial.print("Compensated: ");
  // for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
  //   uint8_t electrode = wheelPads[i];
  //   int16_t rawDelta = abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
  //   int16_t compensated = (int16_t)(rawDelta * electrodeSensitivity[i]);
  //   Serial.print(compensated);
  //   Serial.print(" ");
  // }
  // Serial.println();
  
  // // Find two most dominant
  // int16_t wheelDeltas[WHEEL_ELECTRODES];
  // for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
  //   uint8_t electrode = wheelPads[i];
  //   int16_t rawDelta = abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
  //   wheelDeltas[i] = (int16_t)(rawDelta * electrodeSensitivity[i]);
  // }
  
  // int16_t firstMaxVal = 0, secondMaxVal = 0;
  // int16_t firstMaxIdx = -1, secondMaxIdx = -1;
  
  // for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
  //   if (wheelDeltas[i] > firstMaxVal) {
  //     secondMaxVal = firstMaxVal;
  //     secondMaxIdx = firstMaxIdx;
  //     firstMaxVal = wheelDeltas[i];
  //     firstMaxIdx = i;
  //   } else if (wheelDeltas[i] > secondMaxVal) {
  //     secondMaxVal = wheelDeltas[i];
  //     secondMaxIdx = i;
  //   }
  // }
  
  // Serial.print("Dominant pads: ");
  // if (firstMaxIdx != -1) {
  //   Serial.print("1st=");
  //   Serial.print(firstMaxIdx);
  //   Serial.print("(");
  //   Serial.print(firstMaxVal);
  //   Serial.print(") ");
  // }
  // if (secondMaxIdx != -1) {
  //   Serial.print("2nd=");
  //   Serial.print(secondMaxIdx);
  //   Serial.print("(");
  //   Serial.print(secondMaxVal);
  //   Serial.print(") ");
  // }
  // Serial.println();
  
  // // Show which electrodes those map to
  // if (firstMaxIdx != -1) {
  //   Serial.print("Electrode mapping: ");
  //   Serial.print("1st=electrode_");
  //   Serial.print(wheelPads[firstMaxIdx]);
  //   if (secondMaxIdx != -1) {
  //     Serial.print(", 2nd=electrode_");
  //     Serial.print(wheelPads[secondMaxIdx]);
  //   }
  //   Serial.println();
  // }
  
  // Final angle
  int16_t angle = getWheelAngle();
  Serial.print("Final angle: ");
  Serial.println(angle);
  Serial.println();
}

  bool isCalibrating() const { return calibrationState == CALIB_RUNNING; }

  bool isCalibrationComplete() const {
    return calibrationState == CALIB_COMPLETE;
  }

  void printDebugInfo() {
    Serial.print("Wheel: ");
    int16_t angle = getWheelAngle();
    if (angle != -1) {
      Serial.print("Angle=");
      Serial.print(angle);
      Serial.print("° Scroll=");
      Serial.print(scrollAccumulator, 1);
      Serial.print("°");
    } else {
      Serial.print("No touch");
    }

    Serial.print(" Center=");
    Serial.println(centerPressed ? "PRESSED" : "Released");
  }

private:
  void printCurrentCalibInstruction() {
    Serial.print("Touch ");
    switch (currentCalibElectrode) {
    case 0:
      Serial.print("TOP");
      break;
    case 1:
      Serial.print("RIGHT");
      break;
    case 2:
      Serial.print("BOTTOM");
      break;
    case 3:
      Serial.print("LEFT");
      break;
    }
    Serial.println(" electrode NOW!");
  }

  void finishCalibration() {
    calibrationState = CALIB_COMPLETE;

    Serial.println("=== CALIBRATION COMPLETE ===");
    Serial.println("Recorded maximum values:");

    // Find the minimum recorded value (our reference)
    int16_t minValue = maxCalibValues[0];
    for (uint8_t i = 1; i < WHEEL_ELECTRODES; i++) {
      if (maxCalibValues[i] < minValue && maxCalibValues[i] > 0) {
        minValue = maxCalibValues[i];
      }
    }

    if (minValue == 0) {
      Serial.println("ERROR: No valid calibration data recorded!");
      calibrationState = CALIB_IDLE;
      return;
    }

    Serial.println();
    Serial.println("Calculated sensitivity factors:");

    // Calculate and apply sensitivity factors
    for (uint8_t i = 0; i < WHEEL_ELECTRODES; i++) {
      if (maxCalibValues[i] > 0) {
        electrodeSensitivity[i] = (float)minValue / (float)maxCalibValues[i];
      } else {
        electrodeSensitivity[i] = 1.0; // Fallback
        Serial.print("WARNING: No data for electrode ");
        Serial.println(i);
      }

      Serial.print("Electrode ");
      Serial.print(i);
      Serial.print(" (");
      switch (i) {
      case 0:
        Serial.print("TOP");
        break;
      case 1:
        Serial.print("RIGHT");
        break;
      case 2:
        Serial.print("BOTTOM");
        break;
      case 3:
        Serial.print("LEFT");
        break;
      }
      Serial.print("): max=");
      Serial.print(maxCalibValues[i]);
      Serial.print(", sensitivity=");
      Serial.println(electrodeSensitivity[i], 3);
    }

    sensitivityCalibrated = true;

    Serial.println();
    Serial.println("Copy this code to your setup() for permanent calibration:");
    Serial.print("wheel.setSensitivities(");
    Serial.print(electrodeSensitivity[0], 3);
    Serial.print(", ");
    Serial.print(electrodeSensitivity[1], 3);
    Serial.print(", ");
    Serial.print(electrodeSensitivity[2], 3);
    Serial.print(", ");
    Serial.print(electrodeSensitivity[3], 3);
    Serial.println(");");
    Serial.println();
  }
};