#pragma once
// Cycle-accurate timing of the audio data path, to see where the CPU time
// actually goes while streaming (SD reads, PSRAM traffic, VS1053 clocking vs
// waiting on DREQ, UI). Accumulates per category and prints one line per
// second when enabled; costs a couple of register reads per measured span.
//
// Native unit tests have no DWT and no use for this: everything compiles to
// no-ops there.

#include <Arduino.h>

namespace audioprof {

enum Category : uint8_t {
  SD_READ,      // SdFat reads of audio data
  PSRAM_WRITE,  // SPI_PSRAM::writeData (audio ring + music index)
  PSRAM_READ,   // SPI_PSRAM::readData
  VS_CLOCK,     // clocking bytes out to the VS1053 SDI
  VS_DREQ_WAIT, // busy-waiting for DREQ inside sendData
  AUDIO_LOOP,   // PodPlayer::loop() as a whole
  UI_UPDATE,    // ui.update() as a whole
  CATEGORY_COUNT
};

#ifdef UNIT_TEST

inline void begin() {}
inline uint32_t now() { return 0; }
inline void add(Category, uint32_t) {}
inline void addBytes(Category, uint32_t) {}
inline void markLoop() {}
inline void report() {}

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

inline void add(Category c, uint32_t startCycles) {
  uint32_t d = DWT->CYCCNT - startCycles;
  Slot &s = state().slots[c];
  s.cycles += d;
  s.count++;
  if (d > s.maxCycles) s.maxCycles = d;
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

// Prints and resets the window once a second. Time is reported as total ms
// spent in the window (= ‰ of CPU / 1000ms), worst single span in us, and
// throughput where bytes are counted.
inline void report() {
  State &st = state();
  unsigned long nowMs = millis();
  if (nowMs - st.lastReportMs < 1000) return;
  unsigned long windowMs = nowMs - st.lastReportMs;
  st.lastReportMs = nowMs;
  static const char *const names[CATEGORY_COUNT] = {
      "sd", "psW", "psR", "vsClk", "vsWait", "audio", "ui"};
  const uint32_t cyclesPerUs = SystemCoreClock / 1000000UL;
  Serial.printf("[prof %lums] loops=%lu maxLoop=%lums", windowMs,
                (unsigned long)st.loops,
                (unsigned long)(st.maxLoopCycles / cyclesPerUs / 1000));
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    Slot &s = st.slots[i];
    if (s.count == 0) continue;
    Serial.printf(" | %s %lums max%luus", names[i],
                  (unsigned long)(s.cycles / cyclesPerUs / 1000),
                  (unsigned long)(s.maxCycles / cyclesPerUs));
    if (s.bytes > 0) {
      Serial.printf(" %luKB", (unsigned long)(s.bytes / 1024));
    }
  }
  Serial.println();
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    st.slots[i] = Slot();
  }
  st.loops = 0;
  st.maxLoopCycles = 0;
}

#endif

} // namespace audioprof
