#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "../pinout.h"
#include "../sound/AudioProfiler.h"
#include "../logging.h"

// APS6404L-3SQR-SN is a 64-Megabit chip, i.e. 8 MB - not 64 MB. This used to
// be defined as (64 * 1024 * 1024) (confusing Mbit with MB), which made
// MUSIC_INDEX_SIZE below claim ~58 MB of address space that doesn't
// physically exist: any write past the real 8 MB boundary would wrap around
// and silently corrupt AUDIO_BUFFER_SIZE's region at address 0. Keep this in
// sync with SPI_PSRAM::getCapacity() below, which was already correct.
#define PSRAM_SIZE                (8 * 1024 * 1024) // 8 MB PSRAM chip size
#define AUDIO_BUFFER_BASE_ADDRESS 0x000
// Current-track ring buffer. See docs/blog/09-10.md for the full 8 MB
// budget (4 MB current-track + 1 MB cover buffer + 1 MB next-track prefetch
// + 2 MB music index). 1 MB was reallocated from this buffer (was 5 MB) to
// COVER_BUFFER_SIZE below for the on-device QOI-vs-raw565 cover benchmark.
#define AUDIO_BUFFER_SIZE         (4 * 1024 * 1024) // 4 MB
// Scratch region for album-art decode/cache experiments (see
// software/test/onboard/'s cover format benchmark). Not yet claimed by the
// production cover-fetch path - that's decided by the benchmark's results.
#define COVER_BUFFER_BASE_ADDRESS (AUDIO_BUFFER_BASE_ADDRESS + AUDIO_BUFFER_SIZE)
#define COVER_BUFFER_SIZE         (1 * 1024 * 1024) // 1 MB
// Linear (non-ring) scratch region used to prefetch the start of the next
// track while the current one is still playing, so a track change usually
// needs zero fresh SD activity - see Audio_buffer::prepareNextTrack().
#define NEXT_TRACK_BUFFER_BASE_ADDRESS (COVER_BUFFER_BASE_ADDRESS + COVER_BUFFER_SIZE)
#define NEXT_TRACK_BUFFER_SIZE    (1 * 1024 * 1024) // 1 Mb
#define MUSIC_INDEX_BASE_ADDRESS  (NEXT_TRACK_BUFFER_BASE_ADDRESS + NEXT_TRACK_BUFFER_SIZE)
#define MUSIC_INDEX_SIZE          (PSRAM_SIZE - AUDIO_BUFFER_SIZE - COVER_BUFFER_SIZE - NEXT_TRACK_BUFFER_SIZE) // Remaining PSRAM size for music index
// The music region holds the podcast listening state (state/podcast_state.h,
// 20 B per episode: 6,553 episodes) and, after it, the cache of index pages
// read from the SD card (storage/PageStore.h). The index itself stays on the
// card, so it can be any size.
#define PODCAST_STATE_BASE_ADDRESS MUSIC_INDEX_BASE_ADDRESS
#define PODCAST_STATE_SIZE        (128 * 1024)
#define INDEX_CACHE_BASE_ADDRESS  (PODCAST_STATE_BASE_ADDRESS + PODCAST_STATE_SIZE)
#define INDEX_CACHE_SIZE          (MUSIC_INDEX_SIZE - PODCAST_STATE_SIZE)

static_assert(AUDIO_BUFFER_SIZE + COVER_BUFFER_SIZE + NEXT_TRACK_BUFFER_SIZE < PSRAM_SIZE,
             "Audio + cover buffer + next-track prefetch regions must leave room in PSRAM for the music index");

// SPI PSRAM interface for APS6404L-3SQR-SN using Arduino SPI library
class SPI_PSRAM {
private:
  SPIClass *spi;
  SPISettings spiSettings;
  bool initialized = false;
  // APS6404L Commands
  static const uint8_t CMD_RESET_ENABLE = 0x66;
  static const uint8_t CMD_RESET = 0x99;
  static const uint8_t CMD_READ_ID = 0x9F;
  static const uint8_t CMD_READ = 0x03;
  static const uint8_t CMD_FAST_READ = 0x0B;
  static const uint8_t CMD_WRITE = 0x02;
  static const uint8_t CMD_ENTER_QUAD = 0x35;
  static const uint8_t CMD_EXIT_QUAD = 0xF5;
  static const uint8_t CMD_FAST_READ_QUAD = 0xEB;
  static const uint8_t CMD_WRITE_QUAD = 0x38;
  static const uint8_t CMD_WRAP_TOGGLE = 0xC0;

