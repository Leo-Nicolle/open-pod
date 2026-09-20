#pragma once
// On-device benchmark: measures real QOI-vs-raw565 cover load/decode/render
// cost on the actual STM32F446RE, to decide which format the production
// AlbumArt module (software/src/storage/AlbumArt.h, not yet written) should
// use. See .agents/covers-plan.md, Phase 0.
//
// Reads the bench_qoi/ and bench_raw565/ fixture sets produced by
// indexer/generate-benchmark-covers.ts (copy both onto the SD card under
// /openpod/ before running this).
#include <Arduino.h>
#include <SdFat.h>

#include "../../src/rendering/ILI9341_GFX.h"
#include "../../src/storage/PSRAM_controller.hpp"
#include "qoi_decode.h"

namespace onboard_bench {

// Largest QOI blob we'll allocate a read buffer for. Real covers on the test
// library topped out around 37KB (36.3KB); this leaves a few KB of margin
// without wasting RAM - total SRAM is only 128KB, ~38.5KB of which is
// already claimed by the framework/libraries before this object exists, so
// this buffer plus COVER_BYTES_565 below must stay well under the ~92.5KB
// remaining (see .agents/covers-plan.md Phase 0's RAM notes). Raise this
// (and re-check the RAM budget) before pointing this benchmark at a library
// with larger real covers.
static const uint32_t MAX_QOI_BYTES = 40 * 1024;
static const uint32_t COVER_PIXELS = 128 * 128;
static const uint32_t COVER_BYTES_565 = COVER_PIXELS * 2; // 32KB

struct CoverIndexEntry {
  uint32_t offset;
  uint32_t length;
  uint8_t format; // 0 = qoi, 1 = raw565 - matches indexer's ALBUM_COVER_FORMAT_BYTE
};

struct FormatStats {
  uint32_t count = 0;
  uint32_t totalReadMicros = 0;
  uint32_t totalDecodeMicros = 0;
  uint32_t maxReadMicros = 0;
  uint32_t maxDecodeMicros = 0;
};

class CoverBenchmark {
private:
  SdFat &sd;
  ILI9341_GFX &display;
  uint16_t pixelBuffer[COVER_PIXELS]; // 32KB - the shared render target
  uint8_t qoiReadBuffer[MAX_QOI_BYTES];

  // Reads the dense album_to_cover.bin layout directly (not through
  // MusicLookup - this benchmark intentionally stays independent of the
  // production PSRAM-backed music index so it can run standalone).
  bool loadCoverIndex(const char *path, CoverIndexEntry *out,
                      uint32_t maxEntries, uint32_t &outCount) {
    FsFile file;
    if (!file.open(path, O_RDONLY)) {
      Serial.print("ERROR: could not open ");
      Serial.println(path);
      return false;
    }
    uint32_t fileSize = file.fileSize();
    uint8_t header[4];
    int headerRead = file.read(header, 4);
    uint32_t entryCount = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
                          ((uint32_t)header[2] << 16) |
                          ((uint32_t)header[3] << 24);

    Serial.print("  loadCoverIndex ");
    Serial.print(path);
    Serial.print(": fileSize=");
    Serial.print(fileSize);
    Serial.print(" headerBytesRead=");
    Serial.print(headerRead);
    Serial.print(" entryCount=");
    Serial.println(entryCount);

    // Sanity check: a well-formed file is exactly 4 + entryCount*9 bytes
    // (indexer's exportAlbumCoverIndexToBinary). If entryCount doesn't
    // agree with the actual file size, something upstream (wrong file
    // copied/truncated on the SD card, or a stale/partial write) produced
    // this file - don't trust it enough to loop over possibly-garbage
    // records.
    if (headerRead != 4 || fileSize != 4 + entryCount * 9) {
      Serial.println("  ERROR: entryCount doesn't match file size - "
                     "refusing to trust this index");
      file.close();
      outCount = 0;
      return false;
    }

    outCount = min(entryCount, maxEntries);
    for (uint32_t i = 0; i < outCount; i++) {
      uint8_t rec[9];
      file.read(rec, 9);
      out[i].offset = (uint32_t)rec[0] | ((uint32_t)rec[1] << 8) |
                      ((uint32_t)rec[2] << 16) | ((uint32_t)rec[3] << 24);
      out[i].length = (uint32_t)rec[4] | ((uint32_t)rec[5] << 8) |
                      ((uint32_t)rec[6] << 16) | ((uint32_t)rec[7] << 24);
      out[i].format = rec[8];
      if (i < 3) {
        Serial.print("    entry ");
        Serial.print(i);
        Serial.print(": offset=");
        Serial.print(out[i].offset);
        Serial.print(" length=");
        Serial.print(out[i].length);
        Serial.print(" format=");
        Serial.println(out[i].format);
      }
    }
    file.close();
    return true;
  }

public:
  CoverBenchmark(SdFat &sdCard, ILI9341_GFX &disp)
      : sd(sdCard), display(disp) {}

