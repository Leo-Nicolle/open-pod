#pragma once
#include "Adafruit_MPR121.h"
#include <Arduino.h>
#include <Wire.h>

class ClickWheel {
public:
  // Electrode constants (hardware mapping)
  static const int RIGHT = 0;
  static const int TOP = 2;
  static const int BOTTOM = 3;
  static const int LEFT = 1;

  // Mechanical buttons (Alps SKQGAKE010, see BTN_* in pinout.h)
  enum class Button : uint8_t { Center, Top, Right, Bottom, Left, Count };
  static const uint8_t BUTTON_COUNT = (uint8_t)Button::Count;

  enum class EventType : uint8_t {
    ButtonDown,    // debounced press edge
    ButtonUp,      // debounced release edge
    Click,         // released before LONG_PRESS_MS
    LongPress,     // held >= LONG_PRESS_MS (once per hold, while still held)
    VeryLongPress, // held >= VERY_LONG_PRESS_MS (once per hold)
    Scroll,        // value = signed scroll clicks
    TouchStart,    // capacitive wheel touch began
    TouchEnd       // capacitive wheel touch ended
  };

  struct Event {
    EventType type;
    Button button; // valid for button events only
    int16_t value; // scroll clicks for Scroll, 0 otherwise
  };

  static const unsigned long DEBOUNCE_MS = 20;
  static const unsigned long LONG_PRESS_MS = 800;
  static const unsigned long VERY_LONG_PRESS_MS = 2000;

  // Configuration
  // Wheel travel per Scroll click. The finger angle is accumulated
  // losslessly, so this is the real angle per click (15 -> 24 clicks/turn).
  static constexpr float DEFAULT_DEGREES_PER_CLICK = 15.0;

  // Software touch thresholds applied to the baseline-compensated delta in
  // getWheelAngle() (separate from the MPR121's own hardware touch
  // threshold, which is what touched()/getTouchedMask() reflects).
  static const int16_t TOUCH_THRESHOLD = 6;       // min dominant-pad delta to count as a wheel touch
  static const int16_t SECOND_PAD_THRESHOLD = 3;  // min 2nd-pad delta to interpolate instead of snapping

  // Lifting the finger smears it across the pads and fakes a +/-1 click.
  // Scroll clicks are held this long before being emitted, and dropped if
  // the touch ends in the meantime. Also the added scroll latency.
  static const unsigned long SCROLL_LIFTOFF_MS = 80;

  // After a touch ends, keep polling this long before trusting ~IRQ again:
  // the MPR121's auto-baseline drifted toward the finger during the touch
  // and needs ~1s to recover, so a quick re-touch doesn't raise ~IRQ.
  static const unsigned long IRQ_RESUME_MS = 1500;

private:
  // Hardware
  Adafruit_MPR121 *mpr121;
  int irqPin = -1;              // MPR121 ~IRQ, -1 = not wired (always poll)
  uint16_t lastTouchedMask = 0; // last touched() result, gates the IRQ skip
  static const uint8_t WHEEL_ELECTRODES = 4;
  
  // Electrode mapping in clockwise order starting from TOP
  uint8_t wheelPads[WHEEL_ELECTRODES] = {TOP, RIGHT, BOTTOM, LEFT};
  
  // Calibration and sensitivity
  float electrodeSensitivity[WHEEL_ELECTRODES] = {1.0, 1.0, 1.0, 1.0};
  bool sensitivityCalibrated = false;
  
  // Button state (pins are active low, INPUT_PULLUP)
  struct ButtonState {
    int pin = -1;                    // -1 = not wired
    bool rawPressed = false;         // last raw sample
    unsigned long lastRawChange = 0; // when rawPressed last flipped
    bool pressed = false;            // debounced
    unsigned long pressStart = 0;
    bool longFired = false;
    bool veryLongFired = false;
    // Set by the falling-edge ISR: catches presses shorter than the gap
    // between two polls (a slow loop() pass would otherwise miss them).
    volatile bool fallPending = false;
    volatile unsigned long lastFall = 0;
  };
  ButtonState buttons[BUTTON_COUNT];

  // Event queue (ring buffer, newest dropped on overflow)
  static const uint8_t EVENT_QUEUE_SIZE = 16;
  Event eventQueue[EVENT_QUEUE_SIZE];
  uint8_t queueHead = 0;
  uint8_t queueCount = 0;

  // Scroll clicks waiting out SCROLL_LIFTOFF_MS (oldest first)
  struct HeldScroll {
    unsigned long at;
    int16_t clicks;
  };
  static const uint8_t HELD_SCROLL_SIZE = 8;
  HeldScroll heldScroll[HELD_SCROLL_SIZE];
  uint8_t heldScrollCount = 0;

