#include "PageStore.h"
#include "../logging.h"
#include "../sound/AudioYield.h"
#include "PSRAM_controller.hpp"
#include <Arduino.h>
#include <string.h>

PageStore pageStore;

namespace {
const uint32_t PACK_MAGIC = 0x5849504Fu; // "OPIX"
const uint16_t PACK_VERSION = 2;

uint32_t le32(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}
} // namespace

void PageStore::begin(uint32_t psramBase, uint32_t psramSize) {
  base = psramBase;
  uint32_t n = psramSize / PAGE_SIZE;
  if (n > MAX_SLOTS)
    n = MAX_SLOTS;
  ways = (uint8_t)(n < WAYS ? n : WAYS);
  sets = ways ? (uint16_t)(n / ways) : 0;
  slots = (uint16_t)(sets * ways);
  clear();
}

void PageStore::clear() {
  for (uint16_t i = 0; i < MAX_SLOTS; i++) {
    slotKey[i] = EMPTY;
  }
  memset(refBits, 0, sizeof(refBits));
  memset(hands, 0, sizeof(hands));
  lastKey = EMPTY;
  lastSlot = NONE;
}

int PageStore::open(SdFat &sd, const char *path, uint8_t pack) {
  if (pack >= MAX_PACKS) {
    return -1;
  }
  close(pack);
  files[pack] = sd.open(path, O_RDONLY);
  if (!files[pack].isOpen()) {
    return -1;
  }
  uint8_t h[12];
  if (files[pack].read(h, sizeof(h)) != sizeof(h) || le32(h) != PACK_MAGIC ||
      (uint16_t)(h[4] | (h[5] << 8)) != PACK_VERSION ||
      files[pack].size() > 0x7FFFFFFFu) {
    LOG_ERROR("%s: not an index pack of version %u", path, PACK_VERSION);
    files[pack].close();
    return -1;
  }
  if (files[pack].size() > ((uint64_t)MAX_PAGES << PAGE_STORE_PAGE_LOG2)) {
    LOG_ERROR("%s is too big: %lu KB, the page cache addresses %lu KB", path,
              (unsigned long)(files[pack].size() / 1024),
              (unsigned long)(((uint32_t)MAX_PAGES * PAGE_SIZE) / 1024));
    files[pack].close();
    return -1;
  }
  fileSize[pack] = (uint32_t)files[pack].size();
  // A contiguous file (the usual case: the syncer writes it in one go) is
  // flagged so, and SdFat then seeks in it without walking the FAT: a
  // third of a page miss otherwise, in a pack of tens of MB.
  if (!files[pack].contiguousRange(nullptr, nullptr)) {
    LOG_WARN("%s is fragmented on the card: index reads are slower", path);
  }
  return pack;
}

void PageStore::close(uint8_t pack) {
  if (pack >= MAX_PACKS) {
    return;
  }
  if (files[pack].isOpen()) {
    files[pack].close();
  }
  fileSize[pack] = 0;
  // Drop the pack's pages: the file may be a different one next time.
  for (uint16_t i = 0; i < slots; i++) {
    if (slotKey[i] != EMPTY && (slotKey[i] >> KEY_PAGE_BITS) == pack) {
      slotKey[i] = EMPTY;
    }
  }
  lastKey = EMPTY;
  lastSlot = NONE;
}

bool PageStore::find(uint8_t pack, uint32_t id, PackRef &out) {
  if (!isOpen(pack)) {
    return false;
  }
  PackRef file = {((uint32_t)pack << 31), fileSize[pack]};
  uint32_t count = readU32(file, 8);
  for (uint32_t i = 0; i < count; i++) {
    uint8_t e[12];
    if (!read(file, 12 + i * 12, e, sizeof(e))) {
      return false;
    }
    if (le32(e) == id) {
      uint32_t offset = le32(e + 4);
      out.length = le32(e + 8);
      out.pos = ((uint32_t)pack << 31) | offset;
      return offset <= fileSize[pack] && out.length <= fileSize[pack] - offset;
    }
  }
  return false;
}