  // Runs one format's full sweep: loads every cover in coverIndexPath from
  // thumbsPath, times SD read (and decode, for QOI) per cover, renders each
  // to the screen, and returns the aggregate stats.
  FormatStats runSweep(const char *label, const char *thumbsPath,
                      const char *coverIndexPath) {
    FormatStats stats;

    static CoverIndexEntry entries[256];
    uint32_t count = 0;
    if (!loadCoverIndex(coverIndexPath, entries, 256, count)) {
      return stats;
    }

    FsFile thumbsFile;
    if (!thumbsFile.open(thumbsPath, O_RDONLY)) {
      Serial.print("ERROR: could not open ");
      Serial.println(thumbsPath);
      return stats;
    }

    Serial.print("=== ");
    Serial.print(label);
    Serial.println(" sweep ===");

    for (uint32_t i = 0; i < count; i++) {
      const CoverIndexEntry &entry = entries[i];
      if (entry.length == 0) continue; // no cover resolved for this album

      uint32_t t0 = micros();
      thumbsFile.seekSet(entry.offset);

      bool decoded = false;
      uint32_t decodeMicros = 0;

      if (entry.format == 1) {
        // raw565: the bytes ARE the pixels, straight read into the target.
        if (entry.length > COVER_BYTES_565) {
          Serial.print("  album ");
          Serial.print(i);
          Serial.println(": skipped, cover exceeds COVER_BYTES_565 (bad "
                        "entry or wrong file?)");
          continue;
        }
        thumbsFile.read((uint8_t *)pixelBuffer, entry.length);
        uint32_t t1 = micros();
        stats.totalReadMicros += (t1 - t0);
        stats.maxReadMicros = max(stats.maxReadMicros, t1 - t0);
        decoded = true;
      } else {
        if (entry.length > MAX_QOI_BYTES) {
          Serial.println("  skipped: cover exceeds MAX_QOI_BYTES");
          continue;
        }
        thumbsFile.read(qoiReadBuffer, entry.length);
        uint32_t t1 = micros();
        stats.totalReadMicros += (t1 - t0);
        stats.maxReadMicros = max(stats.maxReadMicros, t1 - t0);

        uint32_t t2 = micros();
        decoded = decodeQoiToRgb565(qoiReadBuffer, entry.length, pixelBuffer,
                                    COVER_PIXELS);
        uint32_t t3 = micros();
        decodeMicros = t3 - t2;
        stats.totalDecodeMicros += decodeMicros;
        stats.maxDecodeMicros = max(stats.maxDecodeMicros, decodeMicros);
      }

      if (!decoded) {
        Serial.print("  album ");
        Serial.print(i);
        Serial.println(": decode FAILED");
        continue;
      }

      stats.count++;
      display.pushWindow(0, 0, 128, 128, pixelBuffer);

      Serial.print("  album ");
      Serial.print(i);
      Serial.print(": read=");
      Serial.print(micros() - t0 - decodeMicros);
      Serial.print("us decode=");
      Serial.print(decodeMicros);
      Serial.println("us");

      delay(150); // let each cover be visible for a moment
    }

    thumbsFile.close();
    return stats;
  }

  void printStats(const char *label, const FormatStats &stats) {
    Serial.print("--- ");
    Serial.print(label);
    Serial.println(" summary ---");
    Serial.print("  covers rendered: ");
    Serial.println(stats.count);
    if (stats.count == 0) return;
    Serial.print("  avg read: ");
    Serial.print(stats.totalReadMicros / stats.count);
    Serial.print("us  max read: ");
    Serial.print(stats.maxReadMicros);
    Serial.println("us");
    Serial.print("  avg decode: ");
    Serial.print(stats.totalDecodeMicros / stats.count);
    Serial.print("us  max decode: ");
    Serial.print(stats.maxDecodeMicros);
    Serial.println("us");
    Serial.print("  avg total (read+decode): ");
    Serial.print((stats.totalReadMicros + stats.totalDecodeMicros) / stats.count);
    Serial.println("us");
  }

  // Caches one already-decoded cover's pixels into the reserved PSRAM
  // COVER_BUFFER region, then times reading them back out - answers whether
  // a PSRAM cache is worth adding to the production path, or whether
  // reading straight from SD each time (this benchmark's main sweep above)
  // is already fast enough on its own.
  void benchmarkPsramCacheRoundtrip() {
    Serial.println("=== PSRAM cover-cache roundtrip ===");
    // pixelBuffer already holds the last-rendered cover from runSweep().
    uint32_t t0 = micros();
    psram.writeData(COVER_BUFFER_BASE_ADDRESS, (uint8_t *)pixelBuffer,
                    COVER_BYTES_565);
    uint32_t t1 = micros();
    Serial.print("  PSRAM write (32KB): ");
    Serial.print(t1 - t0);
    Serial.println("us");

    static uint16_t readBack[COVER_PIXELS];
    uint32_t t2 = micros();
    psram.readData(COVER_BUFFER_BASE_ADDRESS, (uint8_t *)readBack,
                   COVER_BYTES_565);
    uint32_t t3 = micros();
    Serial.print("  PSRAM read (32KB): ");
    Serial.print(t3 - t2);
    Serial.println("us");

    display.pushWindow(0, 0, 128, 128, readBack);
  }
};

} // namespace onboard_bench