  // Touch state
  bool wheelTouched = false; // finger on the wheel (software threshold, see update())
  unsigned long lastWheelTouchAt = 0; // last sample with wheelTouched, gates the IRQ skip
  
  // Wheel state
  int16_t wheelBaseline[12]; // Full baseline array for all 12 electrodes
  int16_t lastWheelAngle = -1;
  bool baselineTaken = false;
  
  // Scroll tracking
  float scrollAccumulator = 0.0; // degrees not yet turned into clicks
  float degreesPerClick = DEFAULT_DEGREES_PER_CLICK;
  
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
    if (firstMaxVal < TOUCH_THRESHOLD) return -1;

    // Single pad active - return center angle
    if (secondMaxVal < SECOND_PAD_THRESHOLD || secondMaxIdx == -1) {
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

  // Signed degrees the finger moved since the previous sample (positive =
  // counter-clockwise, the angle convention of getWheelAngle()). The
  // reference follows every sample, so no movement is ever dropped.
  int16_t getWheelIncrement(int16_t curWheelAngle) {
    if (curWheelAngle == -1 || lastWheelAngle == -1) {
      lastWheelAngle = curWheelAngle;
      return 0;
    }

    // Shortest way round, so crossing 0/360 isn't a full turn
    int16_t delta = curWheelAngle - lastWheelAngle;
    if (delta > 180) delta -= 360;
    else if (delta < -180) delta += 360;

    lastWheelAngle = curWheelAngle;
    return delta;
  }

  // Optional: MPR121 ~IRQ pin. When set, update() skips the I2C reads while
  // nothing is touched and IRQ is high (no touch-status change). While
  // anything is touched it keeps polling, because the scroll angle comes
  // from filteredData(), and IRQ doesn't fire when that changes. "Touched"
  // includes our own software touch (lastWheelAngle != -1): the MPR121's
  // auto-baseline slowly absorbs a finger that keeps moving around the wheel,
  // so its hardware touch bits drop out mid-scroll while the finger is still
  // clearly visible against our fixed wheelBaseline.
  void setIrqPin(int pin) {
    irqPin = pin;
    pinMode(irqPin, INPUT_PULLUP);
  }

  void setButtonPins(int center, int top, int right, int bottom, int left) {
    const int pins[BUTTON_COUNT] = {center, top, right, bottom, left};
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      buttons[i] = ButtonState();
      buttons[i].pin = pins[i];
      if (pins[i] >= 0) {
        pinMode(pins[i], INPUT_PULLUP);
        ButtonState *b = &buttons[i];
        attachInterrupt(digitalPinToInterrupt(pins[i]), [b]() {
          b->fallPending = true;
          b->lastFall = millis();
        }, FALLING);
      }
    }
  }

  void update() {
    // Buttons first: they must keep working while the IRQ skip below
    // short-circuits the capacitive reads.
    updateButtons();

    if (irqPin >= 0 && lastTouchedMask == 0 && lastWheelAngle == -1 &&
        millis() - lastWheelTouchAt >= IRQ_RESUME_MS &&
        digitalRead(irqPin) == HIGH) {
      return;
    }

    // Reading the touch status also releases ~IRQ.
    lastTouchedMask = mpr121->touched();

    // Touch start/end follow the same software reading as the scroll, not
    // the hardware touch bits (see setIrqPin()), so a scroll is never cut by
    // a hardware release while the finger is still turning.
    int16_t angle = getWheelAngle();
    bool wheelTouchedNow = angle != -1;

    if (wheelTouchedNow && !wheelTouched) {
      resetScroll();
      pushEvent({EventType::TouchStart, Button::Count, 0});
    } else if (!wheelTouchedNow && wheelTouched) {
      heldScrollCount = 0; // lift-off jitter: drop clicks not yet emitted
      pushEvent({EventType::TouchEnd, Button::Count, 0});
    }
    wheelTouched = wheelTouchedNow;
    if (wheelTouched) lastWheelTouchAt = millis();

    // Clicks are cut from the accumulated angle; the remainder carries over,
    // and a jittering finger has to travel a full click back to undo one.
    scrollAccumulator += getWheelIncrement(angle);

    int clicks = consumeScrollClicks();
    if (clicks != 0) {
      if (heldScrollCount == HELD_SCROLL_SIZE) {
        releaseHeldScroll(1); // full: emit the oldest early
      }
      heldScroll[heldScrollCount++] = {millis(), (int16_t)clicks};
    }

    uint8_t due = 0;
    while (due < heldScrollCount &&
           millis() - heldScroll[due].at >= SCROLL_LIFTOFF_MS) {
      due++;
    }
    releaseHeldScroll(due);
  }

