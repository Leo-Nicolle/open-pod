#pragma once
// SPI1 is shared by the SD card and the VS1053 (Adafruit breakout wiring),
// and the VS1053 is fed from a timer interrupt (AudioPlayer's feeder). Any
// main-loop code that talks on SPI1 holds this lock for the whole exchange
// (chip select low through the last clock); the feeder skips its tick while
// it's held. The feeder itself never takes it: it only runs when it's free,
// and an interrupt runs to completion before the main loop resumes.
//
// Holders: SharedSpiDriver (every SdFat access), VS1053_driver (SCI/SDI from
// the main loop), the link bench.

#include <stdint.h>

namespace spi1bus {

extern volatile uint8_t lockCount;
// Set by the feeder when it skipped a tick because the bus was held; the
// last release() then calls onFreed (the feeder's "run now" kick) so it
// catches up right away instead of waiting for its next tick.
extern volatile bool feedPending;
extern void (*volatile onFreed)();

inline void acquire() { lockCount = lockCount + 1; }
inline void release() {
  lockCount = lockCount - 1;
  if (lockCount == 0 && feedPending) {
    feedPending = false;
    void (*kick)() = onFreed;
    if (kick) kick();
  }
}
inline bool busy() { return lockCount != 0; }

// Scoped hold.
struct Lock {
  Lock() { acquire(); }
  ~Lock() { release(); }
  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;
};

} // namespace spi1bus
