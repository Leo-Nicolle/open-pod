#pragma once
// A from-scratch QOI decoder, ported line-for-line from
// indexer/src/thumbnails.ts's decodeQOI, for the on-device cover format
// benchmark (see CoverBenchmark.h). If the benchmark favors QOI, this file
// is promoted directly into the production cover module
// (software/src/storage/AlbumArt.h); if raw565 wins, it's deleted.
//
// Differs from the indexer's decoder in one way: it decodes straight into a
// packed RGB565 buffer instead of an intermediate RGB888 buffer, per the
// shared 565 packing contract (R&0xF8)<<8 | (G&0xFC)<<3 | (B>>3) - this
// halves the output buffer needed (32KB instead of 48KB for a 128x128
// image), since the source pixels are already pre-quantized to 5/6/5 levels
// by the indexer (see indexer/src/thumbnails.ts's quantizeTo565InPlace).
#include <stdint.h>

namespace onboard_bench {

inline bool decodeQoiToRgb565(const uint8_t *data, uint32_t length,
                              uint16_t *outPixels565, uint32_t maxPixels,
                              uint32_t *outWidth = nullptr,
                              uint32_t *outHeight = nullptr) {
  if (length < 14) return false;
  if (data[0] != 'q' || data[1] != 'o' || data[2] != 'i' || data[3] != 'f')
    return false;

  uint32_t width = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                    ((uint32_t)data[6] << 8) | (uint32_t)data[7];
  uint32_t height = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                     ((uint32_t)data[10] << 8) | (uint32_t)data[11];
  // data[12] = channels, data[13] = colorspace - not needed to decode.

  uint32_t pixelCount = width * height;
  if (pixelCount == 0 || pixelCount > maxPixels) return false;
  if (outWidth) *outWidth = width;
  if (outHeight) *outHeight = height;

  static const uint8_t QOI_OP_RGB = 0xFE;
  static const uint8_t QOI_MASK_2 = 0xC0;
  static const uint8_t QOI_OP_RUN = 0xC0;
  static const uint8_t QOI_OP_INDEX = 0x00;
  static const uint8_t QOI_OP_DIFF = 0x40;
  // Whatever isn't RGB/RUN/INDEX/DIFF after masking is QOI_OP_LUMA (0x80).

  uint8_t indexR[64];
  uint8_t indexG[64];
  uint8_t indexB[64];
  for (int i = 0; i < 64; i++) {
    indexR[i] = 0;
    indexG[i] = 0;
    indexB[i] = 0;
  }

  uint8_t r = 0, g = 0, b = 0;
  uint32_t p = 14;
  uint32_t px = 0;

  while (px < pixelCount) {
    if (p >= length) return false; // truncated input
    uint8_t byte = data[p++];

    if (byte == QOI_OP_RGB) {
      if (p + 3 > length) return false;
      r = data[p++];
      g = data[p++];
      b = data[p++];
    } else if ((byte & QOI_MASK_2) == QOI_OP_RUN) {
      uint32_t run = (uint32_t)(byte & 0x3F) + 1;
      uint16_t packed = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      for (uint32_t k = 0; k < run && px < pixelCount; k++) {
        outPixels565[px++] = packed;
      }
      continue;
    } else if ((byte & QOI_MASK_2) == QOI_OP_INDEX) {
      uint8_t idx = byte & 0x3F;
      r = indexR[idx];
      g = indexG[idx];
      b = indexB[idx];
    } else if ((byte & QOI_MASK_2) == QOI_OP_DIFF) {
      r = (uint8_t)(r + (((byte >> 4) & 0x03) - 2));
      g = (uint8_t)(g + (((byte >> 2) & 0x03) - 2));
      b = (uint8_t)(b + ((byte & 0x03) - 2));
    } else {
      // QOI_OP_LUMA
      if (p >= length) return false;
      int dg = (int)(byte & 0x3F) - 32;
      uint8_t byte2 = data[p++];
      int drMg = (int)((byte2 >> 4) & 0x0F) - 8;
      int dbMg = (int)(byte2 & 0x0F) - 8;
      g = (uint8_t)(g + dg);
      r = (uint8_t)(r + dg + drMg);
      b = (uint8_t)(b + dg + dbMg);
    }

    uint8_t hash = (uint8_t)((r * 3 + g * 5 + b * 7 + 255 * 11) % 64);
    indexR[hash] = r;
    indexG[hash] = g;
    indexB[hash] = b;
    outPixels565[px++] =
        (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  }

  return true;
}

} // namespace onboard_bench
