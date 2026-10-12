# Roadmap

## UI / Header

- [ ] **Scrolling breadcrumb**: the header title (album/artist/genre name) is
      currently truncated with an ellipsis when it overflows the header width.
      Later, animate a marquee/scroll of the text so long names are fully
      readable instead of being cut off.

## Fonts / text

- [ ] **Non-ASCII (accented) glyphs**: the embedded fonts only cover ASCII
      `32..126` (`scripts/font-converter.py` uses `first_char=32, last_char=126`),
      so accented characters are currently stripped at indexing time
      (`indexer/src/utils.ts::sanitizeName`). To display accented names:
      1. Extend `scripts/font-converter.py` to emit the needed Latin-1 / accented
         glyphs (e.g. `0xC0..0xFF`), and
      2. Either store display names in Latin-1 (single-byte) or add UTF-8
         decoding to `font_renderer.h` (which currently treats each byte as one
         glyph).
