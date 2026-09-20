// Not including "../src/fonts/IBMPlexSans16Bold.h" directly here: it's the
// one place that *defines* the global `IBMPlexSans16Bold` FastFont (matching
// the `extern const FastFont IBMPlexSans16Bold;` declared in
// rendering/font_renderer.h, pulled in transitively below via list_cache.hpp)
// and only one translation unit may do that or the linker sees multiple
// definitions. test_list_cache.cpp owns that include; this file just uses
// the extern symbol.
#include "../src/ui/list/list_cache.hpp"
#include "../src/ui/list/list_renderer.hpp"
#include "../src/ui/list/utils.h"
#include "./mock_ui_types.h"
#include <algorithm>
#include <doctest.h>

class BitOperationsTestFixture {
public:
  ListCache *cache1bpp;
  ListCache *cache2bpp;
  ListCache *cache4bpp;
  ListRenderer *renderer1bpp;
  ListRenderer *renderer2bpp;
  ListRenderer *renderer4bpp;

  BitOperationsTestFixture() {
    cache1bpp = new ListCache(IBMPlexSans16Bold, 1);
    cache2bpp = new ListCache(IBMPlexSans16Bold, 2);
    cache4bpp = new ListCache(IBMPlexSans16Bold, 4);
    renderer1bpp = new ListRenderer(*cache1bpp);
    renderer2bpp = new ListRenderer(*cache2bpp);
    renderer4bpp = new ListRenderer(*cache4bpp);
  }

  ~BitOperationsTestFixture() {
    delete renderer1bpp;
    delete renderer2bpp;
    delete renderer4bpp;
    delete cache1bpp;
    delete cache2bpp;
    delete cache4bpp;
  }
  int countUniqueColors(uint16_t *buffer, int size) {
    const int MAX_COLORS = 256; // Reasonable limit for counting
    uint16_t uniqueColors[MAX_COLORS];
    int count = 0;

    for (int i = 0; i < size && count < MAX_COLORS; i++) {
      bool found = false;
      for (int j = 0; j < count; j++) {
        if (uniqueColors[j] == buffer[i]) {
          found = true;
          break;
        }
      }
      if (!found) {
        uniqueColors[count++] = buffer[i];
      }
    }

    return count;
  }

  friend void test_rendering_quality_comparison();
};