  // Bus speed and burst shape (see init() and setMaxBurst()).
  uint32_t clockHz = 0;
  // Payload bytes per CE# low period; 0 = unlimited (one burst per call).
  uint32_t maxBurst = 0;
  // false = the old byte-per-call SPI.transfer() path, kept for the link
  // bench to compare against.
  bool bulk = true;
#ifndef UNIT_TEST
  GPIO_TypeDef *csPort = nullptr;
  uint32_t csMask = 0;
#endif

  // APS6404L tCEM: CE# may stay low for at most 8 us (standard grade),
  // otherwise the chip can't run its self-refresh and data can be lost. A
  // linear burst may also cross a 1 KB page only once. Both are respected by
  // splitting every access into bursts of at most maxBurst payload bytes.
  static const uint32_t TCEM_US = 8;
  static const uint32_t MIN_USEFUL_BURST = 12;
  static const uint32_t SCRATCH_ADDRESS = COVER_BUFFER_BASE_ADDRESS;

  void setupPins() {
    pinMode(PSRAM_CS, OUTPUT);
    digitalWrite(PSRAM_CS, HIGH);
#ifndef UNIT_TEST
    csPort = digitalPinToPort(PSRAM_CS);
    csMask = digitalPinToBitMask(PSRAM_CS);
#endif
    spi = new SPIClass(PSRAM_MOSI, PSRAM_MISO, PSRAM_CLK);
    spi->begin();
  }

  inline void chipSelect(bool active) {
#ifdef UNIT_TEST
    digitalWrite(PSRAM_CS, active ? LOW : HIGH);
#else
    csPort->BSRR = active ? (csMask << 16) : csMask;
#endif
  }

  // Clocks n bytes out (tx, or 0x00 when null) and stores what comes back
  // into rx (when non-null).
  void xfer(const uint8_t *tx, uint8_t *rx, uint32_t n) {
#ifndef UNIT_TEST
    if (bulk) {
      // PSRAM_CLK/MISO/MOSI (PB13/14/15) are SPI2. Keep one byte queued in
      // DR while the previous one shifts, so bytes go out back to back
      // (the core's spi_transfer() waits for each byte to come back before
      // sending the next one). With two bytes in flight, RXNE must be
      // serviced within one byte time or the next byte overruns it, so each
      // segment runs with interrupts held off (a few us); an overrun still
      // ends the transfer instead of waiting forever for the lost byte.
      static const uint32_t SEGMENT = 16;
      SPI_TypeDef *r = SPI2;
      r->CR1 |= SPI_CR1_SPE;
      while (r->SR & SPI_SR_RXNE) {
        (void)*(volatile uint8_t *)&r->DR;
      }
      for (uint32_t base = 0; base < n; base += SEGMENT) {
        uint32_t end = (n - base > SEGMENT) ? base + SEGMENT : n;
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        uint32_t sent = base, received = base;
        while (received < end) {
          uint32_t sr = r->SR;
          if (sr & SPI_SR_OVR) {
            (void)*(volatile uint8_t *)&r->DR;
            (void)r->SR;
            received = end; // data lost: give up on this segment
            break;
          }
          if (sent < end && sent - received < 2 && (sr & SPI_SR_TXE)) {
            *(volatile uint8_t *)&r->DR = tx ? tx[sent] : 0;
            sent++;
          }
          if (sr & SPI_SR_RXNE) {
            uint8_t v = *(volatile uint8_t *)&r->DR;
            if (rx) rx[received] = v;
            received++;
          }
        }
        while (r->SR & SPI_SR_BSY) {
        }
        __set_PRIMASK(primask);
      }
      return;
    }
#endif
    for (uint32_t i = 0; i < n; i++) {
      uint8_t v = spi->transfer(tx ? tx[i] : 0);
      if (rx) rx[i] = v;
    }
  }

