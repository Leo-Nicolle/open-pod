#pragma once
// Fetches raw565 album-art pixel rows directly from thumbs.bin on the SD
// card into the caller's render buffer, with zero decode step. raw565 was
// chosen (over QOI) after on-device benchmarking - see
// .agents/covers-plan.md Phase 0: real hardware timing showed raw565 ties
// or beats QOI for actual covers (the case that matters), and a PSRAM
// pixel cache round-trip (78ms write + 78ms read for 32KB, its
// byte-at-a-time SPI is ~0.42MB/s) is far slower than just re-reading from
// SD (~1.3MB/s) again, so there's no cache here either - every render is a
// fresh, cheap (~25ms) direct read.
//
// Deliberately decoupled from MusicLookup (which only resolves *where* a
// cover is - offset/length/format, via album_cover_entry_t) and from
// NowPlayingComponent (which only knows *what* to draw) - this module is
// purely "given a byte range in thumbs.bin, get those pixels into a
// buffer."
#include <Arduino.h>
#include <SdFat.h>
#include <stdint.h>

class AlbumArt {
private:
  FsFile thumbsFile;
  bool ready = false;
  static const uint32_t COVER_WIDTH = 128;
  static const uint32_t ROW_BYTES = COVER_WIDTH * 2; // raw565: 2 bytes/pixel

public:
  // Opens /openpod/thumbs.bin once and keeps the handle open for the
  // component's lifetime, avoiding repeated SD open/close overhead on
  // every render. Call once at startup (see main.cpp), after the SD card
  // the passed-in SdFat is already begin()'d on (e.g. musicLookup.getSD()
  // - reuses the existing SD connection rather than opening a second one).
  bool init(SdFat &sd) {
    ready = thumbsFile.open("/openpod/thumbs.bin", O_RDONLY);
    if (!ready) {
      Serial.println("ERROR: AlbumArt could not open /openpod/thumbs.bin");
    }
    return ready;
  }

  bool isReady() const { return ready; }

  // Blits `rowCount` rows starting at `coverRowStart` within a 128x128
  // raw565 cover (coverOffset/coverLength as resolved by
  // MusicLookup::getAlbumCoverEntry) into destBuffer, a caller-owned
  // buffer of stride `destStride` uint16_t per row, starting at
  // (destRow0, destCol0). Row-by-row because destStride (the render
  // chunk's width, which varies during slide transitions) generally
  // differs from the cover's own 128px row width, so a single multi-row
  // read isn't safe. Returns false (leaving destBuffer untouched from
  // that point on) on any failure, so the caller can fall back to a solid
  // fill rather than show garbage.
  bool blitCoverRows(uint16_t *destBuffer, int destStride, int destRow0,
                     int destCol0, uint32_t coverOffset, uint32_t coverLength,
                     int coverRowStart, int rowCount) {
    if (!ready || !destBuffer || rowCount <= 0 || coverRowStart < 0) {
      return false;
    }

    // Bounds-check against the entry's own declared length, defending
    // against a corrupt/stale album_to_cover.bin rather than reading
    // garbage off the end of another cover's blob in thumbs.bin.
    uint32_t neededBytes = (uint32_t)(coverRowStart + rowCount) * ROW_BYTES;
    if (neededBytes > coverLength) {
      return false;
    }

    for (int row = 0; row < rowCount; row++) {
      uint32_t fileOffset =
          coverOffset + (uint32_t)(coverRowStart + row) * ROW_BYTES;
      if (!thumbsFile.seekSet(fileOffset)) {
        return false;
      }
      uint16_t *dest =
          destBuffer + (size_t)(destRow0 + row) * destStride + destCol0;
      int bytesRead = thumbsFile.read((uint8_t *)dest, ROW_BYTES);
      if (bytesRead != (int)ROW_BYTES) {
        return false;
      }
    }
    return true;
  }
};

// Global instance, matching this codebase's existing convention for
// shared hardware-backed singletons (e.g. `psram` in PSRAM_controller.cpp,
// `musicLookup`/`display` in main.cpp). Defined (given storage) in
// main.cpp; init() is called there once, after musicLookup.init().
extern AlbumArt albumArt;
