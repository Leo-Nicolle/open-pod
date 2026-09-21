// Minimal Arduino.h mock for native tests.
//
// Provides just enough of the Arduino API for the storage layer to compile and
// run on the host: integer types, min/max/constrain, pin functions, delay,
// and a no-op Serial object.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>

#include "fake_psram.h"

#ifndef HIGH
#define HIGH 0x1
#define LOW 0x0
#endif

#ifndef INPUT
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#endif

#ifndef HEX
#define HEX 16
#endif
#ifndef DEC
#define DEC 10
#endif
#ifndef BIN
#define BIN 2
#endif
#ifndef OCT
#define OCT 8
#endif

// min/max as templates (matching the STM32 Arduino core) so that std::min /
// std::max remain usable in headers that include <algorithm>.
template <class T, class L>
auto min(const T &a, const L &b) -> decltype((b < a) ? b : a) {
  return (b < a) ? b : a;
}

template <class T, class L>
auto max(const T &a, const L &b) -> decltype((a < b) ? b : a) {
  return (a < b) ? b : a;
}

#ifndef constrain
#define constrain(amt, low, high)                                              \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

// Minimal stand-in for Arduino's String class - just enough for production
// headers (e.g. ListCache::printCacheStats(), never actually called by the
// tests, but still compiled since it's a non-template inline member
// function) to build under the native test toolchain. No real string
// storage/formatting is needed since Serial.println() below is a no-op.
class String {
public:
  String() {}
  String(int) {}
  String(unsigned int) {}
  String(const char *) {}
  String operator+(const char *) const { return String(); }
  String operator+(const String &) const { return String(); }
};
inline String operator+(const char *, const String &) { return String(); }

namespace openpod_test {

class SerialClass {
public:
  void begin(unsigned long) {}
  void end() {}
  void flush() {}

  template <typename... Args> size_t print(Args &&...) { return 0; }
  template <typename... Args> size_t println(Args &&...) { return 0; }

  int printf(const char *, ...) { return 0; }
};

inline SerialClass &serial() {
  static SerialClass instance;
  return instance;
}

} // namespace openpod_test

// The Arduino API lives in the global namespace, matching the real Arduino
// core, so the storage layer can call it unqualified.
inline void pinMode(uint8_t, uint8_t) {}

inline void digitalWrite(uint8_t pin, uint8_t val) {
  // The only digitalWrite in the storage layer drives the PSRAM chip select
  // (PSRAM_CS == PB6, see src/pinout.h).
  if (pin == PB6) {
    openpod_test::fakePsram().select(val == LOW);
  }
}

inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
// Real elapsed wall time since the test process started, matching millis()'s
// semantics closely enough for wall-clock bounds (e.g. Audio_buffer's
// AUDIO_MAX_LOAD_MS) to compile and behave sanely under test: a handful of
// in-memory chunk reads/writes complete in microseconds, well under any of
// the firmware's millisecond-scale budgets, so those bounds stay inactive
// and chunk-count caps remain the operative limit in tests.
inline unsigned long millis() {
  static const auto start = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::steady_clock::now() - start;
  return (unsigned long)
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}
// Native tests are single-threaded with no ISRs, so these are no-ops -
// Audio_buffer brackets its PSRAM read/write calls with them to keep the
// real (interrupt-driven) firmware from tearing a multi-byte transfer.
inline void noInterrupts() {}
inline void interrupts() {}

// Make `Serial.println(...)` resolve to the mock singleton. This matches how
// the storage layer uses Serial and avoids a global object definition.
#define Serial (::openpod_test::serial())
