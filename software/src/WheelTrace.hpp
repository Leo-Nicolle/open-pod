#pragma once
#include "ClickWheel.hpp"
#include "rendering/ILI9341_GFX.h"
#include <Arduino.h>
#include <math.h>

#define DEG_TO_RAD 0.0174532925

class WheelTrace {
public:
  static const int MAX_POINTS = 32;
  static const uint16_t TRACE_RADIUS = 3;

  enum Pad { TOP = 0, RIGHT = 1, BOTTOM = 2, LEFT = 3 };

  struct TracePoint {
    float angle;
    unsigned long timestamp;
  };

private:
  TracePoint points[MAX_POINTS];
  int index;
  int count;
  unsigned long fadeTimeMs;

  // Click/tap flash state for the debug HUD: a tap fires for only one loop
  // iteration (see ClickWheel::update()'s one-shot *PressFired flags), which
  // is too brief to see on screen, so latch a timestamp and fade it out.
  unsigned long centerFlashTime = 0;
  unsigned long topFlashTime = 0;
  unsigned long bottomFlashTime = 0;
  static const unsigned long FLASH_DURATION_MS = 400;

public:
  WheelTrace() : index(0), count(0), fadeTimeMs(1000) {}

  void add(float angle) {
    points[index].angle = angle;
    points[index].timestamp = millis();
    index = (index + 1) % MAX_POINTS;
    if (count < MAX_POINTS) count++;
  }

  void draw(ILI9341_GFX &display, int cx, int cy, int r) {
    unsigned long currentTime = millis();
    
    // Draw crosshairs for reference
    display.drawFastHLine(cx - r, cy, 2 * r + 1, 0x4208); // Dark gray
    display.drawFastVLine(cx, cy - r, 2 * r + 1, 0x4208);
    
    // Draw trace points
    for (int i = 0; i < count; i++) {
      unsigned long age = currentTime - points[i].timestamp;
      if (age > fadeTimeMs) continue;
      
      float angleRad = points[i].angle * DEG_TO_RAD;
      int x = cx + cos(angleRad) * r;
      int y = cy - sin(angleRad) * r;

      Pad pad = padFromAngle(points[i].angle);
      uint16_t color = colorForPad(pad);
      
      // Apply fade effect
      if (age > fadeTimeMs / 2) {
        float fadeRatio = 1.0 - (float)(age - fadeTimeMs/2) / (fadeTimeMs/2);
        color = fadeColor(color, fadeRatio);
      }
      
      display.fillCircle(x, y, TRACE_RADIUS, color);
    }
  }

  void setFadeTime(unsigned long ms) { fadeTimeMs = ms; }
  void clear() { count = 0; index = 0; }

