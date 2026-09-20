// Test-only fake audio file content for Audio_buffer tests.
//
// Each "file" is a stream of little-endian uint32 words, one every 4 bytes.
// Word at byte offset `off` encodes (fileId << 28) | off: the top 4 bits say
// which fake file this data belongs to, the bottom 28 bits are that file's
// own byte offset. This makes any chunk of bytes recovered from PSRAM
// self-describing - decoding it directly proves which file it came from and
// exactly where in that file it sits, without needing a separately-kept
// "expected" copy to diff against. That's what lets the Audio_buffer tests
// confirm the ring buffer and the next-track prefetch region never
// cross-contaminate: two fake files' words simply never decode to the same
// (fileId, offset) pair unless the code actually mixed their bytes up.
#pragma once

#include <doctest.h>
#include <stdint.h>

#include <vector>

namespace openpod_test {

// fileId must fit in 4 bits (0-15). sizeBytes is rounded down to a multiple
// of 4 so every word stays 4-byte aligned.
inline std::vector<uint8_t> buildFakeAudioFile(uint8_t fileId,
                                               uint32_t sizeBytes) {
  sizeBytes -= sizeBytes % 4;
  std::vector<uint8_t> out(sizeBytes);
  for (uint32_t offset = 0; offset < sizeBytes; offset += 4) {
    uint32_t word =
        ((uint32_t)(fileId & 0xF) << 28) | (offset & 0x0FFFFFFFu);
    out[offset + 0] = (uint8_t)(word & 0xFF);
    out[offset + 1] = (uint8_t)((word >> 8) & 0xFF);
    out[offset + 2] = (uint8_t)((word >> 16) & 0xFF);
    out[offset + 3] = (uint8_t)((word >> 24) & 0xFF);
  }
  return out;
}

inline uint32_t decodeWord(const uint8_t *data, size_t pos) {
  return (uint32_t)data[pos] | ((uint32_t)data[pos + 1] << 8) |
         ((uint32_t)data[pos + 2] << 16) | ((uint32_t)data[pos + 3] << 24);
}

inline uint8_t wordFileId(uint32_t word) { return (uint8_t)(word >> 28); }
inline uint32_t wordOffset(uint32_t word) { return word & 0x0FFFFFFFu; }

// Failure codes from checkSequentialRunError(), packed the same way as
// buildFakeAudioFile()'s words: top 4 bits are the failure kind, bottom 28
// bits are the byte index into `data` where it was first detected. Every
// failure code is non-zero (kOk == 0), so a failure at index 0 still can't
// be mistaken for success.
enum SequentialRunErrorCode {
  kSequentialRunOk = 0,
  kSequentialRunBadLength = 1,  // len wasn't a multiple of 4
  kSequentialRunWrongFileId = 2,
  kSequentialRunWrongOffset = 3,
};

// Plain C++ scan (no doctest macros) so a large buffer doesn't emit one
// CHECK per word - with `success = true` in test/main.cpp, doctest prints
// every passing CHECK too, which flooded the console on anything but a tiny
// buffer. Returns the first mismatch found, packed as described above, or
// kSequentialRunOk if `data` is exactly expectedFileId's words from
// startOffset, in order, with no gaps/repeats/foreign bytes.
inline uint32_t checkSequentialRunError(const uint8_t *data, size_t len,
                                        uint8_t expectedFileId,
                                        uint32_t startOffset) {
  if (len % 4 != 0) {
    return (uint32_t)kSequentialRunBadLength << 28;
  }
  for (size_t i = 0; i < len; i += 4) {
    uint32_t word = decodeWord(data, i);
    if (wordFileId(word) != expectedFileId) {
      return ((uint32_t)kSequentialRunWrongFileId << 28) |
             ((uint32_t)i & 0x0FFFFFFFu);
    }
    if (wordOffset(word) != startOffset + (uint32_t)i) {
      return ((uint32_t)kSequentialRunWrongOffset << 28) |
             ((uint32_t)i & 0x0FFFFFFFu);
    }
  }
  return kSequentialRunOk;
}

// Single-CHECK wrapper around checkSequentialRunError(), with a decoded
// explanation attached only when it actually fails.
inline void checkSequentialRun(const uint8_t *data, size_t len,
                               uint8_t expectedFileId, uint32_t startOffset) {
  uint32_t err = checkSequentialRunError(data, len, expectedFileId, startOffset);
  if (err != kSequentialRunOk) {
    uint32_t code = err >> 28;
    uint32_t index = err & 0x0FFFFFFFu;
    const char *what = code == kSequentialRunBadLength  ? "length not a multiple of 4"
                       : code == kSequentialRunWrongFileId ? "wrong file id"
                       : code == kSequentialRunWrongOffset ? "wrong byte offset"
                                                            : "unknown";
    INFO("checkSequentialRun failed: " << what << " (code " << code
                                       << ") at byte index " << index);
  }
  CHECK(err == kSequentialRunOk);
}

} // namespace openpod_test
