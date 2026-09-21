#pragma once

#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define BGR565(r, g, b) (((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3))
#define HEX_TO_RGB565(hex) RGB565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)
#define HEX_TO_BGR565(hex) BGR565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)

// Dark theme for the Now Playing screen (see docs/ui-builder).
// The ILI9341 is wired BGR, so every color uses HEX_TO_BGR565.
#define COLOR_BG         HEX_TO_BGR565(0x12100F) // screen background
#define COLOR_SURFACE    HEX_TO_BGR565(0x1E1B19) // header bar, volume band, art plate
#define COLOR_LINE       HEX_TO_BGR565(0x332E2B) // 1px rules, empty track, borders
#define COLOR_TEXT       HEX_TO_BGR565(0xF2EDE6) // title, elapsed time, clock
#define COLOR_DIM        HEX_TO_BGR565(0x9B918A) // artist, secondary numbers
#define COLOR_MUTED      HEX_TO_BGR565(0x6E655F) // album, total time, inactive chips
#define COLOR_ACCENT     HEX_TO_BGR565(0x31AAA9) // progress fill, active state
#define COLOR_ACCENT_DK  HEX_TO_BGR565(0x6C1A1A) // seek track fill behind the handle
#define COLOR_ACCENT_ALT HEX_TO_BGR565(0xF8E0A4) // seek handle, SEEK chip

#define SCROLLBAR_X 312
#define SCROLLBAR_W 8
