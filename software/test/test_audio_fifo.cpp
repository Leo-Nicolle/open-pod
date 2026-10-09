#include <doctest.h>

#include <vector>

#include "../src/sound/AudioFifo.h"

namespace {

// Drains up to `max` bytes the way the feeder does (contiguous reads,
// at most 32 per burst).
std::vector<uint8_t> drain(AudioFifo &fifo, size_t max) {
  std::vector<uint8_t> out;
  while (out.size() < max) {
    uint32_t n;
    const uint8_t *p = fifo.readPtr(n);
    if (n == 0) break;
    if (n > 32) n = 32;
    if (n > max - out.size()) n = (uint32_t)(max - out.size());
    out.insert(out.end(), p, p + n);
    fifo.consume(n);
  }
  return out;
}

} // namespace

TEST_SUITE("AudioFifo") {
  TEST_CASE("starts empty, fills to capacity, refuses the excess") {
    static AudioFifo fifo;
    fifo.reset();
    CHECK(fifo.size() == 0);
    CHECK(fifo.freeSpace() == AudioFifo::CAPACITY);
    std::vector<uint8_t> data(AudioFifo::CAPACITY + 100, 7);
    CHECK(fifo.write(data.data(), data.size()) == AudioFifo::CAPACITY);
    CHECK(fifo.size() == AudioFifo::CAPACITY);
    CHECK(fifo.freeSpace() == 0);
    uint32_t contig;
    fifo.writePtr(contig);
    CHECK(contig == 0);
  }

  TEST_CASE("keeps byte order across many wraps") {
    static AudioFifo fifo;
    fifo.reset();
    uint8_t next = 0, expected = 0;
    size_t mismatches = 0;
    // Odd write and read sizes so the wrap point moves around.
    for (int round = 0; round < 2000; round++) {
      std::vector<uint8_t> chunk(1000 + (round * 37) % 3000);
      for (auto &b : chunk) b = next++;
      uint32_t taken = fifo.write(chunk.data(), (uint32_t)chunk.size());
      next = (uint8_t)(next - (chunk.size() - taken)); // not taken: resend
      std::vector<uint8_t> out = drain(fifo, 700 + (round * 53) % 4000);
      for (uint8_t b : out) {
        mismatches += (b != expected);
        expected++;
      }
    }
    std::vector<uint8_t> rest = drain(fifo, AudioFifo::CAPACITY);
    for (uint8_t b : rest) {
      mismatches += (b != expected);
      expected++;
    }
    CHECK(mismatches == 0);
    CHECK(expected == next);
  }

  TEST_CASE("fill() appends a repeated byte") {
    static AudioFifo fifo;
    fifo.reset();
    uint8_t one = 1;
    fifo.write(&one, 1);
    CHECK(fifo.fill(0x55, 12288) == 12288);
    std::vector<uint8_t> out = drain(fifo, 20000);
    REQUIRE(out.size() == 12289);
    CHECK(out[0] == 1);
    size_t wrong = 0;
    for (size_t i = 1; i < out.size(); i++) wrong += (out[i] != 0x55);
    CHECK(wrong == 0);
  }
}