  // Runs one burst (header + payload) with interrupts held off when bursts
  // are bounded, so an ISR can't stretch CE# low past tCEM.
  void burst(const uint8_t *hdr, uint32_t hdrLen, const uint8_t *tx,
             uint8_t *rx, uint32_t n) {
#ifndef UNIT_TEST
    uint32_t primask = __get_PRIMASK();
    if (maxBurst) __disable_irq();
#endif
    chipSelect(true);
    xfer(hdr, nullptr, hdrLen);
    xfer(tx, rx, n);
    chipSelect(false);
#ifndef UNIT_TEST
    __set_PRIMASK(primask);
#endif
  }

  // Writes and reads back a pattern in the scratch (cover) region.
  bool selfTest() {
    static const uint32_t LEN = 1024;
    uint8_t buf[256];
    uint32_t seed = 0x1234567u ^ clockHz;
    for (uint32_t off = 0; off < LEN; off += sizeof(buf)) {
      for (uint32_t i = 0; i < sizeof(buf); i++) {
        seed = seed * 1103515245u + 12345u;
        buf[i] = (uint8_t)(seed >> 16);
      }
      writeData(SCRATCH_ADDRESS + off, buf, sizeof(buf));
    }
    seed = 0x1234567u ^ clockHz;
    for (uint32_t off = 0; off < LEN; off += sizeof(buf)) {
      readData(SCRATCH_ADDRESS + off, buf, sizeof(buf));
      for (uint32_t i = 0; i < sizeof(buf); i++) {
        seed = seed * 1103515245u + 12345u;
        if (buf[i] != (uint8_t)(seed >> 16)) return false;
      }
    }
    return true;
  }

public:
  SPI_PSRAM() : spi(nullptr) {
    // Conservative speed for reset + ID; init() then probes for the fastest
    // speed that passes a pattern test.
    setClock(8000000);
  }

  // Requested SPI clock; the STM32 core rounds down to APB1 (45 MHz) / 2^k,
  // so 22.5, 11.25, 5.625 MHz... (see actualClockHz()).
  void setClock(uint32_t hz) {
    clockHz = hz;
    spiSettings = SPISettings(hz, MSBFIRST, SPI_MODE0);
  }

  uint32_t actualClockHz() const {
#ifdef UNIT_TEST
    return clockHz;
#else
    uint32_t f = HAL_RCC_GetPCLK1Freq() / 2;
    while (f > clockHz && f > HAL_RCC_GetPCLK1Freq() / 256) f /= 2;
    return f;
#endif
  }

  // Largest payload whose burst (5 header bytes + 2 bytes of slack for
  // CS/call overhead) still fits in tCEM at the current clock.
  uint32_t tcemBurstBytes() const {
    uint32_t bytesInTcem = actualClockHz() / 1000000UL * TCEM_US / 8;
    return bytesInTcem > 8 ? bytesInTcem - 7 : 1;
  }

  uint32_t getClock() const { return clockHz; }
  void setMaxBurst(uint32_t bytes) { maxBurst = bytes; }
  uint32_t getMaxBurst() const { return maxBurst; }
  void setBulk(bool enable) { bulk = enable; }

  ~SPI_PSRAM() {
    if (spi) {
      spi->end();
      delete spi;
    }
  }

  bool init() {
    if (initialized) return true;
    LOG_INFO("=== PSRAM INIT START ===");
    Serial.flush();

    setupPins();
    delay(1); // T_POWER_UP

    // Reset + read the ID a few times; the chip is unreliable right after
    // power-up, so do a full reset (not just a re-read) on each attempt.
    for (int attempt = 0; attempt < 5; attempt++) {
      resetDevice();
      delay(1); // reset recovery

      uint8_t id[2] = {0, 0};
      readDeviceID(id);
      LOG_INFO("PSRAM ID attempt %d: 0x%X 0x%X", (int)attempt, id[0], id[1]);

      if (id[0] == 0x0D && id[1] == 0x5D) {
        initialized = true;
        probeSpeed();
        Serial.flush();
        return true;
      }
      delay(5);
    }

    LOG_ERROR("PSRAM init failed");
    return false;
  }

