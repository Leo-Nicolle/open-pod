// Display bus throughput benchmark. Built only by the `display_bench` env
// (pio run -e display_bench -t upload, then pio device monitor): measures how
// fast the 16-bit parallel bus pushes pixels, to compare against the 2 MHz
// limit of the backup-domain pins (PC13-PC15), see docs/next-step-pinout.md.
#include "logging.h"
#include "rendering/ILI9341_GFX.h"
#include <Arduino.h>

ILI9341_GFX display;

static const uint32_t W = 320, H = 240, FRAME = W * H;
static uint16_t line[W];

static void startCycles() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// Prints cycles/pixel, Mpx/s, frames/s and the fastest toggle rate of a
// single data line (pattern alternating every pixel -> half the word rate).
static void report(const char *name, uint32_t cycles, uint32_t pixels) {
  uint32_t cpp100 = (uint32_t)((uint64_t)cycles * 100 / pixels);
  uint32_t kpxps = (uint32_t)((uint64_t)pixels * (SystemCoreClock / 1000) / cycles);
  uint32_t fps10 = (uint32_t)((uint64_t)kpxps * 10000 / FRAME);
  LOG_INFO("%-28s %lu.%02lu cyc/px  %lu.%03lu Mpx/s  %lu.%lu fps  line toggle <= %lu.%03lu MHz",
           name, (unsigned long)(cpp100 / 100), (unsigned long)(cpp100 % 100),
           (unsigned long)(kpxps / 1000), (unsigned long)(kpxps % 1000),
           (unsigned long)(fps10 / 10), (unsigned long)(fps10 % 10),
           (unsigned long)(kpxps / 2000), (unsigned long)((kpxps / 2) % 1000));
}

static void benchFill(uint16_t color, int frames) {
  uint32_t t0 = DWT->CYCCNT;
  for (int i = 0; i < frames; i++)
    display.fillScreen(color ^ (i & 1 ? 0xFFFF : 0));
  report("fillScreen", DWT->CYCCNT - t0, FRAME * frames);
}

// Full frames streamed from RAM, one line at a time (the pushPixels path the
// UI uses for text/images).
static void benchPush(const char *name, uint16_t a, uint16_t b, int frames) {
  for (uint32_t x = 0; x < W; x++)
    line[x] = (x & 1) ? b : a;
  uint32_t t0 = DWT->CYCCNT;
  for (int i = 0; i < frames; i++) {
    display.setWindow(0, 0, W - 1, H - 1);
    for (uint32_t y = 0; y < H; y++)
      display.pushPixels(line, W);
  }
  report(name, DWT->CYCCNT - t0, FRAME * frames);
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  display.begin();
  startCycles();
  LOG_INFO("=== Display bus benchmark, SystemCoreClock = %lu Hz ===",
           (unsigned long)SystemCoreClock);
}

void loop() {
  benchFill(0x0000, 10);
  benchPush("pushPixels solid", 0x07E0, 0x07E0, 10);
  benchPush("pushPixels alt 0000/FFFF", 0x0000, 0xFFFF, 10);
  LOG_INFO("---");
  delay(2000);
}
