#pragma once
// Boot-time throughput bench for every link of the audio path:
//   SD -> MCU (SPI1, SdFat), MCU <-> PSRAM (SPI2), MCU -> VS1053 (SPI1).
// Enabled with enableLinkBench in main.cpp. Prints one line per measurement;
// leaves every bus setting as it found it.

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "AudioPlayer.h"
#include "../storage/Music_lookup.h"
#include "../storage/PSRAM_controller.hpp"

namespace linkbench {

// Allocated only while the bench runs (it's off in normal builds).
static const uint32_t BUF_LEN = 16 * 1024;
static uint8_t *buf = nullptr;

inline uint32_t usSince(uint32_t startCycles) {
  return (DWT->CYCCNT - startCycles) / (SystemCoreClock / 1000000UL);
}

inline void printRate(const char *what, uint32_t bytes, uint32_t us) {
  Serial.printf("  %-44s %7lu B in %7lu us = %5lu KB/s\n", what,
                (unsigned long)bytes, (unsigned long)us,
                (unsigned long)(us ? (uint64_t)bytes * 1000000ULL / us / 1024
                                   : 0));
}

// Depth-first search for the first .flac file (falls back to any .mp3).
inline bool findAudioFile(FsFile &dir, char *path, size_t pathLen, int depth,
                          const char *ext) {
  FsFile entry;
  size_t base = strlen(path);
  while (entry.openNext(&dir, O_RDONLY)) {
    char name[128];
    entry.getName(name, sizeof(name));
    if (name[0] == '.') {
      entry.close();
      continue;
    }
    snprintf(path + base, pathLen - base, "/%s", name);
    if (entry.isDir()) {
      if (depth < 4 && findAudioFile(entry, path, pathLen, depth + 1, ext)) {
        entry.close();
        return true;
      }
    } else {
      const char *dot = strrchr(name, '.');
      if (dot && strcasecmp(dot, ext) == 0) {
        entry.close();
        return true;
      }
    }
    entry.close();
    path[base] = '\0';
  }
  return false;
}

inline void benchSD(SdFat &sd) {
  Serial.println("[bench] SD -> MCU (SPI1, SdFat)");
  char path[256] = "";
  FsFile root = sd.open("/");
  bool found = findAudioFile(root, path, sizeof(path), 0, ".flac");
  root.close();
  if (!found) {
    path[0] = '\0';
    root = sd.open("/");
    found = findAudioFile(root, path, sizeof(path), 0, ".mp3");
    root.close();
  }
  if (!found) {
    Serial.println("  no .flac/.mp3 file found, skipped");
    return;
  }
  Serial.printf("  file: %s\n", path);
  FsFile f = sd.open(path, O_RDONLY);
  static const uint32_t TOTAL = 256 * 1024;
  const uint32_t chunkSizes[] = {512, 4096, BUF_LEN};
  for (uint32_t chunk : chunkSizes) {
    f.seekSet(0);
    uint32_t worst = 0, total = 0;
    uint32_t t0 = DWT->CYCCNT;
    while (total < TOTAL) {
      uint32_t c0 = DWT->CYCCNT;
      int n = f.read(buf, chunk);
      uint32_t us = usSince(c0);
      if (us > worst) worst = us;
      if (n <= 0) break;
      total += n;
    }
    char what[64];
    snprintf(what, sizeof(what), "read %lu B chunks (worst %lu us)",
             (unsigned long)chunk, (unsigned long)worst);
    printRate(what, total, usSince(t0));
  }
  f.close();
}

// Writes then reads back `len` bytes at the scratch (cover) region with the
// current PSRAM settings; reports both rates and any mismatch.
inline void benchPsramConfig(const char *label, uint32_t len) {
  uint32_t addr = COVER_BUFFER_BASE_ADDRESS;
  static const uint32_t CHUNK = 4096;
  for (uint32_t i = 0; i < CHUNK; i++) buf[i] = (uint8_t)(i * 7 + 3);
  uint32_t t0 = DWT->CYCCNT;
  for (uint32_t off = 0; off < len; off += CHUNK) {
    psram.writeData(addr + off, buf, CHUNK);
  }
  uint32_t writeUs = usSince(t0);
  uint32_t errors = 0;
  t0 = DWT->CYCCNT;
  uint32_t readUs = 0;
  for (uint32_t off = 0; off < len; off += CHUNK) {
    uint32_t c0 = DWT->CYCCNT;
    psram.readData(addr + off, buf + CHUNK, CHUNK);
    readUs += usSince(c0);
    for (uint32_t i = 0; i < CHUNK; i++) {
      if (buf[CHUNK + i] != (uint8_t)(i * 7 + 3)) errors++;
    }
  }
  char what[80];
  snprintf(what, sizeof(what), "%s write", label);
  printRate(what, len, writeUs);
  snprintf(what, sizeof(what), "%s read (%lu bad bytes)", label,
           (unsigned long)errors);
  printRate(what, len, readUs);
}

// 1 MB pattern, 1 s of idle, read back: catches data lost to refresh being
// starved by long CE# low periods (tCEM).
inline void benchPsramRetention(const char *label) {
  static const uint32_t LEN = COVER_BUFFER_SIZE;
  static const uint32_t CHUNK = 4096;
  uint32_t addr = COVER_BUFFER_BASE_ADDRESS;
  for (uint32_t off = 0; off < LEN; off += CHUNK) {
    for (uint32_t i = 0; i < CHUNK; i++) buf[i] = (uint8_t)((off + i) * 13 >> 3);
    psram.writeData(addr + off, buf, CHUNK);
  }
  delay(1000);
  uint32_t errors = 0;
  for (uint32_t off = 0; off < LEN; off += CHUNK) {
    psram.readData(addr + off, buf, CHUNK);
    for (uint32_t i = 0; i < CHUNK; i++) {
      if (buf[i] != (uint8_t)((off + i) * 13 >> 3)) errors++;
    }
  }
  Serial.printf("  %-44s %lu bad bytes / 1 MB\n", label,
                (unsigned long)errors);
}

inline void benchPsram() {
  Serial.println("[bench] MCU <-> PSRAM (SPI2)");
  uint32_t savedClock = psram.getClock();
  uint32_t savedBurst = psram.getMaxBurst();
  static const uint32_t LEN = 64 * 1024;

  // What the firmware did before: 8 MHz requested, one SPI.transfer() call
  // per byte, a whole 4 KB access under one CE# low.
  psram.setClock(8000000);
  psram.setBulk(false);
  psram.setMaxBurst(0);
  benchPsramConfig("legacy 5.6MHz bytewise long-burst", LEN);
  benchPsramRetention("legacy retention");

  psram.setBulk(true);
  const uint32_t speeds[] = {5625000, 11250000, 22500000};
  for (uint32_t hz : speeds) {
    psram.setClock(hz);
    char label[64];
    psram.setMaxBurst(0);
    snprintf(label, sizeof(label), "%luk bulk long-burst",
             (unsigned long)(psram.actualClockHz() / 1000));
    benchPsramConfig(label, LEN);
    psram.setMaxBurst(psram.tcemBurstBytes());
    snprintf(label, sizeof(label), "%luk bulk tCEM %luB bursts",
             (unsigned long)(psram.actualClockHz() / 1000),
             (unsigned long)psram.getMaxBurst());
    benchPsramConfig(label, LEN);
  }
  psram.setClock(22500000);
  psram.setMaxBurst(0);
  benchPsramRetention("22.5MHz long-burst retention");
  psram.setMaxBurst(psram.tcemBurstBytes());
  benchPsramRetention("22.5MHz tCEM retention");

  psram.setClock(savedClock);
  psram.setMaxBurst(savedBurst);
}

// Raw SPI1 clocking cost at the VS1053 SDI speeds, with no chip selected
// (nothing listens, no DREQ involved): the old one-call-per-byte path vs the
// bulk 32-byte bursts sendData() now uses.
inline void benchVs1053(AudioPlayer &player) {
  Serial.println("[bench] MCU -> VS1053 SDI (SPI1, clocking only)");
  Serial.printf("  driver: SCI %lu Hz, SDI %lu Hz requested\n",
                (unsigned long)player.getDriver().getSciFreq(),
                (unsigned long)player.getDriver().getSdiFreq());
  static const uint32_t LEN = 8 * 1024;
  memset(buf, 0, LEN);
  const uint32_t speeds[] = {703125, 1406250, 2812500, 5625000, 11250000};
  for (uint32_t hz : speeds) {
    char label[64];
    SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
    uint32_t t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < LEN; i++) SPI.transfer(buf[i]);
    uint32_t us = usSince(t0);
    snprintf(label, sizeof(label), "%luk byte per call", (unsigned long)(hz / 1000));
    printRate(label, LEN, us);
    t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < LEN; i += 32) SPI.transfer(buf + i, nullptr, 32);
    us = usSince(t0);
    snprintf(label, sizeof(label), "%luk 32 B bulk", (unsigned long)(hz / 1000));
    printRate(label, LEN, us);
    SPI.endTransaction();
  }
}

} // namespace linkbench

inline void runLinkBench(AudioPlayer &player, MusicLookup &lookup) {
  Serial.println("=== LINK BENCH ===");
  linkbench::buf = (uint8_t *)malloc(linkbench::BUF_LEN);
  if (!linkbench::buf) {
    Serial.println("no RAM for the bench buffer, skipped");
    return;
  }
  Serial.printf("core %lu MHz, PCLK1 %lu MHz, PCLK2 %lu MHz\n",
                (unsigned long)(SystemCoreClock / 1000000),
                (unsigned long)(HAL_RCC_GetPCLK1Freq() / 1000000),
                (unsigned long)(HAL_RCC_GetPCLK2Freq() / 1000000));
  linkbench::benchSD(lookup.getSD());
  linkbench::benchPsram();
  linkbench::benchVs1053(player);
  free(linkbench::buf);
  linkbench::buf = nullptr;
  Serial.println("=== LINK BENCH DONE ===");
  Serial.flush();
}