bool PageStore::read(const PackRef &s, uint32_t off, void *dst, uint32_t len) {
  uint8_t *out = (uint8_t *)dst;
  if (len == 0) {
    return true;
  }
  if (!isOpen(s.pack()) || slots == 0 || off > s.length ||
      len > s.length - off) {
    memset(out, 0, len);
    return false;
  }
  uint32_t pos = s.offset() + off;
  while (len > 0) {
    uint32_t page = pos >> PAGE_STORE_PAGE_LOG2;
    uint32_t inPage = pos & (PAGE_SIZE - 1);
    uint32_t n = PAGE_SIZE - inPage;
    if (n > len) {
      n = len;
    }
    // page < MAX_PAGES: the read is inside the pack (open() checked its size).
    uint16_t key = keyOf(s.pack(), page);
    uint16_t slot = lookup(key);
    if (slot == NONE) {
      slot = load(key);
      if (slot == NONE) {
        memset(out, 0, len);
        return false;
      }
    }
    psram.readData(base + (uint32_t)slot * PAGE_SIZE + inPage, out, n);
    out += n;
    pos += n;
    len -= n;
  }
  return true;
}

uint16_t PageStore::lookup(uint16_t key) {
  if (key == lastKey) {
    st.hits++;
    setRef(lastSlot);
    return lastSlot;
  }
  uint16_t first = (uint16_t)(setOf(key) * ways);
  for (uint16_t i = first; i < first + ways; i++) {
    if (slotKey[i] == key) {
      st.hits++;
      setRef(i);
      lastKey = key;
      lastSlot = i;
      return i;
    }
  }
  return NONE;
}

bool PageStore::takeRef(uint16_t slot) {
  uint8_t mask = (uint8_t)(1 << (slot & 7));
  bool was = refBits[slot >> 3] & mask;
  refBits[slot >> 3] &= (uint8_t)~mask;
  return was;
}

uint16_t PageStore::load(uint16_t key) {
  uint32_t t0 = micros();
  // CLOCK within the page's set: the first slot that is empty or wasn't used
  // since the hand last passed it.
  uint16_t set = setOf(key);
  uint16_t first = (uint16_t)(set * ways);
  uint16_t victim;
  for (;;) {
    victim = (uint16_t)(first + hands[set]);
    hands[set] = (uint8_t)((hands[set] + 1) % ways);
    if (slotKey[victim] == EMPTY || !takeRef(victim)) {
      break;
    }
  }
  slotKey[victim] = EMPTY;
  if (lastSlot == victim) {
    lastKey = EMPTY;
    lastSlot = NONE;
  }

  FsFile &f = files[key >> KEY_PAGE_BITS];
  uint32_t filePos = (uint32_t)(key & KEY_PAGE_MASK) << PAGE_STORE_PAGE_LOG2;
  uint32_t ts = micros();
  if (!f.seekSet(filePos)) {
    return NONE;
  }
  st.seekUs += micros() - ts;
  // Through the lent scratch buffer, else one SD sector at a time on the
  // stack (no heap to spare).
  uint8_t sector[512];
  uint8_t *buf = scratch ? scratch : sector;
  uint32_t step = scratch ? scratchSize : sizeof(sector);
  if (step > PAGE_SIZE)
    step = PAGE_SIZE;
  uint32_t addr = base + (uint32_t)victim * PAGE_SIZE;
  for (uint32_t done = 0; done < PAGE_SIZE; done += step) {
    size_t got = f.read(buf, step);
    if (got < step) {
      memset(buf + got, 0, step - got); // the pack's last page
    }
    psram.writeData(addr + done, buf, step);
    if (got < step) {
      // Nothing left to read: the rest of the page is never asked for
      // (reads stay inside their section).
      break;
    }
    audioYield();
  }

  slotKey[victim] = key;
  setRef(victim);
  lastKey = key;
  lastSlot = victim;
  st.misses++;
  st.missUs += micros() - t0;
  return victim;
}
