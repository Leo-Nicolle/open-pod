#pragma once
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