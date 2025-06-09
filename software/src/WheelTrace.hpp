#pragma once
#include "rendering/ILI9341_GFX.h"
#include <Arduino.h>
#include <math.h>

#define DEG_TO_RAD 0.0174532925

class WheelTrace {
public:
  static const int MAX_POINTS = 32; // Reduced from 64 for quicker fade
  static const uint16_t TRACE_RADIUS = 3;

  enum Pad { TOP = 0, RIGHT = 1, BOTTOM = 2, LEFT = 3 };

  struct TracePoint {
    float angle; // In degrees
    unsigned long timestamp; // Time when point was added
  };

  WheelTrace() : index(0), count(0), fadeTimeMs(1000) {} // 1 second fade

  void add(float angle) {
    points[index].angle = angle;
    points[index].timestamp = millis();
    index = (index + 1) % MAX_POINTS;
    if (count < MAX_POINTS)
      count++;
  }

  void draw(ILI9341_GFX &display, int cx, int cy, int r) {
    unsigned long currentTime = millis();
    
    display.drawFastHLine(cx - r, cy, 2 * r + 1, 0); // Draw horizontal line
    display.drawFastVLine(cx, cy - r, 2 * r + 1, 0); // Draw vertical line
    for (int i = 0; i < count; i++) {
      // Calculate age of this point
      unsigned long age = currentTime - points[i].timestamp;
      
      // Skip points that are too old
      if (age > fadeTimeMs) continue;
      
      float angleRad = points[i].angle * DEG_TO_RAD;
      int x = cx + cos(angleRad) * r;
      int y = cy - sin(angleRad) * r;

      Pad pad = padFromAngle(points[i].angle);
      uint16_t color = colorForPad(pad);
      
      // Apply fade effect based on age
      if (age > fadeTimeMs / 2) {
        // Fade to black in second half of lifetime
        float fadeRatio = 1.0 - (float)(age - fadeTimeMs/2) / (fadeTimeMs/2);
        color = fadeColor(color, fadeRatio);
      }
      
      display.fillCircle(x, y, TRACE_RADIUS, color);
    }
  }

  // Set how long points should persist (in milliseconds)
  void setFadeTime(unsigned long ms) {
    fadeTimeMs = ms;
  }

  // Clear all points immediately
  void clear() {
    count = 0;
    index = 0;
  }

  static Pad padFromAngle(float angle) {
    // angle ∈ [0, 360)
    if (angle < 45 || angle >= 315)
      return TOP; // 0° zone
    if (angle < 135)
      return RIGHT; // 90°
    if (angle < 225)
      return BOTTOM; // 180°
    return LEFT;     // 270°
  }

  static uint16_t colorForPad(Pad pad) {
    switch (pad) {
    case TOP:
      return 0x001F; // Blue
    case RIGHT:
      return 0x0000; // Black
    case BOTTOM:
      return 0xF800; // Red
    case LEFT:
      return 0x07E0; // Green
    default:
      return 0xFFFF; // White (fallback)
    }
  }

private:
  TracePoint points[MAX_POINTS];
  int index;
  int count;
  unsigned long fadeTimeMs;

  // Simple color fading - reduces intensity
  uint16_t fadeColor(uint16_t color, float intensity) {
    if (intensity <= 0) return 0x0000; // Black
    if (intensity >= 1) return color;
    
    // Extract RGB components from 565 format
    uint16_t r = (color >> 11) & 0x1F;
    uint16_t g = (color >> 5) & 0x3F;
    uint16_t b = color & 0x1F;
    
    // Apply fade
    r = (uint16_t)(r * intensity);
    g = (uint16_t)(g * intensity);
    b = (uint16_t)(b * intensity);
    
    // Recombine
    return (r << 11) | (g << 5) | b;
  }
};