  // Debug HUD: per-electrode raw delta vs. the software touch threshold(s),
  // plus click/tap state, so mis-scrolls/mis-clicks can be traced back to
  // whether the touch/tap logic saw what the finger actually did.
  void drawDebugHUD(ILI9341_GFX &display, ClickWheel &wheel, int x, int y) {
    unsigned long now = millis();
    if (wheel.wasCenterJustPressed()) centerFlashTime = now;
    if (wheel.wasTopJustPressed()) topFlashTime = now;
    if (wheel.wasBottomJustPressed()) bottomFlashTime = now;

    struct ElectrodeInfo { const char *name; uint8_t electrode; };
    const ElectrodeInfo electrodes[5] = {
      {"CTR", ClickWheel::CENTER_ELECTRODE},
      {"TOP", ClickWheel::TOP},
      {"RGT", ClickWheel::RIGHT},
      {"BTM", ClickWheel::BOTTOM},
      {"LFT", ClickWheel::LEFT},
    };

    const int barW = 22;
    const int barGap = 8;
    const int barMaxH = 50;
    const int16_t deltaScaleMax = 40; // deltas beyond this just fill the bar
    const int barBaseY = y + barMaxH;
    const int panelW = 5 * (barW + barGap) + 12;
    const int panelH = 170;

    display.fillRect(x - 10, y - 14, panelW, panelH, 0x0000); // clear panel

    // Threshold lines shared across all bars (drawn full-width so it's
    // obvious which electrodes are currently above/below each one).
    int touchLineY = barBaseY - (barMaxH * ClickWheel::TOUCH_THRESHOLD) / deltaScaleMax;
    int secondLineY = barBaseY - (barMaxH * ClickWheel::SECOND_PAD_THRESHOLD) / deltaScaleMax;
    display.drawFastHLine(x - 2, secondLineY, panelW - 16, 0xFFE0); // yellow: 2nd-pad/interp threshold
    display.drawFastHLine(x - 2, touchLineY, panelW - 16, 0xF800);  // red: dominant-pad touch threshold

    uint16_t touchedMask = wheel.getTouchedMask();

    for (int i = 0; i < 5; i++) {
      int16_t delta = wheel.getDelta(electrodes[i].electrode);
      int16_t clamped = delta > deltaScaleMax ? deltaScaleMax : delta;
      int barH = (barMaxH * clamped) / deltaScaleMax;
      int bx = x + i * (barW + barGap);

      bool hwTouched = (touchedMask & (1 << electrodes[i].electrode)) != 0;
      bool aboveThreshold = delta >= ClickWheel::TOUCH_THRESHOLD;
      uint16_t barColor = aboveThreshold ? 0x07E0 : 0x39E7;   // green vs. gray
      uint16_t frameColor = hwTouched ? 0xFFE0 : 0x4208;      // yellow if MPR121 HW threshold says touched

      display.drawRect(bx, y, barW, barMaxH, frameColor);
      if (barH > 0) {
        display.fillRect(bx + 1, barBaseY - barH, barW - 2, barH - 1, barColor);
      }

      // TOP/BOTTOM also drive tap-vs-scroll disambiguation: mark whether the
      // current touch is still a live tap candidate for that electrode.
      bool tapCandidate = (electrodes[i].electrode == ClickWheel::TOP && wheel.isTopTapCandidate()) ||
                           (electrodes[i].electrode == ClickWheel::BOTTOM && wheel.isBottomTapCandidate());
      if (electrodes[i].electrode == ClickWheel::TOP || electrodes[i].electrode == ClickWheel::BOTTOM) {
        uint16_t dotColor = tapCandidate ? 0x07FF : 0x2104; // cyan when a tap is still "live"
        display.fillRect(bx + barW / 2 - 2, y - 8, 4, 4, dotColor);
      }

      display.setTextSize(1);
      display.setTextColor(0xFFFF);
      display.setCursor(bx, barBaseY + 4);
      display.print(electrodes[i].name);
      display.setCursor(bx, barBaseY + 14);
      display.print(delta);
    }

    int clickY = barBaseY + 30;
    drawClickIndicator(display, x, clickY, "CTR", wheel.isCenterPressed(), centerFlashTime, now);
    drawClickIndicator(display, x + 55, clickY, "TOP", wheel.isTopPressed(), topFlashTime, now);
    drawClickIndicator(display, x + 110, clickY, "BOT", wheel.isBottomPressed(), bottomFlashTime, now);

    int16_t angle = wheel.getWheelAngle();
    display.setTextSize(1);
    display.setTextColor(0xFFFF);
    display.setCursor(x, clickY + 26);
    display.print("Angle: ");
    if (angle == -1) display.print("--");
    else display.print(angle);
  }

  static Pad padFromAngle(float angle) {
    // Match the coordinate system: 0°=RIGHT, 90°=TOP, 180°=LEFT, 270°=BOTTOM
    if (angle >= 315 || angle < 45) return RIGHT;  // 0° ± 45°
    if (angle < 135) return TOP;                   // 90° ± 45°
    if (angle < 225) return LEFT;                  // 180° ± 45°
    return BOTTOM;                                 // 270° ± 45°
  }

  static uint16_t colorForPad(Pad pad) {
    switch (pad) {
    case TOP:    return 0x001F; // Blue
    case RIGHT:  return 0xF800; // Red  
    case BOTTOM: return 0xFFE0; // Yellow
    case LEFT:   return 0x07E0; // Green
    default:     return 0xFFFF; // White
    }
  }

private:
  void drawClickIndicator(ILI9341_GFX &display, int x, int y, const char *label,
                           bool pressed, unsigned long flashTime, unsigned long now) {
    bool flashing = (now - flashTime) < FLASH_DURATION_MS;
    uint16_t color = pressed ? 0x07E0 : (flashing ? 0xFFE0 : 0x4208); // green held, yellow=recent tap
    display.fillRect(x, y, 16, 16, color);
    display.drawRect(x, y, 16, 16, 0xFFFF);
    display.setTextSize(1);
    display.setTextColor(0xFFFF);
    display.setCursor(x, y + 18);
    display.print(label);
  }

  uint16_t fadeColor(uint16_t color, float intensity) {
    if (intensity <= 0) return 0x0000;
    if (intensity >= 1) return color;
    
    uint16_t r = (color >> 11) & 0x1F;
    uint16_t g = (color >> 5) & 0x3F;
    uint16_t b = color & 0x1F;
    
    r = (uint16_t)(r * intensity);
    g = (uint16_t)(g * intensity);
    b = (uint16_t)(b * intensity);
    
    return (r << 11) | (g << 5) | b;
  }
};