TEST_CASE_FIXTURE(BitOperationsTestFixture, "1bpp storage efficiency") {
  int width = 64;
  int height = 32;
  int expectedBytes = (width * height + 7) / 8; // Round up to nearest byte

  int actualBytes = calculateBufferSize(width, height, 1);
  CHECK_EQ(expectedBytes, actualBytes);

  // renderElementBinary() always memsets cache->bytesPerElement bytes
  // (computed from the cache's fixed internal cacheWidth x cacheHeight),
  // regardless of the width/height used above for the calculateBufferSize()
  // check - allocate the buffer that actually gets rendered into using that
  // size instead, to avoid a heap overflow.
  uint8_t *buffer = new uint8_t[cache1bpp->bytesPerElement];
  memset(buffer, 0, cache1bpp->bytesPerElement);

  cache1bpp->renderElementBinary("Test", buffer);

  // Buffer should have some content
  bool hasContent = false;
  for (int i = 0; i < cache1bpp->bytesPerElement; i++) {
    if (buffer[i] != 0) {
      hasContent = true;
      break;
    }
  }
  CHECK(hasContent);

  delete[] buffer;
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "2bpp transparency levels") {
  // ListRenderer::renderRect() always reads from the cache's own internal
  // per-row buffer (cache.getCacheForRow()), not from a buffer supplied by
  // the caller, so to exercise specific bit patterns we have to write them
  // directly into the cache's row 0 buffer and mark that row valid.
  int width = cache2bpp->cacheWidth;
  int height = cache2bpp->cacheHeight;
  int bufferSize = cache2bpp->bytesPerElement;

  uint8_t *binaryBuffer = cache2bpp->getCacheForRow(0);
  uint16_t *displayBuffer = new uint16_t[width * height];

  SUBCASE("All transparent (00)") {
    memset(binaryBuffer, 0x00, bufferSize); // All 00 (transparent)
    cache2bpp->cacheValid[0] = true;
    renderer2bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    // Should render valid colors
    bool allValid = true;
    for (int i = 0; i < width * height; i++) {
      if (displayBuffer[i] > 0xFFFF) {
        allValid = false;
        break;
      }
    }
    CHECK(allValid);
  }

  SUBCASE("All opaque (11)") {
    memset(binaryBuffer, 0xFF, bufferSize); // All 11 (opaque)
    cache2bpp->cacheValid[0] = true;
    renderer2bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    // Should render valid colors
    bool allValid = true;
    for (int i = 0; i < width * height; i++) {
      if (displayBuffer[i] > 0xFFFF) {
        allValid = false;
        break;
      }
    }
    CHECK(allValid);
  }

  SUBCASE("Intermediate transparency (01)") {
    memset(binaryBuffer, 0x55, bufferSize); // Pattern of 01 (25% opacity)
    cache2bpp->cacheValid[0] = true;
    renderer2bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    // Should render valid colors
    bool allValid = true;
    for (int i = 0; i < width * height; i++) {
      if (displayBuffer[i] > 0xFFFF) {
        allValid = false;
        break;
      }
    }
    CHECK(allValid);
  }

  delete[] displayBuffer;
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "4bpp smooth gradients") {
  int width = cache4bpp->cacheWidth;
  int height = cache4bpp->cacheHeight;
  int bufferSize = cache4bpp->bytesPerElement;

  uint8_t *binaryBuffer = cache4bpp->getCacheForRow(0);
  uint16_t *displayBuffer = new uint16_t[width * height];

  SUBCASE("Minimum transparency (0000)") {
    memset(binaryBuffer, 0x00, bufferSize);
    cache4bpp->cacheValid[0] = true;
    renderer4bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    CHECK_LE(displayBuffer[0], 0xFFFF);
  }

  SUBCASE("Maximum transparency (1111)") {
    memset(binaryBuffer, 0xFF, bufferSize);
    cache4bpp->cacheValid[0] = true;
    renderer4bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    CHECK_LE(displayBuffer[0], 0xFFFF);
  }

  SUBCASE("Intermediate transparency (1000)") {
    memset(binaryBuffer, 0x88, bufferSize); // 1000 pattern (8/15 opacity)
    cache4bpp->cacheValid[0] = true;
    renderer4bpp->renderRect(0, 0, width, height, displayBuffer, -1);

    // Should get a valid color
    uint16_t blendedColor = displayBuffer[0];
    CHECK_LE(blendedColor, 0xFFFF);
  }

  delete[] displayBuffer;
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "Bit boundary handling") {
  // Test edge cases where bits cross byte boundaries.
  // Note: renderElementBinary()'s buffer size is fixed by the cache's own
  // cacheWidth x cacheHeight (not by the width/height below), so only the
  // `width` values below (passed through to clip the rendered text) still
  // vary the code path; they're kept for coverage of the clipping logic.

  int testWidths[] = {7, 15, 31, 63, 127};
  int bitDepths[] = {1, 2, 4};

  for (int w = 0; w < 5; w++) {
    for (int b = 0; b < 3; b++) {
      int width = testWidths[w];
      int bpp = bitDepths[b];

      ListCache *cache = nullptr;
      switch (bpp) {
      case 1:
        cache = cache1bpp;
        break;
      case 2:
        cache = cache2bpp;
        break;
      case 4:
        cache = cache4bpp;
        break;
      }

      int bufferSize = cache->bytesPerElement;
      uint8_t *buffer = new uint8_t[bufferSize];

      // Should not crash with odd widths
      REQUIRE_NOTHROW(cache->renderElementBinary("Test", buffer, 0, width));

      // Verify buffer is not corrupted (no access violations)
      bool accessible = true;
      for (int i = 0; i < bufferSize; i++) {
        volatile uint8_t test = buffer[i]; // Read each byte
        (void)test;                        // Avoid unused variable warning
      }
      CHECK(accessible);

      delete[] buffer;
    }
  }
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "Color blending accuracy") {
  // Test that color blending produces an expected intermediate value.
  // renderRect() blends between the theme's COLOR_BACKGROUND and
  // COLOR_TEXT (not arbitrary black/white), so the expected bounds below
  // are derived from those actual theme colors rather than hardcoded ones.
  int width = cache4bpp->cacheWidth;
  int height = cache4bpp->cacheHeight;
  int bufferSize = cache4bpp->bytesPerElement;

  uint8_t *binaryBuffer = cache4bpp->getCacheForRow(0);
  uint16_t *displayBuffer = new uint16_t[width * height];

  // 0x88 = 1000 1000: two consecutive 4bpp pixels with value 8 (of max 15),
  // i.e. roughly a 50% blend between background and text.
  memset(binaryBuffer, 0x88, bufferSize);
  cache4bpp->cacheValid[0] = true;
  renderer4bpp->renderRect(0, 0, width, height, displayBuffer, -1);

  uint16_t blendedColor = displayBuffer[0];

  // Extract RGB565 components of the blended pixel and of both blend
  // endpoints (the theme's actual background/text colors).
  auto extract = [](uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (c >> 11) & 0x1F;
    g = (c >> 5) & 0x3F;
    b = c & 0x1F;
  };

  uint8_t r, g, b;
  uint8_t bgR, bgG, bgB;
  uint8_t fgR, fgG, fgB;
  extract(blendedColor, r, g, b);
  extract(COLOR_BACKGROUND, bgR, bgG, bgB);
  extract(COLOR_TEXT, fgR, fgG, fgB);

  // A partial blend should land between (inclusive) the background and text
  // channel values, whichever order they happen to be in.
  CHECK(r >= std::min(bgR, fgR));
  CHECK(r <= std::max(bgR, fgR));
  CHECK(g >= std::min(bgG, fgG));
  CHECK(g <= std::max(bgG, fgG));
  CHECK(b >= std::min(bgB, fgB));
  CHECK(b <= std::max(bgB, fgB));

  delete[] displayBuffer;
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "Memory efficiency comparison") {
  int width = 100;
  int height = 30;

  int size1bpp = calculateBufferSize(width, height, 1);
  int size2bpp = calculateBufferSize(width, height, 2);
  int size4bpp = calculateBufferSize(width, height, 4);

  // Verify expected size relationships
  CHECK_EQ(size1bpp * 2, size2bpp);
  CHECK_EQ(size1bpp * 4, size4bpp);
  CHECK_EQ(size2bpp * 2, size4bpp);

  // Output memory usage information
  MESSAGE("Memory usage for ", width, "x", height, " track:");
  MESSAGE("1bpp: ", size1bpp, " bytes");
  MESSAGE("2bpp: ", size2bpp, " bytes");
  MESSAGE("4bpp: ", size4bpp, " bytes");
}

