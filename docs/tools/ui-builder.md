---
aside: false
sidebar: false
head:
  - - link
    - rel: preconnect
      href: https://fonts.googleapis.com
  - - link
    - rel: preconnect
      href: https://fonts.gstatic.com
      crossorigin: ""
  - - link
    - rel: stylesheet
      href: https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;500;600&display=swap
---

# UI builder

Every screen of the opentone firmware, drawn exactly as the device draws it (320×240, RGB565).
**Preview** walks through the screens like a slideshow (←/→ keys work too). **Spec** adds the
implementation details: pixel geometry, the redraw budget, the chunk grid, the sprites and the
title-screen timeline.

Change the palette on the left to re-theme everything. Edits stay in this tab until you press
**Save**, which keeps the palette in this browser.

<UIBuilder />

## Using the export

**Export** downloads `opentone-ui.zip`:

- **sprites.h**: the sprite pixel arrays (and 1-bit masks for `play` / `pause` /
  `speaker`), a `Sprite` struct, and `blitSprite` / `blitSpriteToDisplay` helpers,
  in the same shape as `scripts/gen-sprites.py` produces.
- **theme.h**: the `COLOR_*` macros for your palette.
- **logo.h**: the title screen (logo, landing squash frame, wordmark, theme-derived logo
  palette, `BOOT_*` timeline and blit helpers). Goes to `software/src/ui/logo.h`;
  `ui/boot_screen.hpp` plays it.
- **palette.json**: the palette as hex colours.
- **png/**: an 8× PNG preview of each sprite.

The tinted glyphs (`play`, `pause`, `speaker`) ship both a mask and a pre-tinted pixel array,
so you can either blit them through the mask with a runtime tint, or blit the pixels directly.
The `bt` and `chevron_r` sprites are exported too, but the firmware draws the same rows itself
(`ui/bt_widgets.hpp`).
