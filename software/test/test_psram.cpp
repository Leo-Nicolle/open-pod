#include <doctest.h>

#include <vector>

#include "../src/storage/PSRAM_controller.hpp"
#include "mocks/fake_psram.h"

using openpod_test::fakePsram;

namespace {

std::vector<uint8_t> pattern(size_t len, uint8_t seed) {
  std::vector<uint8_t> v(len);
  for (size_t i = 0; i < len; i++) v[i] = (uint8_t)(i * 31 + seed);
  return v;
}

} // namespace

TEST_SUITE("PSRAM controller") {
  TEST_CASE("tCEM burst size follows the SPI clock") {
    REQUIRE(psram.init());
    uint32_t savedClock = psram.getClock();
    psram.setClock(22500000);
    // 22 bytes fit in 8 us at 22.5 MHz, minus 5 header + 2 slack bytes.
    CHECK(psram.tcemBurstBytes() == 15);
    psram.setClock(5625000);
    CHECK(psram.tcemBurstBytes() == 1);
    psram.setClock(savedClock);
  }

  TEST_CASE("Split bursts write and read back across page boundaries") {
    fakePsram().reset();
    REQUIRE(psram.init());
    uint32_t savedBurst = psram.getMaxBurst();
    const uint32_t bursts[] = {0, 1, 7, 15, 64};
    for (uint32_t b : bursts) {
      CAPTURE(b);
      psram.setMaxBurst(b);
      // Odd length starting just before a 1 KB page boundary.
      uint32_t addr = 3 * 1024 - 13;
      std::vector<uint8_t> data = pattern(2 * 1024 + 37, (uint8_t)b);
      psram.writeData(addr, data.data(), data.size());
      std::vector<uint8_t> back(data.size());
      psram.readData(addr, back.data(), back.size());
      CHECK(back == data);
    }
    psram.setMaxBurst(savedBurst);
  }
}
