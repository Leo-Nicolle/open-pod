// In-memory emulation of the APS6404 SPI PSRAM for native tests.
//
// The real SPI_PSRAM (src/storage/PSRAM_controller.hpp) drives a fake SPIClass
// (test/mocks/SPI.h) which forwards every byte to this device. The device
// interprets the APS6404 command protocol (RESET, READ_ID, WRITE, FAST_READ)
// and stores data in a plain byte buffer so tests can inspect what the
// firmware wrote to PSRAM and read it back.
#pragma once

#include <stdint.h>
#include <string.h>

namespace openpod_test {

class FakePsram {
public:
  static const uint32_t SIZE = 8u * 1024u * 1024u; // 8 MiB

private:
  uint8_t *mem;
  bool chipSelected; // true when CS is asserted (LOW)
  // Protocol state machine.
  uint8_t phase;     // 0=cmd,1=READ_ID dummy,2=WRITE addr,3=WRITE data,
                     // 4=READ_ID data,5=READ addr,6=READ dummy,7=READ data
  uint8_t cmd;
  uint32_t addr;
  uint32_t addrBytes;
  uint32_t idBytes;
  uint32_t readPos;

public:
  FakePsram() : mem(new uint8_t[SIZE]) { reset(); }
  ~FakePsram() { delete[] mem; }

  void reset() {
    memset(mem, 0, SIZE);
    chipSelected = false;
    phase = 0;
    cmd = 0;
    addr = 0;
    addrBytes = 0;
    idBytes = 0;
    readPos = 0;
  }

  // Called by the fake Arduino digitalWrite() when the CS pin toggles.
  void select(bool active) {
    chipSelected = active;
    phase = 0;
    cmd = 0;
    addr = 0;
    addrBytes = 0;
    idBytes = 0;
    readPos = 0;
  }

  // Direct read from the emulated memory (used by tests to peek).
  void peek(uint32_t address, uint8_t *out, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
      out[i] = (address + i < SIZE) ? mem[address + i] : 0;
    }
  }

  uint8_t transfer(uint8_t b) {
    if (!chipSelected) {
      return 0xFF;
    }
    switch (phase) {
    case 0: // command byte
      cmd = b;
      addr = 0;
      addrBytes = 0;
      idBytes = 0;
      readPos = 0;
      switch (b) {
      case 0x66: // RESET_ENABLE
      case 0x99: // RESET
        phase = 0;
        break;
      case 0x9F: // READ_ID
        phase = 1;
        break;
      case 0x02: // WRITE
        phase = 2;
        break;
      case 0x0B: // FAST_READ
        phase = 5;
        break;
      default:
        phase = 0;
        break;
      }
      return 0xFF;

    case 1: // READ_ID dummy byte
      if (++idBytes >= 3) {
        phase = 4;
        readPos = 0;
      }
      return 0xFF;

    case 4: { // READ_ID response (APS6404 => 0x0D 0x5D)
      static const uint8_t id[2] = {0x0D, 0x5D};
      uint8_t r = id[readPos];
      if (++readPos >= 2) {
        phase = 0;
      }
      return r;
    }

    case 2: // WRITE address byte
      addr = (addr << 8) | b;
      if (++addrBytes >= 3) {
        phase = 3;
      }
      return 0xFF;

    case 3: // WRITE data byte
      if (addr < SIZE) {
        mem[addr] = b;
      }
      addr++;
      return 0xFF;

    case 5: // FAST_READ address byte
      addr = (addr << 8) | b;
      if (++addrBytes >= 3) {
        phase = 6;
      }
      return 0xFF;

    case 6: // FAST_READ dummy byte
      phase = 7;
      return 0xFF;

    case 7: { // FAST_READ data byte
      uint8_t r = (addr < SIZE) ? mem[addr] : 0xFF;
      addr++;
      return r;
    }

    default:
      return 0xFF;
    }
  }
};

inline FakePsram &fakePsram() {
  static FakePsram instance;
  return instance;
}

} // namespace openpod_test
