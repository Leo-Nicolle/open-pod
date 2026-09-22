#include <doctest.h>
#include <vector>

#include "../src/storage/AlbumArt.h"
#include "mocks/SdFat.h"

namespace {

// Builds a synthetic thumbs.bin: `preludeBytes` of filler (simulating an
// earlier cover's blob before this one), followed by a 160-column raw565
// "cover" of `rows` rows, each row filled with the single uint16 value
// `rowValue(row)` - so a correct blit is trivially verifiable pixel-by-pixel.
std::vector<uint8_t> buildThumbsBin(uint32_t preludeBytes, int rows,
                                    uint16_t (*rowValue)(int)) {
  std::vector<uint8_t> out(preludeBytes, 0xAA); // filler, must never be read
  for (int row = 0; row < rows; row++) {
    uint16_t value = rowValue(row);
    for (int col = 0; col < 160; col++) {
      out.push_back((uint8_t)(value & 0xFF));
      out.push_back((uint8_t)((value >> 8) & 0xFF));
    }
  }
  return out;
}

uint16_t rowPlusOne(int row) { return (uint16_t)(row + 1); }

} // namespace

TEST_CASE("AlbumArt - blitCoverRows copies the right bytes to the right place") {
  openpod_test::FakeFs::get().reset();
  const uint32_t coverOffset = 900; // simulates a non-zero offset in thumbs.bin
  const int coverRows = 160;
  const uint32_t coverLength = coverRows * 160 * 2;
  auto bin = buildThumbsBin(coverOffset, coverRows, rowPlusOne);
  openpod_test::FakeFs::get().addFile("/openpod/thumbs.bin", bin);

  SdFat sd;
  AlbumArt art;
  REQUIRE(art.init(sd) == true);
  CHECK(art.isReady() == true);

  // Destination buffer wider than the cover (like a 320px-wide chunk
  // buffer), blitting 3 rows starting at cover row 10, placed at
  // destRow0=2, destCol0=100 - deliberately different stride/offset from
  // the cover's own 160px width, to prove row-by-row placement is correct
  // rather than accidentally relying on stride == 160.
  const int destStride = 320;
  const int destRow0 = 2;
  const int destCol0 = 100;
  const int rowCount = 3;
  const int coverRowStart = 10;
  std::vector<uint16_t> dest(destStride * (destRow0 + rowCount + 2), 0xDEAD);

  bool ok = art.blitCoverRows(dest.data(), destStride, destRow0, destCol0,
                              coverOffset, coverLength, coverRowStart,
                              rowCount);
  REQUIRE(ok == true);

  for (int row = 0; row < rowCount; row++) {
    uint16_t expected = rowPlusOne(coverRowStart + row);
    for (int col = 0; col < 160; col++) {
      uint16_t actual = dest[(destRow0 + row) * destStride + destCol0 + col];
      CHECK(actual == expected);
    }
    // Column just before the blitted region must be untouched.
    CHECK(dest[(destRow0 + row) * destStride + destCol0 - 1] == 0xDEAD);
  }
  // Row just before/after the blitted region must be untouched.
  CHECK(dest[(destRow0 - 1) * destStride + destCol0] == 0xDEAD);
  CHECK(dest[(destRow0 + rowCount) * destStride + destCol0] == 0xDEAD);
}

TEST_CASE("AlbumArt - blitCoverRows rejects a range exceeding the entry's declared length") {
  openpod_test::FakeFs::get().reset();
  auto bin = buildThumbsBin(0, 5, rowPlusOne);
  openpod_test::FakeFs::get().addFile("/openpod/thumbs.bin", bin);

  SdFat sd;
  AlbumArt art;
  REQUIRE(art.init(sd) == true);

  std::vector<uint16_t> dest(160 * 10, 0);
  // coverLength only covers 5 rows (5*160*2 bytes), but we ask for rows
  // 3..7 (5 rows starting at row 3) - rows 5,6 are out of bounds.
  uint32_t coverLength = 5 * 160 * 2;
  bool ok = art.blitCoverRows(dest.data(), 160, 0, 0, 0, coverLength, 3, 5);
  CHECK(ok == false);
}

TEST_CASE("AlbumArt - blitCoverRows fails cleanly when thumbs.bin is missing") {
  openpod_test::FakeFs::get().reset(); // no thumbs.bin registered

  SdFat sd;
  AlbumArt art;
  CHECK(art.init(sd) == false);
  CHECK(art.isReady() == false);

  std::vector<uint16_t> dest(160 * 2, 0);
  bool ok = art.blitCoverRows(dest.data(), 160, 0, 0, 0, 160 * 2, 0, 2);
  CHECK(ok == false);
}

TEST_CASE("AlbumArt - blitCoverRows rejects a non-positive row count") {
  openpod_test::FakeFs::get().reset();
  auto bin = buildThumbsBin(0, 2, rowPlusOne);
  openpod_test::FakeFs::get().addFile("/openpod/thumbs.bin", bin);

  SdFat sd;
  AlbumArt art;
  REQUIRE(art.init(sd) == true);

  std::vector<uint16_t> dest(160 * 2, 0);
  CHECK(art.blitCoverRows(dest.data(), 160, 0, 0, 0, 2 * 160 * 2, 0, 0) == false);
  CHECK(art.blitCoverRows(dest.data(), 160, 0, 0, 0, 2 * 160 * 2, -1, 2) == false);
}
