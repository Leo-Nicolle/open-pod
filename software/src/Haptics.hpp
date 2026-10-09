#pragma once
#include "Adafruit_DRV2605.h"
#include <Arduino.h>
#include <Wire.h>

// Click-wheel haptics on a DRV2605L driving a 10mm coin ERM.
//
// Pulses are the chip's built-in ERM library effects (TI DRV2605L datasheet,
// table 11.2), which already carry their own overdrive kick and active brake,
// and are timed by the chip itself: play() is one or two register writes and
// returns immediately. This is deliberate. Hand-shaped RTP pulses timed from
// loop() (see main_haptic.cpp) felt great on the bench, but any long loop()
// iteration - a volume-overlay redraw, the Now Playing transition, an SD
// refill burst - stretched the kick or brake into the motor spinning flat
// out. A chip-timed effect always stops on time, whatever the MCU is doing.
//
// Pulses never overlap: while the chip is still playing (GO bit set), new
// requests are dropped.
class Haptics {
public:
  struct Effect {
    uint8_t id;      // ERM library effect, 1..123
    // Lengthen (or, negative, shorten) the effect's overdrive (kick) and
    // brake parts, in 5ms steps (ODT/BRT time offset registers).
    int8_t kickOffset;
    int8_t brakeOffset;
  };

  // Strong Click 100%: one strong thunk per click when scrolling fast, where
  // clicks get dropped and each one that plays has to stand out.
  static constexpr Effect SCROLL_FAST = {1, 0, 0};
  // Medium Click 1 100%: lighter detent for slow, deliberate scrolling.
  static constexpr Effect SCROLL_SLOW = {21, 0, 0};
  // Double Click 100%: distinct double bump for button presses.
  static constexpr Effect BUTTON = {10, 0, 0};

  // Scroll clicks closer than this (or several clicks in one event) count as
  // fast scrolling.
  static const unsigned long FAST_SCROLL_INTERVAL_MS = 80;

  bool begin(TwoWire *wire) {
    if (!drv.begin(wire)) return false; // always at 0x5A (DRV2605_ADDR)
    drv.useERM();
    drv.selectLibrary(1); // ERM library A: 10mm coin rise/brake times
    // Full-scale drive voltage, ~21.3 mV/step in ERM open loop. The chip
    // default (0x8C, ~3.0V) caps every effect below the 3.3V supply; 0xFF
    // lets it reach the rail, fine for short pulses on a ~3V coin ERM.
    drv.writeRegister8(REG_OD_CLAMP, 0xFF);
    drv.setMode(DRV2605_MODE_INTTRIG);
    drv.setWaveform(1, 0); // every sequence is a single effect
    ready = true;
    return true;
  }

  // One Scroll event of `clicks` (signed) clicks.
  void scrollTick(int clicks) {
    unsigned long now = millis();
    bool fast = abs(clicks) > 1 || now - lastScrollTick < FAST_SCROLL_INTERVAL_MS;
    lastScrollTick = now;
    play(fast ? SCROLL_FAST : SCROLL_SLOW);
  }

  void buttonClick() { play(BUTTON); }

  // Starts an effect unless the previous one is still playing.
  void play(const Effect &e) {
    if (!ready || (drv.readRegister8(DRV2605_REG_GO) & 0x01)) return;
    if (e.kickOffset != kickOffset) {
      kickOffset = e.kickOffset;
      drv.writeRegister8(DRV2605_REG_OVERDRIVE, (uint8_t)kickOffset);
    }
    if (e.brakeOffset != brakeOffset) {
      brakeOffset = e.brakeOffset;
      drv.writeRegister8(DRV2605_REG_BREAK, (uint8_t)brakeOffset);
    }
    drv.setWaveform(0, e.id);
    drv.go();
  }

private:
  static const uint8_t REG_OD_CLAMP = 0x17;

  Adafruit_DRV2605 drv;
  bool ready = false;
  int8_t kickOffset = 0;  // as written by drv.begin()
  int8_t brakeOffset = 0;
  unsigned long lastScrollTick = 0;
};
