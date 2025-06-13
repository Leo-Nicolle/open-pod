#pragma once
#include "../rendering/font_renderer.h"
#include "theme.h"
#include "ui_types.h"
#include <Arduino.h>

// Binary track renderer for fast caching with multi-bit support
class TrackRenderer {
  friend class TracksCache;
  friend class BitOperationsTestFixture; // Allow test fixture access
#ifdef UNIT_TEST
public:
#else
private:
#endif
  const FastFont &font;
  static const int MARGIN_LEFT = 5;
  int bitsPerPixel;

public:
  TrackRenderer(const FastFont &mainFont, int bpp = 1)
      : font(mainFont), bitsPerPixel(bpp) {
    // Validate bits per pixel
    if (bpp != 1 && bpp != 2 && bpp != 4) {
      bitsPerPixel = 1; // Default to 1 if invalid
    }
  }

  // Render track to binary bitmap with multi-bit support
  void renderTrackBinary(const char *title, uint8_t *binaryBuffer,
                         int bufferWidth, int bufferHeight, int x = 0,
                         int width = SCREEN_WIDTH) {

    // Clear the binary buffer first
    int totalBits = bufferWidth * bufferHeight * bitsPerPixel;
    int totalBytes = (totalBits + 7) / 8;
    memset(binaryBuffer, 0, totalBytes);

    // Set up font renderer to use a temporary pixel buffer
    uint16_t *tempPixelBuffer = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(tempPixelBuffer, width, bufferHeight);

    // Clear temp buffer to background color
    for (int i = 0; i < bufferHeight * width; i++) {
      tempPixelBuffer[i] = COLOR_BACKGROUND;
    }

    // Find visible portion (same logic as original)
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = fontRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;

      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1) {
          startChar = i;
        }
        endChar = i + 1;
      } else if (startChar != -1) {
        break;
      }

      currentWidth += charWidth;
      if (charStartX > x + width) {
        break;
      }
    }

    // Render visible text to temp buffer
    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + fontRenderer.measureText(upToStart, font) - x;

      // Render to temp buffer with contrasting colors
      fontRenderer.renderText(toRender, offsetX, 10, font, COLOR_TEXT,
                              COLOR_BACKGROUND);
    }

    // Convert pixel buffer to binary bitmap
    convertPixelsToBinary(tempPixelBuffer, binaryBuffer, width, bufferHeight,
                          bufferWidth);
  }

  // Render binary bitmap to display with transparency support
  void renderBinaryToDisplay(uint8_t *binaryBuffer, int bufferWidth,
                             int bufferHeight, uint16_t *displayBuffer,
                             int displayWidth, int displayHeight,
                             uint16_t textColor, uint16_t backgroundColor,
                             bool selected = false) {

    uint16_t baseColor = selected ? COLOR_PRIMARY : backgroundColor;

    for (int y = 0; y < bufferHeight && y < displayHeight; y++) {
      for (int x = 0; x < bufferWidth && x < displayWidth; x++) {
        uint8_t pixelValue = getBinaryPixel(binaryBuffer, x, y, bufferWidth);
        uint16_t finalColor = blendColor(baseColor, textColor, pixelValue);

        // Apply gradient if selected
        if (selected) {
          finalColor = getGradientColor(finalColor, y, bufferHeight);
        }

        int displayIndex = y * displayWidth + x;
        if (displayIndex < displayWidth * displayHeight) {
          displayBuffer[displayIndex] = finalColor;
        }
      }
    }
  }

  // Original color rendering method (keep for compatibility)
  void renderTrack(const char *title, bool selected, int x = 0,
                   int width = SCREEN_WIDTH) {
    uint16_t *buf = g_buffers.getCurrentBuffer();
    fontRenderer.setBuffer(buf, width, CHUNK_HEIGHT);

    uint16_t baseColor = selected ? COLOR_PRIMARY : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    // Fill background
    if (selected) {
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        uint16_t gradientColor = getGradientColor(baseColor, y, CHUNK_HEIGHT);
        int rowStart = y * width;
        for (int px = 0; px < width; px++) {
          buf[rowStart + px] = gradientColor;
        }
      }
    } else {
      for (int i = 0; i < CHUNK_HEIGHT * width; i++) {
        buf[i] = COLOR_BACKGROUND;
      }
    }

    // Render text (same logic as before)
    int startChar = -1;
    int endChar = -1;
    int currentWidth = 0;
    int textStartX = MARGIN_LEFT;

    for (int i = 0; title[i] != '\0' && i < 64; i++) {
      char tempChar[2] = {title[i], '\0'};
      int charWidth = fontRenderer.measureText(tempChar, font);

      int charStartX = textStartX + currentWidth;
      int charEndX = charStartX + charWidth;
      bool charVisible = (charEndX > x) && (charStartX < x + width);

      if (charVisible) {
        if (startChar == -1)
          startChar = i;
        endChar = i + 1;
      } else if (startChar != -1) {
        break;
      }

      currentWidth += charWidth;
      if (charStartX > x + width)
        break;
    }

    if (startChar != -1) {
      char toRender[64];
      int visibleLen = endChar - startChar;
      strncpy(toRender, title + startChar, visibleLen);
      toRender[visibleLen] = '\0';

      char upToStart[64];
      strncpy(upToStart, title, startChar);
      upToStart[startChar] = '\0';
      int offsetX = textStartX + fontRenderer.measureText(upToStart, font) - x;

      fontRenderer.renderText(toRender, offsetX, 10, font, textColor,
                              baseColor);
    }
  }
