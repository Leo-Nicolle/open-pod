#pragma once
// Auto-generated Now Playing theme (see docs/ui-builder).
// Packing: plain RGB565. The ILI9341's MADCTL BGR bit (set in
// ILI9341_GFX::setRotation) compensates for this panel's physically
// BGR-ordered subpixels in hardware, transparently - software always sends
// plain RGB565 and never swaps channels itself. See
// .agents/screen-red-problem.md - the ui-builder's code export has
// repeatedly reintroduced a BGR565 double-swap here; if that happens
// again, switch HEX_TO_BGR565 back to HEX_TO_RGB565 below.
//
// Also see screen-red-problem.md §4 for a SEPARATE bug that lived in the
// RGB565/BGR565 macros themselves for a while (missing parens around each
// bare parameter reference let C's operator precedence corrupt the blue
// channel) - both macros below already have the fix; don't let a
// re-download drop the extra parens.
//
// SCROLLBAR_X is firmware-only state the ui-builder export doesn't know
// about - re-adding it after a re-download is on us, not a design change.

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define BGR565(r, g, b) ((((b) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((r) >> 3))
#define HEX_TO_RGB565(hex) RGB565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)
#define HEX_TO_BGR565(hex) BGR565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)

#define COLOR_BG          HEX_TO_RGB565(0x12100F)
#define COLOR_SURFACE     HEX_TO_RGB565(0x4B4744)
#define COLOR_LINE        HEX_TO_RGB565(0x968D88)
#define COLOR_TEXT        HEX_TO_RGB565(0xFDA326)
#define COLOR_DIM         HEX_TO_RGB565(0xDEDEDE)
#define COLOR_MUTED       HEX_TO_RGB565(0x000000)
#define COLOR_ACCENT      HEX_TO_RGB565(0xEB980A)
#define COLOR_ACCENT_DK   HEX_TO_RGB565(0x664014)
#define COLOR_ACCENT_ALT  HEX_TO_RGB565(0xFFD15C)

#define SCROLLBAR_X 312