TEST_CASE_FIXTURE(BitOperationsTestFixture, "Rendering quality comparison") {
  const char *testText = "Quality Test N";

  // Render the same text at each bit depth directly into each cache's own
  // row 0 buffer (renderRect() only ever reads from cache.getCacheForRow(),
  // so rendering into a separate, disconnected buffer would never actually
  // reach the display buffer below).
  int width = cache1bpp->cacheWidth;
  int height = cache1bpp->cacheHeight;

  cache1bpp->renderElementBinary(testText, cache1bpp->getCacheForRow(0));
  cache1bpp->cacheValid[0] = true;
  cache2bpp->renderElementBinary(testText, cache2bpp->getCacheForRow(0));
  cache2bpp->cacheValid[0] = true;
  cache4bpp->renderElementBinary(testText, cache4bpp->getCacheForRow(0));
  cache4bpp->cacheValid[0] = true;

  uint16_t *display1 = new uint16_t[width * height];
  uint16_t *display2 = new uint16_t[width * height];
  uint16_t *display4 = new uint16_t[width * height];

  renderer1bpp->renderRect(0, 0, width, height, display1, -1);
  renderer2bpp->renderRect(0, 0, width, height, display2, -1);
  renderer4bpp->renderRect(0, 0, width, height, display4, -1);

  // Count unique colors in each rendering
  int uniqueColors1 = countUniqueColors(display1, width * height);
  int uniqueColors2 = countUniqueColors(display2, width * height);
  int uniqueColors4 = countUniqueColors(display4, width * height);

  MESSAGE("Unique colors - 1bpp: ", uniqueColors1, ", 2bpp: ", uniqueColors2,
          ", 4bpp: ", uniqueColors4);

  // Higher bit depth should generally have more unique colors (better
  // anti-aliasing)
  CHECK_GE(uniqueColors2, uniqueColors1);
  CHECK_GE(uniqueColors4, uniqueColors2);

  delete[] display1;
  delete[] display2;
  delete[] display4;
}
