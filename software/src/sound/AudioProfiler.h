#pragma once
// Cycle-accurate timing of the audio data path, to see where the CPU time
// actually goes while streaming (SD reads, PSRAM traffic, VS1053 clocking vs
// waiting on DREQ, UI). Accumulates per category and prints one line per
// second when enabled; costs a couple of register reads per measured span.
//
// Native unit tests have no DWT and no use for this: everything compiles to
// no-ops there.

#include <Arduino.h>
#include "../logging.h"
#ifndef UNIT_TEST
#include "../app/stack_probe.h"
#endif

namespace audioprof {

enum Category : uint8_t {
  SD_READ,      // SdFat reads of audio data
  PSRAM_WRITE,  // SPI_PSRAM::writeData (audio ring + music index)
  PSRAM_READ,   // SPI_PSRAM::readData
  VS_CLOCK,     // clocking bytes out to the VS1053 SDI
  VS_DREQ_WAIT, // busy-waiting for DREQ inside sendData
  AUDIO_LOOP,   // PodPlayer::loop() as a whole
  UI_UPDATE,    // ui.update() as a whole
  FEED_ISR,     // feeder interrupt ticks that sent data
  // Top level of loop(), disjoint (Span): with UI_UPDATE and AUDIO_LOOP they
  // cover the loop; MAIN_LOOP minus their sum is what no span covers.
  WHEEL_UPDATE,   // wheel.update() (MPR121 over I2C, holds the SPI1 lock)
  INPUT_HANDLING, // handling wheel events, timeouts, idle checks
  BT_LOOP,        // Bluetooth module and its config
  BENCH_IO,       // console, bench commands' parsing and answers, monitor lines
  MISC_WORK,      // progress updates, trace, telemetry
  MAIN_LOOP,      // loop() as a whole
  CATEGORY_COUNT
};

#ifdef UNIT_TEST

inline void begin() {}
inline uint32_t now() { return 0; }
inline void add(Category, uint32_t) {}
inline void addBytes(Category, uint32_t) {}
inline void markLoop() {}
inline void report() {}
inline void moveTo(Category, uint32_t) {}
struct Span {
  explicit Span(Category) {}
};

#else

struct Slot {
  uint32_t cycles;
  uint32_t maxCycles;
  uint32_t count;
  uint32_t bytes;
};

struct State {
  Slot slots[CATEGORY_COUNT];
  uint32_t lastLoopCycles;
  uint32_t maxLoopCycles;
  uint32_t loops;
  unsigned long lastReportMs;
  uint32_t moved; // cycles moved out of the open Span (moveTo)
};

inline State &state() {
  static State s = {};
  return s;
}

// DWT cycle counter: 180 MHz on the F446, wraps every ~23 s, which is fine
// for spans and for the 1 s report window (unsigned subtraction).
inline void begin() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

inline uint32_t now() { return DWT->CYCCNT; }

inline void record(Category c, uint32_t d) {
  Slot &s = state().slots[c];
  s.cycles += d;
  s.count++;
  if (d > s.maxCycles) s.maxCycles = d;
}

inline void add(Category c, uint32_t startCycles) {
  record(c, DWT->CYCCNT - startCycles);
}

// Times a top-level step of loop() as `c`, minus whatever ran inside it was
// handed to another category with moveTo(). Spans don't nest.
struct Span {
  Category c;
  uint32_t start, moved0;
  explicit Span(Category cat)
      : c(cat), start(DWT->CYCCNT), moved0(state().moved) {}
  ~Span() {
    record(c, DWT->CYCCNT - start - (state().moved - moved0));
  }
};

// Inside a Span: counts the time since startCycles as `c` instead of the
// span's category (a bench command running a search is UI work; the audio
// monitor's log line is bench overhead).
inline void moveTo(Category c, uint32_t startCycles) {
  uint32_t d = DWT->CYCCNT - startCycles;
  record(c, d);
  state().moved += d;
}

inline void addBytes(Category c, uint32_t bytes) {
  state().slots[c].bytes += bytes;
}

// Call once per main-loop iteration: tracks the worst loop period.
inline void markLoop() {
  State &st = state();
  uint32_t t = DWT->CYCCNT;
  if (st.loops > 0) {
    uint32_t d = t - st.lastLoopCycles;
    if (d > st.maxLoopCycles) st.maxLoopCycles = d;
  }
  st.lastLoopCycles = t;
  st.loops++;
}

// Prints and resets the window once a second. Time is reported in us (ms
// truncated sub-ms spans and loops to 0): total time spent in the window
// (busy / window = share of CPU), worst single span, and throughput where
// bytes are counted. The top-level steps (Span) add up to the loop; the
// detail spans (sd/psW/psR/vsClk/vsWait) run inside audio or ui, and the
// feeder interrupt preempts whatever runs, so those overlap the rest.
// Call at the very end of loop(), outside every span: its own time (mostly
// formatting this line) is counted as BENCH_IO and MAIN_LOOP in the next window.
inline void report() {
  State &st = state();
  unsigned long nowMs = millis();
  if (nowMs - st.lastReportMs < 1000) return;
  uint32_t start = DWT->CYCCNT;
  unsigned long windowMs = nowMs - st.lastReportMs;
  st.lastReportMs = nowMs;
  static const char *const names[CATEGORY_COUNT] = {
      "sd",    "psW",   "psR", "vsClk", "vsWait", "audio", "ui",
      "isr",   "wheel", "input", "bt",  "bench", "misc",  "loop"};
  const uint32_t cyclesPerUs = SystemCoreClock / 1000000UL;
  // One line, built in pieces (report() only runs with LOG_AUDIO_BUFFER).
  LOG_RAW(LOG_AUDIO_BUFFER, "[prof %lums] loops=%lu maxLoop=%luus", windowMs,
          (unsigned long)st.loops,
          (unsigned long)(st.maxLoopCycles / cyclesPerUs));
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    Slot &s = st.slots[i];
    if (s.count == 0) continue;
    LOG_RAW(LOG_AUDIO_BUFFER, " | %s %luus max%luus", names[i],
            (unsigned long)(s.cycles / cyclesPerUs),
            (unsigned long)(s.maxCycles / cyclesPerUs));
    if (s.bytes > 0) {
      LOG_RAW(LOG_AUDIO_BUFFER, " %luKB", (unsigned long)(s.bytes / 1024));
    }
  }
  // Stack high-water mark (app/stack_probe.h): bytes left above the heap.
  LOG_RAW(LOG_AUDIO_BUFFER, " | stackFree %luB stackMax %luB\n",
          (unsigned long)stackFreeMin(), (unsigned long)stackUsedMax());
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    st.slots[i] = Slot();
  }
  st.loops = 0;
  st.maxLoopCycles = 0;
  uint32_t d = DWT->CYCCNT - start;
  record(BENCH_IO, d);
  record(MAIN_LOOP, d);
}

#endif

} // namespace audioprof
