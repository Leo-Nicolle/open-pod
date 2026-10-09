#pragma once
// Cooperative top-up point for long main-loop work (list loading, album-art
// reads). The feeder interrupt plays from the L1 fifo, but only the main
// loop refills it from PSRAM: a 200+ ms UI operation can drain the fifo
// (32 KB is ~280 ms of a 113 KB/s FLAC, ~180 ms at 178 KB/s). Calling
// audioYield() every few iterations of such a loop tops it up; it returns
// right away when the fifo has no room for a chunk, so it's cheap to call
// often.
//
// Never call it from inside a PSRAM transfer or with the SPI1 bus lock
// held. The hook is installed by AudioPlayer::begin(); unset (native
// tests, before begin()), audioYield() does nothing.

inline void (*&audioYieldHook())() {
  static void (*hook)() = nullptr;
  return hook;
}

inline void audioYield() {
  void (*hook)() = audioYieldHook();
  if (hook) hook();
}
