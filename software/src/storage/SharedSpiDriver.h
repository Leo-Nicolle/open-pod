#pragma once
// SD card SPI configuration shared by every SdFat instance (audio buffer,
// music lookup, music index): SPI1 at 25 MHz requested (22.5 MHz actual).
//
// On the device SdFat goes through SharedSpiDriver (SPI_DRIVER_SELECT=3 in
// platformio.ini), which holds the SPI1 bus lock for every SD exchange so
// the VS1053 feeder interrupt never clocks the bus mid-transfer (see
// sound/Spi1Bus.h). Native tests use the SdFat mock and a plain config.

#include <SdFat.h>

#ifdef UNIT_TEST

#define OPENPOD_SD_CONFIG(cs) SdSpiConfig((cs), SHARED_SPI, SD_SCK_MHZ(25))

#else

#include <SPI.h>
#include "../sound/Spi1Bus.h"

#if SPI_DRIVER_SELECT != 3
#error "SharedSpiDriver needs -DSPI_DRIVER_SELECT=3 (see platformio.ini)"
#endif

// Same transfers as SdFat's STM32 driver (SdSpiSTM32Core.cpp), plus the
// SPI1 lock. SdFat calls activate() before asserting the card's chip select
// and deactivate() after releasing it (and its trailing 0xFF), so the lock
// covers the whole exchange.
class SharedSpiDriver : public SdSpiBaseClass {
public:
  void activate() override {
    spi1bus::acquire();
    SPI.beginTransaction(settings);
  }
  void begin(SdSpiConfig) override { SPI.begin(); }
  void deactivate() override {
    SPI.endTransaction();
    spi1bus::release();
  }
  // SPI1 also drives the VS1053: never shut it down from SdFat.
  void end() override {}
  uint8_t receive() override { return SPI.transfer(0xFF); }
  uint8_t receive(uint8_t *buf, size_t count) override {
    memset(buf, 0xFF, count);
    SPI.transfer(buf, count);
    return 0;
  }
  void send(uint8_t data) override { SPI.transfer(data); }
  void send(const uint8_t *buf, size_t count) override {
    SPI.transfer(buf, nullptr, count);
  }
  void setSckSpeed(uint32_t maxSck) override {
    settings = SPISettings(maxSck, MSBFIRST, SPI_MODE0);
  }

private:
  SPISettings settings;
};

extern SharedSpiDriver sharedSpiDriver;

#define OPENPOD_SD_CONFIG(cs)                                                  \
  SdSpiConfig((cs), SHARED_SPI, SD_SCK_MHZ(25), &sharedSpiDriver)

#endif
