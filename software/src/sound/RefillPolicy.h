#pragma once
// When and how hard to refill the audio buffer from SD, as a pure function
// of the state AudioPlayer::loop() sees (no hardware, unit-tested natively).
//
// Everything is in time, not bytes: "runway" is how long the buffered audio
// (PSRAM ring + L1 fifo) lasts at the stream's rate, so the same thresholds
// work for 128 kbps MP3 (16 KB/s) and 24-bit FLAC (~150 KB/s).
//
// Priorities, highest first (the caller runs them in this order):
//   1. PSRAM -> L1 fifo top-up       always, every pass (no SD)
//   2. SD -> L1 fifo / PSRAM refill  per decide() below
//   3. next-track prefetch           only when idle and not refilling

#include <stdint.h>
#include <stddef.h>

namespace refill {

// A refill starts below this runway and runs until the PSRAM target is
// reached (or the file is fully read); the SD card is idle in between.
static const uint32_t LOW_RUNWAY_MS = 5000;
// Below this, refill passes get the full budget instead of the relaxed one.
static const uint32_t URGENT_RUNWAY_MS = 2500;
// Below this, the refill ignores user activity: audio first.
static const uint32_t CRITICAL_RUNWAY_MS = 700;

// While the user interacts (wheel input, screen transitions), a refill
// pass with runway between CRITICAL and URGENT reads exactly one chunk at
// most once per this interval (~200 KB/s with 4 KB chunks: above any FLAC
// rate, so the buffer still grows), and none at all above URGENT.
static const uint32_t BUSY_CHUNK_INTERVAL_MS = 20;
// Below this runway the trickle reads two chunks per slot instead of one:
// on a slow UI pass (40-70 ms while scrolling lists) one chunk per pass only
// just kept up with a 138 KB/s FLAC; heavier streams need the margin.
static const uint32_t BUSY_BOOST_RUNWAY_MS = 1500;

static const uint32_t FULL_BUDGET_US = 40000;   // 40 ms
static const uint32_t RELAXED_BUDGET_US = 8000; // ~1 chunk
static const uint32_t ONE_CHUNK_BUDGET_US = 1;  // load(): exactly one chunk
// Fits a second chunk at the usual ~8-12 ms per chunk, not a third.
static const uint32_t TWO_CHUNK_BUDGET_US = 25000;

// Used until the stream's rate is known: CD PCM, higher than any FLAC/MP3
// we play, so the runway is underestimated (refill early), never over.
static const uint32_t FALLBACK_BYTES_PER_SECOND = 176400;

inline uint32_t runwayMs(size_t bufferedBytes, uint32_t bytesPerSecond) {
  if (bytesPerSecond == 0) bytesPerSecond = FALLBACK_BYTES_PER_SECOND;
  return (uint32_t)((uint64_t)bufferedBytes * 1000ULL / bytesPerSecond);
}

struct Inputs {
  uint32_t runwayMs;
  bool refilling;          // a refill is in progress (latched)
  bool targetReached;      // PSRAM target reached, or the file fully read
  bool userBusy;           // user input / transition in the last holdoff
  uint32_t msSinceBusyChunk; // since the last one-chunk pass while busy
};

struct Decision {
  bool refilling;   // new latch state
  bool load;        // run a refill pass now
  uint32_t budgetUs; // its budget (when load)
  bool busyChunk;   // this pass is the one-chunk trickle (restart its timer)
  bool prefetch;    // spend this pass on the next-track prefetch instead
};

inline Decision decide(const Inputs &in) {
  Decision d = {in.refilling, false, 0, false, false};
  if (in.runwayMs < LOW_RUNWAY_MS) {
    d.refilling = true;
  }
  if (d.refilling && in.targetReached) {
    d.refilling = false;
  }
  if (!d.refilling) {
    d.prefetch = !in.userBusy;
    return d;
  }

  bool critical = in.runwayMs < CRITICAL_RUNWAY_MS;
  bool urgent = in.runwayMs < URGENT_RUNWAY_MS;
  if (in.userBusy && !critical) {
    if (!urgent || in.msSinceBusyChunk < BUSY_CHUNK_INTERVAL_MS) {
      return d; // wait: the user stops, or the next trickle slot
    }
    d.load = true;
    d.budgetUs = in.runwayMs < BUSY_BOOST_RUNWAY_MS ? TWO_CHUNK_BUDGET_US
                                                    : ONE_CHUNK_BUDGET_US;
    d.busyChunk = true;
    return d;
  }
  d.load = true;
  d.budgetUs = urgent ? FULL_BUDGET_US : RELAXED_BUDGET_US;
  return d;
}

} // namespace refill
