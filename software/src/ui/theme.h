#pragma once

#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define BGR565(r, g, b) (((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3))
#define HEX_TO_RGB565(hex) RGB565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)
#define HEX_TO_BGR565(hex) BGR565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)

// Usage with hex values:
#define COLOR_PRIMARY    HEX_TO_BGR565(0x1E40AF)
#define COLOR_SECONDARY  HEX_TO_BGR565(0xA1D6B2)
#define COLOR_BACKGROUND HEX_TO_BGR565(0xF8FAFC)
#define COLOR_HIGHLIGHT  HEX_TO_BGR565(0x8B5CF6)
#define COLOR_ACCENT     HEX_TO_BGR565(0x10B981)
#define COLOR_TEXT       HEX_TO_BGR565(0x374151)

#define SCROLLBAR_X 312
#define SCROLLBAR_W 8