#ifdef UNIT_TEST
public: // Make these public during testing
#else
private:
#endif
        // Convert rendered pixels to binary bitmap with optimized bit packing
  void convertPixelsToBinary(uint16_t *pixels, uint8_t *binaryBuffer,
                             int pixelWidth, int pixelHeight, int binaryWidth) {
    for (int y = 0; y < pixelHeight && y < binaryWidth; y++) {
      for (int x = 0; x < pixelWidth && x < binaryWidth; x++) {
        int pixelIndex = y * pixelWidth + x;
        uint8_t shade = getShadeFromColor(pixels[pixelIndex]);
        setBinaryPixel(binaryBuffer, x, y, binaryWidth, shade);
      }
    }
  }

  // Optimized pixel setting with proper bit manipulation
  void setBinaryPixel(uint8_t *buffer, int x, int y, int width, uint8_t value) {
    // Calculate bit position
    int totalBitIndex = (y * width + x) * bitsPerPixel;
    int byteIndex = totalBitIndex >> 3; // Divide by 8 (faster than /)
    int bitOffset = totalBitIndex & 7;  // Modulo 8 (faster than %)

    // Create mask for the bits we want to modify
    uint8_t maxValue = (1 << bitsPerPixel) - 1;
    value &= maxValue; // Ensure value fits in our bit range

    // Handle case where bits don't cross byte boundary
    if (bitOffset + bitsPerPixel <= 8) {
      uint8_t mask = maxValue << bitOffset;
      buffer[byteIndex] = (buffer[byteIndex] & ~mask) | (value << bitOffset);
    } else {
      // Handle crossing byte boundary
      int bitsInFirstByte = 8 - bitOffset;
      int bitsInSecondByte = bitsPerPixel - bitsInFirstByte;

      // First byte
      uint8_t firstMask = ((1 << bitsInFirstByte) - 1) << bitOffset;
      uint8_t firstValue = (value & ((1 << bitsInFirstByte) - 1)) << bitOffset;
      buffer[byteIndex] = (buffer[byteIndex] & ~firstMask) | firstValue;

      // Second byte
      uint8_t secondMask = (1 << bitsInSecondByte) - 1;
      uint8_t secondValue = value >> bitsInFirstByte;
      buffer[byteIndex + 1] =
          (buffer[byteIndex + 1] & ~secondMask) | secondValue;
    }
  }

  // Optimized pixel reading with proper bit manipulation
  uint8_t getBinaryPixel(uint8_t *buffer, int x, int y, int width) {
    // Calculate bit position
    int totalBitIndex = (y * width + x) * bitsPerPixel;
    int byteIndex = totalBitIndex >> 3; // Divide by 8
    int bitOffset = totalBitIndex & 7;  // Modulo 8

    uint8_t maxValue = (1 << bitsPerPixel) - 1;

    // Handle case where bits don't cross byte boundary
    if (bitOffset + bitsPerPixel <= 8) {
      return (buffer[byteIndex] >> bitOffset) & maxValue;
    } else {
      // Handle crossing byte boundary
      int bitsInFirstByte = 8 - bitOffset;
      int bitsInSecondByte = bitsPerPixel - bitsInFirstByte;

      uint8_t firstPart =
          (buffer[byteIndex] >> bitOffset) & ((1 << bitsInFirstByte) - 1);
      uint8_t secondPart =
          buffer[byteIndex + 1] & ((1 << bitsInSecondByte) - 1);

      return firstPart | (secondPart << bitsInFirstByte);
    }
  }

  // Improved color to shade mapping with better anti-aliasing detection
  uint8_t getShadeFromColor(uint16_t color) {
    if (bitsPerPixel == 1) {
      // Simple binary: background or text
      return (color != COLOR_BACKGROUND) ? 1 : 0;
    }

    // For multi-bit modes, analyze color similarity for anti-aliasing
    uint8_t maxShade = (1 << bitsPerPixel) - 1;

    if (color == COLOR_BACKGROUND)
      return 0;
    if (color == COLOR_TEXT)
      return maxShade;

    // Calculate luminance-based transparency for anti-aliased pixels
    float bgLuma = getColorLuminance(COLOR_BACKGROUND);
    float textLuma = getColorLuminance(COLOR_TEXT);
    float pixelLuma = getColorLuminance(color);

    // Interpolate between background and text based on luminance
    float ratio;
    if (textLuma != bgLuma) {
      ratio = (pixelLuma - bgLuma) / (textLuma - bgLuma);
      ratio = constrain(ratio, 0.0f, 1.0f);
    } else {
      ratio = (color == COLOR_TEXT) ? 1.0f : 0.0f;
    }

    return (uint8_t)(ratio * maxShade + 0.5f); // Round to nearest
  }

  // Calculate luminance of RGB565 color
  float getColorLuminance(uint16_t color) {
    // Extract RGB components from RGB565
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    // Convert to 8-bit values
    float r8 = (r * 255.0f) / 31.0f;
    float g8 = (g * 255.0f) / 63.0f;
    float b8 = (b * 255.0f) / 31.0f;

    // Calculate luminance using standard coefficients
    return (0.299f * r8 + 0.587f * g8 + 0.114f * b8) / 255.0f;
  }

  // Blend colors based on transparency value
  uint16_t blendColor(uint16_t backgroundColor, uint16_t textColor,
                      uint8_t transparency) {
    if (transparency == 0)
      return backgroundColor;

    uint8_t maxTransparency = (1 << bitsPerPixel) - 1;
    if (transparency >= maxTransparency)
      return textColor;

    // Extract RGB565 components
    uint8_t bgR = (backgroundColor >> 11) & 0x1F;
    uint8_t bgG = (backgroundColor >> 5) & 0x3F;
    uint8_t bgB = backgroundColor & 0x1F;

    uint8_t textR = (textColor >> 11) & 0x1F;
    uint8_t textG = (textColor >> 5) & 0x3F;
    uint8_t textB = textColor & 0x1F;

    // Blend components
    float alpha = (float)transparency / maxTransparency;
    uint8_t blendR = (uint8_t)(bgR + alpha * (textR - bgR) + 0.5f);
    uint8_t blendG = (uint8_t)(bgG + alpha * (textG - bgG) + 0.5f);
    uint8_t blendB = (uint8_t)(bgB + alpha * (textB - bgB) + 0.5f);

    // Clamp values to valid ranges
    blendR = constrain(blendR, 0, 31);
    blendG = constrain(blendG, 0, 63);
    blendB = constrain(blendB, 0, 31);

    return (blendR << 11) | (blendG << 5) | blendB;
  }

  // Generate gradient color (keep for color rendering)
  uint16_t getGradientColor(uint16_t baseColor, int y, int totalHeight) {
    float progress = (float)y / (totalHeight - 1);
    uint8_t r = (baseColor >> 11) & 0x1F;
    uint8_t g = (baseColor >> 5) & 0x3F;
    uint8_t b = baseColor & 0x1F;

    float brightnessMod = 1.0f + 0.04f * (0.5f - progress);
    int newR = constrain((int)(r * brightnessMod), 0, 31);
    int newG = constrain((int)(g * brightnessMod), 0, 63);
    int newB = constrain((int)(b * brightnessMod), 0, 31);

    return (newR << 11) | (newG << 5) | newB;
  }

  // Utility function to calculate buffer size needed
  static int calculateBufferSize(int width, int height, int bpp) {
    int totalBits = width * height * bpp;
    return (totalBits + 7) / 8; // Round up to nearest byte
  }

  // Change bits per pixel (useful for testing different modes)
  void setBitsPerPixel(int bpp) {
    if (bpp == 1 || bpp == 2 || bpp == 4) {
      bitsPerPixel = bpp;
    }
  }

  int getBitsPerPixel() const { return bitsPerPixel; }
};