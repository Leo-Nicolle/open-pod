#include "screen.h"
void PodScreen::setup(void) {
  Serial.print("Screen setup");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_GREY);
  tft.setTextColor(TFT_GREEN, TFT_GREY); // Adding a black background colour
                                         // erases previous text automatically

  // Draw clock face
  tft.fillCircle(64, 64, 61, TFT_BLUE);
  tft.fillCircle(64, 64, 57, TFT_BLACK);

  // Draw 12 lines
  for (int i = 0; i < 360; i += 30) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 57 + 64;
    yy0 = sy * 57 + 64;
    x1 = sx * 50 + 64;
    yy1 = sy * 50 + 64;

    tft.drawLine(x0, yy0, x1, yy1, TFT_BLUE);
  }

  // Draw 60 dots
  for (int i = 0; i < 360; i += 6) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 53 + 64;
    yy0 = sy * 53 + 64;

    tft.drawPixel(x0, yy0, TFT_BLUE);
    if (i == 0 || i == 180)
      tft.fillCircle(x0, yy0, 1, TFT_CYAN);
    if (i == 0 || i == 180)
      tft.fillCircle(x0 + 1, yy0, 1, TFT_CYAN);
    if (i == 90 || i == 270)
      tft.fillCircle(x0, yy0, 1, TFT_CYAN);
    if (i == 90 || i == 270)
      tft.fillCircle(x0 + 1, yy0, 1, TFT_CYAN);
  }

  tft.fillCircle(65, 65, 3, TFT_RED);

  // Draw text at position 64,125 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters
  // [space] 0 1 2 3 4 5 6 7 8 9 : . a p m Font 7 is a 7 segment font and only
  // contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  tft.drawCentreString("Time flies", 64, 130, 4);

  targetTime = millis() + 1000;
  Serial.print("Screen end setup");
}

void PodScreen::loop() {
  Serial.print("loop");
  if (targetTime < millis()) {
    targetTime = millis() + 1000;
    ss++; // Advance second
    if (ss == 60) {
      ss = 0;
      mm++; // Advance minute
      if (mm > 59) {
        mm = 0;
        hh++; // Advance hour
        if (hh > 23) {
          hh = 0;
        }
      }
    }

    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss * 6;                     // 0-59 -> 0-354
    mdeg = mm * 6 + sdeg * 0.01666667; // 0-59 -> 0-360 - includes seconds
    hdeg = hh * 30 +
           mdeg * 0.0833333; // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg - 90) * 0.0174532925);
    hy = sin((hdeg - 90) * 0.0174532925);
    mx = cos((mdeg - 90) * 0.0174532925);
    my = sin((mdeg - 90) * 0.0174532925);
    sx = cos((sdeg - 90) * 0.0174532925);
    sy = sin((sdeg - 90) * 0.0174532925);

    if (ss == 0 || initial) {
      initial = 0;
      // Erase hour and minute hand positions every minute
      tft.drawLine(ohx, ohy, 65, 65, TFT_BLACK);
      ohx = hx * 33 + 65;
      ohy = hy * 33 + 65;
      tft.drawLine(omx, omy, 65, 65, TFT_BLACK);
      omx = mx * 44 + 65;
      omy = my * 44 + 65;
    }

    // Redraw new hand positions, hour and minute hands not erased here to avoid
    // flicker
    tft.drawLine(osx, osy, 65, 65, TFT_BLACK);
    tft.drawLine(ohx, ohy, 65, 65, TFT_WHITE);
    tft.drawLine(omx, omy, 65, 65, TFT_WHITE);
    osx = sx * 47 + 65;
    osy = sy * 47 + 65;
    tft.drawLine(osx, osy, 65, 65, TFT_RED);

    tft.fillCircle(65, 65, 3, TFT_RED);
  }
}