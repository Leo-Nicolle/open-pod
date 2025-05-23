

#pragma once
#include <SPI.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip

#define TFT_GREY 0x5AEB // New colour
#define TFT_GREY 0xBDF7

class PodScreen {
  float sx = 0, sy = 1, mx = 1, my = 0, hx = -1,
        hy = 0; // Saved H, M, S x & y multipliers
  float sdeg = 0, mdeg = 0, hdeg = 0;
  uint16_t osx = 64, osy = 64, omx = 64, omy = 64, ohx = 64,
           ohy = 64; // Saved H, M, S x & y coords
  uint16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0;
  uint32_t targetTime = 0; // for next 1 second timeout

  static uint8_t conv2d(const char *p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
      v = *p - '0';
    return 10 * v + *++p - '0';
  }

  uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3),
          ss = conv2d(__TIME__ + 6); // Get H, M, S from compile time

  bool initial = 1;

  TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
public:
  void setup(void);
  void loop(void);
};
