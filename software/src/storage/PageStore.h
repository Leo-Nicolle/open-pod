#pragma once
#include "PSRAM_controller.hpp"
#include <SdFat.h>
#include <stdint.h>

// Reads the index packs (/opentone/index.pack, /opentone/podcasts.pack, see
// syncer/crates/core/src/index/pack.rs) straight from the SD card, keeping
// the most recently used PAGE_SIZE pages of them in a PSRAM cache. Nothing
// is copied at boot, so the library can be any size the card holds; a list
// or search touches only the pages it needs.
//
// Pages are pages of the pack file (not of a section), so small sections
// share them. The cache is 4-way set associative (a page can only go in its
// set's 4 slots: no hash table to keep in RAM), with CLOCK (second chance)
// eviction within a set. On a miss the page is read
// a scratch buffer at a time with audioYield() in between, so a cold read
// never starves the audio fifo.

#ifndef PAGE_STORE_PAGE_LOG2
// 4 KB: a miss reads the card in one go (~6.5 ms, the cost is in the
// transfer), and searches touch pages scattered over the pack.
#define PAGE_STORE_PAGE_LOG2 12
#endif

// Where a section lives: which pack (top bit of `pos`), at which byte of it
// (the other 31 bits), how long. 8 bytes: MusicLookup keeps ~30 of them.
struct PackRef {
  uint32_t pos;
  uint32_t length;
  uint8_t pack() const { return (uint8_t)(pos >> 31); }
  uint32_t offset() const { return pos & 0x7FFFFFFFu; }
};

class PageStore {
public:
  static const uint32_t PAGE_SIZE = 1u << PAGE_STORE_PAGE_LOG2;
  static const uint8_t MAX_PACKS = 2;
  // The device's cache region, whole pages: 480 of 4 KB.
  static const uint16_t MAX_SLOTS = INDEX_CACHE_SIZE / PAGE_SIZE;
  static const uint8_t WAYS = 4;
  // Pages per pack: a page number takes 15 bits of a slot key, and the
  // last one would make pack 1's key EMPTY. 128 MB at 4 KB pages (a
  // 100k-track index.pack is ~24 MB).
  static const uint16_t MAX_PAGES = 0x7FFF;

  struct Stats {
    uint32_t hits;
    uint32_t misses;
    uint32_t missUs; // total time spent loading pages
    uint32_t seekUs; // of which seeking in the pack
  };

  // The PSRAM region the cache uses (whole pages, at most MAX_SLOTS).
  void begin(uint32_t psramBase, uint32_t psramSize);

  // A RAM buffer to read pages through (a multiple of 512 bytes). Bigger
  // reads cost much less per byte on the card: 512 at a time, a page takes
  // ~26 ms. Without one, a 512-byte stack buffer.
  void setScratch(uint8_t *buf, uint32_t size) {
    scratch = buf;
    scratchSize = size & ~511u;
  }

  // Opens a pack file; its index (0..MAX_PACKS-1), or -1 if it's missing,
  // not a pack of this version or bigger than MAX_PAGES. Reopening a pack slot drops its cached pages.
  int open(SdFat &sd, const char *path, uint8_t pack);
  void close(uint8_t pack);
  bool isOpen(uint8_t pack) const { return pack < MAX_PACKS && files[pack].isOpen(); }

  // Looks a section up in an open pack's table (read through the cache:
  // the table is in the pack's first page).
  bool find(uint8_t pack, uint32_t id, PackRef &out);

  // Copies [off, off + len) of a section into dst. A read past the end of
  // the section (or of an unopened pack) zero-fills and returns false.
  bool read(const PackRef &s, uint32_t off, void *dst, uint32_t len);
  uint32_t readU32(const PackRef &s, uint32_t off) {
    uint32_t v = 0;
    read(s, off, &v, 4);
    return v;
  }

  const Stats &stats() const { return st; }
  void resetStats() { st = Stats{0, 0, 0, 0}; }
  uint16_t slotCount() const { return slots; }

private:
  static const uint16_t NONE = 0xFFFF;
  static const uint16_t EMPTY = 0xFFFF;
  static const uint8_t KEY_PAGE_BITS = 15;
  static const uint16_t KEY_PAGE_MASK = (1u << KEY_PAGE_BITS) - 1;

  FsFile files[MAX_PACKS];
  uint8_t *scratch = nullptr;
  uint32_t scratchSize = 0;
  uint32_t fileSize[MAX_PACKS] = {0, 0};
  uint32_t base = 0;
  uint16_t slots = 0;
  uint16_t sets = 0;
  uint8_t ways = 0;
  // Per slot: the page it holds (pack << 15 | page number, or EMPTY) and its
  // reference bit; per set, its CLOCK hand. Set s owns slots
  // [s * ways, s * ways + ways).
  uint16_t slotKey[MAX_SLOTS];
  uint8_t refBits[(MAX_SLOTS + 7) / 8];
  uint8_t hands[(MAX_SLOTS + WAYS - 1) / WAYS];
  // The last page hit: binary searches and string reads come back to it.
  uint16_t lastKey = EMPTY;
  uint16_t lastSlot = NONE;
  Stats st = {0, 0, 0, 0};

  static uint16_t keyOf(uint8_t pack, uint32_t page) {
    return (uint16_t)(((uint32_t)pack << KEY_PAGE_BITS) | page);
  }
  uint16_t setOf(uint16_t key) const {
    return (uint16_t)((key + (key >> KEY_PAGE_BITS) * 97u) % sets);
  }
  void clear();
  uint16_t lookup(uint16_t key);
  uint16_t load(uint16_t key);
  void setRef(uint16_t slot) { refBits[slot >> 3] |= (uint8_t)(1 << (slot & 7)); }
  bool takeRef(uint16_t slot);
};

extern PageStore pageStore;