  void resetDevice() {
    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET_ENABLE);
    chipSelect(false);
    spi->endTransaction();

    delayMicroseconds(1);

    spi->beginTransaction(spiSettings);
    chipSelect(true);
    spi->transfer(CMD_RESET);
    chipSelect(false);
    spi->endTransaction();

    delayMicroseconds(100); // T_RESET
    delay(1); // extra margin: reset recovery
  }

  bool readDeviceID(uint8_t *id) {
    spi->beginTransaction(spiSettings);
    chipSelect(true);

    spi->transfer(CMD_READ_ID);
    spi->transfer(0x00);
    spi->transfer(0x00);
    spi->transfer(0x00);

    id[0] = spi->transfer(0x00);
    id[1] = spi->transfer(0x00);

    chipSelect(false);
    spi->endTransaction();
    return true;
  }

  // Picks the fastest SPI2 speed (22.5 MHz down to 5.6 MHz) that passes a
  // write/read-back pattern test with tCEM-sized bursts.
  void probeSpeed() {
    static const uint32_t speeds[] = {22500000, 11250000, 5625000};
    for (uint32_t hz : speeds) {
      setClock(hz);
      // Below 22.5 MHz a tCEM-sized burst is only a few bytes, slower than
      // the bus is worth: fall back to unbounded bursts (out of spec, as
      // before) rather than crawl.
      setMaxBurst(tcemBurstBytes() >= MIN_USEFUL_BURST ? tcemBurstBytes() : 0);
      if (selfTest()) {
        LOG_INFO("PSRAM OK: SPI %lu Hz, %s",
                 (unsigned long)actualClockHz(),
                 maxBurst ? "tCEM-sized bursts"
                               : "unbounded bursts (tCEM NOT met)");
        return;
      }
      LOG_WARN("PSRAM pattern test failed at %lu Hz",
               (unsigned long)actualClockHz());
    }
    LOG_WARN("PSRAM: no speed passed the pattern test, using 5.6 MHz");
  }

  void writeData(uint32_t address, const uint8_t *data, uint32_t size) {
    if (!data || size == 0) return;
    uint32_t t0 = audioprof::now();
    audioprof::addBytes(audioprof::PSRAM_WRITE, size);

    spi->beginTransaction(spiSettings);
    while (size > 0) {
      uint32_t n = (maxBurst && size > maxBurst) ? maxBurst : size;
      uint8_t hdr[4] = {CMD_WRITE, (uint8_t)(address >> 16),
                        (uint8_t)(address >> 8), (uint8_t)address};
      burst(hdr, sizeof(hdr), data, nullptr, n);
      address += n;
      data += n;
      size -= n;
    }
    spi->endTransaction();
    audioprof::add(audioprof::PSRAM_WRITE, t0);
  }

  void readData(uint32_t address, uint8_t *data, uint32_t size) {
    if (!data || size == 0) return;
    uint32_t t0 = audioprof::now();
    audioprof::addBytes(audioprof::PSRAM_READ, size);

    spi->beginTransaction(spiSettings);
    while (size > 0) {
      uint32_t n = (maxBurst && size > maxBurst) ? maxBurst : size;
      uint8_t hdr[5] = {CMD_FAST_READ, (uint8_t)(address >> 16),
                        (uint8_t)(address >> 8), (uint8_t)address,
                        0x00}; // dummy byte
      burst(hdr, sizeof(hdr), nullptr, data, n);
      address += n;
      data += n;
      size -= n;
    }
    spi->endTransaction();
    audioprof::add(audioprof::PSRAM_READ, t0);
  }

  uint32_t getCapacity() {
    return 8 * 1024 * 1024; // 8MB
  }

  uint32_t getPageSize() {
    return 1024; // 1KB pages
  }
};
extern SPI_PSRAM psram;