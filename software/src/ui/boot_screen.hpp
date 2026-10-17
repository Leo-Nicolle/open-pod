#pragma once
// Title screen, "Drop & hop" intro (design: docs/ui-builder, Title screen).
//
// setup() is single-threaded, so the intro is driven by tick() and the boot
// init runs in its idle gaps through runStep(): a step waits for the next gap
// so it never freezes a movement halfway, and a step that overruns into the
// next movement makes it jump ahead (frames are computed from the clock, not
// counted). finish() holds the final frame until init is done, then wipes into
// the menu one chunk every BOOT_CHUNK_MS, also waiting on an optional hold
// condition (the startup sound). Any button press skips to the final frame.
//
// frameAt() mirrors bootFrame() in the ui-builder's logo.ts; the numbers come
// from logo.h, which the builder exports.

#include "pinout.h"
#include "ui/logo.h"
#include <Arduino.h>
#include <math.h>

class BootScreen {
public:
  explicit BootScreen(ILI9341_GFX *d) : display(d) {}

  void begin() {
    // Same wiring as ClickWheel::setButtonPins: pull-ups, active low. The
    // wheel takes the pins over (with its interrupts) after finish().
    for (int pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);
    display->fillScreen(OPENTONE_BOOT_BG);
    t0 = millis();
  }

  // Called on every tick, e.g. to keep the audio fifo topped up.
  void setIdle(void (*fn)()) { idle = fn; }

  // A button press skipped the intro.
  bool wasSkipped() const { return skipped; }

  // Draw what changed since the last tick.
  void tick() {
    if (idle) idle();
    if (!skipped && anyButtonDown()) skipped = true;
    Frame f = frameAt(now());
    drawLogo(f);
    while (lettersDrawn < f.letters) drawWordmarkGlyph(display, lettersDrawn++);
  }

  // Run one init step at the next idle gap of the intro.
  template <typename Step> void runStep(Step step) {
    while (!inIdleGap(now())) tick();
    step();
    tick();
  }

  // Finish the intro and wait while holdWhile() is true, then paint the
  // menu's first frame chunk by chunk.
  template <typename UI> void finish(UI &ui, bool (*holdWhile)() = nullptr) {
    while (now() < BOOT_WIPE_AT || (holdWhile && holdWhile())) tick();
    tick();
    uint32_t wipeStart = millis();
    for (int k = 0; k < WIPE_CHUNKS; ++k) {
      while (millis() - wipeStart < (uint32_t)(k * BOOT_CHUNK_MS)) {
      }
      ui.renderBootChunk(k);
    }
    ui.endBoot();
  }

  // Wait (bounded) until no button is held, so a press that skipped the intro
  // doesn't reach the menu as a click.
  void waitForRelease(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (anyButtonDown() && millis() - start < timeoutMs) delay(5);
    delay(30); // let the contacts settle before the wheel attaches its ISRs
  }

private:
  static constexpr int BUTTON_PINS[5] = {BTN_CENTER, BTN_TOP, BTN_RIGHT,
                                         BTN_BOTTOM, BTN_LEFT};
  static const int WIPE_CHUNKS = 8;
  static const int GLYPHS = OPENTONE_WORD_GLYPHS;

  enum Logo : uint8_t { NONE, NORMAL, SQUASH };
  struct Frame {
    Logo logo;
    int dy; // offset of the normal logo from OPENTONE_LOGO_Y
    int letters;
  };
  struct Rect {
    int x, y, w, h;
  };

  ILI9341_GFX *display;
  void (*idle)() = nullptr;
  uint32_t t0 = 0;
  bool skipped = false;
  int lettersDrawn = 0;
  Logo drawnLogo = NONE;
  int drawnDy = 0;
  Rect drawnRect = {0, 0, 0, 0};

  // Intro time in ms; a skip jumps straight to the final frame.
  uint32_t now() const {
    uint32_t t = millis() - t0;
    return skipped && t < BOOT_WIPE_AT ? BOOT_WIPE_AT : t;
  }

  static bool anyButtonDown() {
    for (int pin : BUTTON_PINS)
      if (digitalRead(pin) == LOW) return true;
    return false;
  }

  static bool inIdleGap(uint32_t t) {
    const uint32_t lastLetter =
        BOOT_LETTER_START + (GLYPHS - 1) * BOOT_LETTER_STEP;
    return t < BOOT_DROP_START ||
           (t >= BOOT_SQUASH_END && t < BOOT_LETTER_START) ||
           (t >= lastLetter && t < BOOT_HOP_START) || t >= BOOT_HOP_END;
  }

  static float seg(uint32_t t, uint32_t a, uint32_t b) {
    if (t <= a) return 0.0f;
    if (t >= b) return 1.0f;
    return (float)(t - a) / (float)(b - a);
  }

  static int roundHalfUp(float v) { return (int)floorf(v + 0.5f); }

  static Frame frameAt(uint32_t t) {
    Frame f = {t < BOOT_DROP_START ? NONE : NORMAL, 0, 0};
    if (t >= BOOT_DROP_START && t < BOOT_DROP_END) {
      float q = 1.0f - seg(t, BOOT_DROP_START, BOOT_DROP_END);
      f.dy = roundHalfUp(-BOOT_DROP_PX * q * q); // ease-out drop
    } else if (t >= BOOT_DROP_END && t < BOOT_SQUASH_END) {
      f.logo = SQUASH;
    } else if (t >= BOOT_HOP_START && t < BOOT_HOP_END) {
      f.dy = roundHalfUp(-BOOT_HOP_PX *
                         sinf((float)M_PI * seg(t, BOOT_HOP_START, BOOT_HOP_END)));
    }
    if (t >= BOOT_LETTER_START) {
      int n = (t - BOOT_LETTER_START) / BOOT_LETTER_STEP + 1;
      f.letters = n < GLYPHS ? n : GLYPHS;
    }
    return f;
  }

  // Redraw the logo if it moved or changed frame: one pass over the union of
  // the old and new rects clears the old pixels and draws the new ones.
  void drawLogo(const Frame &f) {
    if (f.logo == drawnLogo && (f.logo != NORMAL || f.dy == drawnDy)) return;
    if (f.logo == NONE) return;
    const LogoImage &img = f.logo == SQUASH ? OPENTONE_LOGO_SQUASH : OPENTONE_LOGO;
    Rect r = f.logo == SQUASH
                 ? Rect{OPENTONE_SQUASH_X, OPENTONE_SQUASH_Y, img.w, img.h}
                 : Rect{OPENTONE_LOGO_X, OPENTONE_LOGO_Y + f.dy, img.w, img.h};
    Rect u = r;
    if (drawnLogo != NONE) {
      int x1 = max(r.x + r.w, drawnRect.x + drawnRect.w);
      int y1 = max(r.y + r.h, drawnRect.y + drawnRect.h);
      u.x = min(r.x, drawnRect.x);
      u.y = min(r.y, drawnRect.y);
      u.w = x1 - u.x;
      u.h = y1 - u.y;
    }
    blitLogoImage(display, img, r.x, r.y, u.x, u.y, u.w, u.h, OPENTONE_BOOT_BG);
    drawnLogo = f.logo;
    drawnDy = f.dy;
    drawnRect = r;
  }
};
