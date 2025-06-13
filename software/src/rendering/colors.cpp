#include "colors.h"

bool isColorDark(uint16_t color) {
  // Extract RGB and calculate luminance
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;

  // Simple luminance calculation (you could use a more sophisticated one)
  int luminance = r * 2 + g + b; // Rough approximation
  return luminance < 40;         // Adjust threshold as needed
}


float computeGamma(float alpha, bool darkBackground) {
  if (alpha <= 0.0f) return 0.0f;
  if (alpha >= 1.0f) return 1.0f;

  if (darkBackground) {
    // Alpha curve that boosts edge contrast on dark
    return std::min(1.0f, (alpha * alpha + alpha) * 0.5f); // empirical
  } else {
    return powf(alpha, 1.8f);
  }
}
uint16_t blendColor(uint16_t bgColor, uint16_t fgColor, float alpha) {
  uint8_t bgR = (bgColor >> 11) & 0x1F;
  uint8_t bgG = (bgColor >> 5) & 0x3F;
  uint8_t bgB = bgColor & 0x1F;

  uint8_t fgR = (fgColor >> 11) & 0x1F;
  uint8_t fgG = (fgColor >> 5) & 0x3F;
  uint8_t fgB = fgColor & 0x1F;

  bool darkBackground = isColorDark(bgColor);
  float a = alpha;
  if (darkBackground) {
    a = std::min(1.0f, (a * a) / 0.75f); // Better boost on dark
  } else {
    a = powf(a, 1.8f);
  }

  int blendR = roundf((1.0f - a) * bgR + a * fgR);
  int blendG = roundf((1.0f - a) * bgG + a * fgG);
  int blendB = roundf((1.0f - a) * bgB + a * fgB);

  return ((constrain(blendR, 0, 31) << 11) |
          (constrain(blendG, 0, 63) << 5) |
          constrain(blendB, 0, 31));
}
