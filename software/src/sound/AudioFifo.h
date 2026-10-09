#pragma once
// L1 audio buffer in MCU RAM, between the PSRAM ring buffer and the VS1053.
//
// Single producer (main loop: SD or PSRAM -> here) and single consumer (the
// feeder timer interrupt: here -> VS1053 SDI). Lock-free: the producer only
// ever writes `head`, the consumer only `tail`; both are free-running
// counters, so size = head - tail even across 32-bit wrap (CAPACITY is a
// power of two).
//
// reset() is only safe while the consumer is stopped (feeder disabled).

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class AudioFifo {
public:
  // ~350 ms of CD-quality FLAC, ~6 s of 320 kbps MP3.
  enum : uint32_t { CAPACITY = 32 * 1024 };

  void reset() {
    head = 0;
    tail = 0;
  }

  uint32_t size() const { return head - tail; }
  uint32_t freeSpace() const { return CAPACITY - size(); }

  // --- producer side (main loop) ---

  // Where the next bytes go; `contig` gets how many fit before the wrap.
  uint8_t *writePtr(uint32_t &contig) {
    uint32_t index = head & MASK;
    uint32_t free = freeSpace();
    contig = (CAPACITY - index < free) ? CAPACITY - index : free;
    return data + index;
  }
  // Publishes `n` bytes written through writePtr().
  void commit(uint32_t n) {
    barrier(); // data stores before the head update
    head += n;
  }
  // Copies as much of `src` as fits; returns the bytes taken.
  uint32_t write(const uint8_t *src, uint32_t len) {
    uint32_t done = 0;
    while (done < len) {
      uint32_t contig;
      uint8_t *dst = writePtr(contig);
      if (contig == 0) break;
      uint32_t n = (len - done < contig) ? len - done : contig;
      memcpy(dst, src + done, n);
      commit(n);
      done += n;
    }
    return done;
  }
  // Appends `len` copies of `value` (VS1053 end fill bytes); returns the
  // bytes taken.
  uint32_t fill(uint8_t value, uint32_t len) {
    uint32_t done = 0;
    while (done < len) {
      uint32_t contig;
      uint8_t *dst = writePtr(contig);
      if (contig == 0) break;
      uint32_t n = (len - done < contig) ? len - done : contig;
      memset(dst, value, n);
      commit(n);
      done += n;
    }
    return done;
  }

  // --- consumer side (feeder interrupt) ---

  // Oldest unread bytes; `contig` gets how many are readable before the wrap.
  const uint8_t *readPtr(uint32_t &contig) const {
    uint32_t index = tail & MASK;
    uint32_t avail = size();
    contig = (CAPACITY - index < avail) ? CAPACITY - index : avail;
    return data + index;
  }
  void consume(uint32_t n) {
    barrier(); // data reads before the tail update
    tail += n;
  }

  // Raw storage, for borrowing as scratch while nothing plays (link bench).
  uint8_t *storage() { return data; }

private:
  enum : uint32_t { MASK = CAPACITY - 1 };
  static_assert((CAPACITY & MASK) == 0, "CAPACITY must be a power of two");

  static inline void barrier() { __asm__ volatile("" ::: "memory"); }

  uint8_t data[CAPACITY];
  volatile uint32_t head = 0; // written by the producer only
  volatile uint32_t tail = 0; // written by the consumer only
};
