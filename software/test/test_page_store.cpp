#include <doctest.h>
#include <vector>

#include "../src/storage/PageStore.h"
#include "test_helpers.h"

namespace {

const uint32_t PAGE = PageStore::PAGE_SIZE;

// A pack with section 1 = `size` bytes where byte i is (i * 7 + i / 251) &
// 0xFF (not periodic in the page size), and a small section 2.
std::vector<uint8_t> patterned(uint32_t size) {
  std::vector<uint8_t> v(size);
  for (uint32_t i = 0; i < size; i++)
    v[i] = (uint8_t)((i * 7 + i / 251) & 0xFF);
  return v;
}

void registerPack(uint32_t size) {
  openpod_test::resetAll();
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> sections;
  sections.push_back(std::make_pair(1u, patterned(size)));
  sections.push_back(std::make_pair(2u, std::vector<uint8_t>{1, 2, 3}));
  openpod_test::FakeFs::get().addFile("/openpod/test.pack",
                                      openpod_test::buildPack(sections));
}

} // namespace

TEST_CASE("PageStore - reads match the file, across pages") {
  registerPack(5 * PAGE + 123);
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 16 * PAGE);
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  PackRef s;
  REQUIRE(store.find(0, 1, s));
  CHECK(s.length == 5 * PAGE + 123);
  std::vector<uint8_t> want = patterned(s.length);

  // One read spanning three pages.
  std::vector<uint8_t> got(2 * PAGE + 10);
  REQUIRE(store.read(s, PAGE - 5, got.data(), (uint32_t)got.size()));
  for (size_t i = 0; i < got.size(); i++) {
    REQUIRE(got[i] == want[PAGE - 5 + i]);
  }
  // The last bytes of the section.
  uint8_t tail[4];
  REQUIRE(store.read(s, s.length - 4, tail, 4));
  for (int i = 0; i < 4; i++)
    CHECK(tail[i] == want[s.length - 4 + i]);

  PackRef small;
  REQUIRE(store.find(0, 2, small));
  uint8_t b[3];
  REQUIRE(store.read(small, 0, b, 3));
  CHECK(b[0] == 1);
  CHECK(b[2] == 3);
  CHECK_FALSE(store.find(0, 99, small));
}

TEST_CASE("PageStore - pages come from the cache once loaded") {
  registerPack(4 * PAGE);
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 16 * PAGE);
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  PackRef s;
  REQUIRE(store.find(0, 1, s));
  store.resetStats();
  uint32_t v;
  for (int round = 0; round < 3; round++) {
    for (uint32_t off = 0; off < 4 * PAGE; off += 512)
      store.read(s, off, &v, 4);
  }
  // Section 1 starts inside the first page: 4 or 5 pages, read once each.
  CHECK(store.stats().misses <= 5);
  CHECK(store.stats().hits > 40);
}

TEST_CASE("PageStore - eviction keeps reads correct with a tiny cache") {
  registerPack(20 * PAGE);
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 3 * PAGE); // 3 slots
  CHECK(store.slotCount() == 3);
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  PackRef s;
  REQUIRE(store.find(0, 1, s));
  std::vector<uint8_t> want = patterned(s.length);

  // Strided and backwards reads over 20 pages through 3 slots.
  uint32_t seed = 12345;
  for (int i = 0; i < 80; i++) {
    seed = seed * 1103515245u + 12345u;
    uint32_t off = (seed >> 8) % (s.length - 64);
    uint8_t got[64];
    REQUIRE(store.read(s, off, got, sizeof(got)));
    for (int k = 0; k < 64; k++)
      REQUIRE(got[k] == want[off + k]);
  }
  CHECK(store.stats().misses > 3);
}

TEST_CASE("PageStore - reads outside a section are refused") {
  registerPack(1000);
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 4 * PAGE);
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  PackRef s;
  REQUIRE(store.find(0, 1, s));
  uint8_t b[8] = {9, 9, 9, 9, 9, 9, 9, 9};
  CHECK_FALSE(store.read(s, 996, b, 8)); // runs past the end
  CHECK(b[0] == 0);                      // zero-filled
  CHECK_FALSE(store.read(s, 5000, b, 1));
  CHECK(store.read(s, 992, b, 8));

  // An unopened pack reads nothing.
  PackRef other = {(1u << 31) | 16, 8};
  CHECK_FALSE(store.read(other, 0, b, 4));
}

TEST_CASE("PageStore - rejects files that aren't packs") {
  openpod_test::resetAll();
  openpod_test::FakeFs::get().addFile("/openpod/x.pack",
                                      std::vector<uint8_t>(64, 0));
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 4 * PAGE);
  CHECK(store.open(sd, "/openpod/x.pack", 0) == -1);
  CHECK(store.open(sd, "/openpod/missing.pack", 0) == -1);
  CHECK_FALSE(store.isOpen(0));
}

TEST_CASE("PageStore - reopening a pack drops its cached pages") {
  registerPack(2 * PAGE);
  PageStore store;
  SdFat sd;
  store.begin(INDEX_CACHE_BASE_ADDRESS, 4 * PAGE);
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  PackRef s;
  REQUIRE(store.find(0, 1, s));
  uint8_t before;
  store.read(s, 100, &before, 1);

  // The card's pack changes (a re-sync): same name, other bytes.
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> sections;
  sections.push_back(std::make_pair(1u, std::vector<uint8_t>(2 * PAGE, 0xAB)));
  openpod_test::FakeFs::get().addFile("/openpod/test.pack",
                                      openpod_test::buildPack(sections));
  REQUIRE(store.open(sd, "/openpod/test.pack", 0) == 0);
  REQUIRE(store.find(0, 1, s));
  uint8_t after;
  store.read(s, 100, &after, 1);
  CHECK(after == 0xAB);
}
