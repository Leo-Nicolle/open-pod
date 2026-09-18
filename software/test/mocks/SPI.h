// Minimal SPI.h mock for native tests.
//
// The storage code only instantiates SPIClass directly (`new SPIClass(mosi,
// miso, sck)`) inside SPI_PSRAM. That instance's transfers are forwarded to the
// in-memory fake PSRAM device.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fake_psram.h"

#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif

#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

#define SPI_HAS_TRANSACTION 1

class SPISettings {
  uint32_t clock;
  uint8_t bitOrder;
  uint8_t dataMode;

public:
  SPISettings(uint32_t c, uint8_t o, uint8_t m)
      : clock(c), bitOrder(o), dataMode(m) {}
  SPISettings() : clock(4000000), bitOrder(MSBFIRST), dataMode(SPI_MODE0) {}

  bool operator==(const SPISettings &other) const {
    return clock == other.clock && bitOrder == other.bitOrder &&
           dataMode == other.dataMode;
  }
};

class SPIClass {
  int mosi_;
  int miso_;
  int sck_;

public:
  SPIClass(int mosi, int miso, int sck)
      : mosi_(mosi), miso_(miso), sck_(sck) {}
  SPIClass() : mosi_(-1), miso_(-1), sck_(-1) {}

  void begin() {}
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}

  uint8_t transfer(uint8_t data) {
    return openpod_test::fakePsram().transfer(data);
  }

  void transfer(void *buf, size_t count) {
    uint8_t *p = static_cast<uint8_t *>(buf);
    for (size_t i = 0; i < count; i++) {
      p[i] = transfer(p[i]);
    }
  }
};