  // Emits the n oldest held clicks as one Scroll event.
  void releaseHeldScroll(uint8_t n) {
    if (n == 0) return;
    int16_t clicks = 0;
    for (uint8_t i = 0; i < n; i++) clicks += heldScroll[i].clicks;
    for (uint8_t i = n; i < heldScrollCount; i++) heldScroll[i - n] = heldScroll[i];
    heldScrollCount -= n;
    if (clicks != 0) {
      pushEvent({EventType::Scroll, Button::Count, clicks});
    }
  }

  // Pops the oldest pending event; returns false when the queue is empty.
  bool pollEvent(Event &e) {
    if (queueCount == 0) return false;
    e = eventQueue[queueHead];
    queueHead = (queueHead + 1) % EVENT_QUEUE_SIZE;
    queueCount--;
    return true;
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
  bool isWheelTouched() const { return wheelTouched; }
  bool isCalibrating() const { return calibrationState == CALIB_RUNNING; }

  // Button state
  bool isPressed(Button b) const { return buttons[(uint8_t)b].pressed; }
  unsigned long pressedDuration(Button b) const {
    const ButtonState &s = buttons[(uint8_t)b];
    return s.pressed ? millis() - s.pressStart : 0;
  }
  // Bit i set = Button(i) pressed
  uint8_t pressedMask() const {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (buttons[i].pressed) mask |= (1 << i);
    }
    return mask;
  }
  static const char *buttonName(Button b) {
    switch (b) {
    case Button::Center: return "CENTER";
    case Button::Top:    return "TOP";
    case Button::Right:  return "RIGHT";
    case Button::Bottom: return "BOTTOM";
    case Button::Left:   return "LEFT";
    default:             return "?";
    }
  }

  // Debug/HUD interface: raw MPR121 hardware touch bits (independent of the
  // software delta threshold above).
  uint16_t getTouchedMask() const { return mpr121->touched(); }

  // Scroll interface (scroll clicks are delivered as Scroll events)
  float getScrollDegrees() const { return scrollAccumulator; }
  void setDegreesPerClick(float deg) { degreesPerClick = deg; }
  float getDegreesPerClick() const { return degreesPerClick; }

  // Debug methods
  int16_t getDelta(uint8_t electrode) const {
    if (!baselineTaken || electrode >= 12) return 0;
    return abs(wheelBaseline[electrode] - mpr121->filteredData(electrode));
  }

  void printDebugInfo() {
    int16_t angle = getWheelAngle();
    Serial.print("Angle: ");
    Serial.print(angle);
    Serial.print("° Buttons: 0x");
    Serial.println(pressedMask(), HEX);
  }

private:
  int getScrollClicks() const { return (int)(scrollAccumulator / degreesPerClick); }
  int consumeScrollClicks() { int c = getScrollClicks(); scrollAccumulator -= c * degreesPerClick; return c; }
  void resetScroll() { scrollAccumulator = 0.0; }

  void pushEvent(const Event &e) {
    if (queueCount >= EVENT_QUEUE_SIZE) return; // full: drop newest
    eventQueue[(queueHead + queueCount) % EVENT_QUEUE_SIZE] = e;
    queueCount++;
  }

  void updateButtons() {
    unsigned long now = millis();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      ButtonState &b = buttons[i];
      if (b.pin < 0) continue;
      Button id = (Button)i;

      bool raw = digitalRead(b.pin) == LOW; // active low
      if (raw != b.rawPressed) {
        b.rawPressed = raw;
        b.lastRawChange = now;
      }

      // Debounce: accept a level only once it has been stable long enough
      if (b.rawPressed != b.pressed && now - b.lastRawChange >= DEBOUNCE_MS) {
        b.pressed = b.rawPressed;
        // Any edge so far (incl. bounce) belongs to this debounced press/release
        b.fallPending = false;
        if (b.pressed) {
          b.pressStart = now;
          b.longFired = false;
          b.veryLongFired = false;
          pushEvent({EventType::ButtonDown, id, 0});
        } else {
          pushEvent({EventType::ButtonUp, id, 0});
          if (!b.longFired) pushEvent({EventType::Click, id, 0});
        }
      }

      // A whole press+release fit between two polls: the debounce never saw
      // it, but the ISR did. Replay it as a click once the line has settled.
      if (b.fallPending && !b.pressed && !b.rawPressed &&
          now - b.lastFall >= DEBOUNCE_MS) {
        b.fallPending = false;
        pushEvent({EventType::ButtonDown, id, 0});
        pushEvent({EventType::ButtonUp, id, 0});
        pushEvent({EventType::Click, id, 0});
      }

      if (b.pressed) {
        unsigned long held = now - b.pressStart;
        if (!b.longFired && held >= LONG_PRESS_MS) {
          b.longFired = true;
          pushEvent({EventType::LongPress, id, 0});
        }
        if (!b.veryLongFired && held >= VERY_LONG_PRESS_MS) {
          b.veryLongFired = true;
          pushEvent({EventType::VeryLongPress, id, 0});
        }
      }
    }
  }

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