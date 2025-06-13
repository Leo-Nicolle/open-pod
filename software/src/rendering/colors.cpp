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

float adjustedAlpha(uint8_t alpha, bool darkBackground) {
  if (!darkBackground || alpha == 0 || alpha == 255)
    return alpha / 255.0f;
  return std::min(1.0f, (alpha * alpha) / 180.0f / 255.0f);
}
uint16_t blendColor(uint16_t bgColor, uint16_t fgColor, float alpha) {
  // Extract RGB565 components
  uint8_t bgR = (bgColor >> 11) & 0x1F;
  uint8_t bgG = (bgColor >> 5) & 0x3F;
  uint8_t bgB = bgColor & 0x1F;

  uint8_t fgR = (fgColor >> 11) & 0x1F;
  uint8_t fgG = (fgColor >> 5) & 0x3F;
  uint8_t fgB = fgColor & 0x1F;

  // Apply gamma correction
  float a = powf(alpha, 1.8f);

  // Blend components with full float precision
  int blendR = roundf((1.0f - a) * bgR + a * fgR);
  int blendG = roundf((1.0f - a) * bgG + a * fgG);
  int blendB = roundf((1.0f - a) * bgB + a * fgB);

  // Clamp to RGB565 valid ranges
  blendR = constrain(blendR, 0, 31);
  blendG = constrain(blendG, 0, 63);
  blendB = constrain(blendB, 0, 31);

  return (blendR << 11) | (blendG << 5) | blendB;
}
