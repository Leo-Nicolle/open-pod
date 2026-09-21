---
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

Tweak the Now Playing color scheme and export the sprites as RGB565 arrays that
drop straight into the firmware. The preview renders the real 320×240 layout, and
every sprite is rasterized from the same definitions that the export uses, so what
you see is byte-for-byte what you get.

::: tip Panel wiring
The ILI9341 is wired **BGR** (MADCTL `BGR` bit set), so sprites are packed with the
blue channel in the high bits — the same convention as `theme.h`'s `BGR565()` macro.
Keep the **Packing** switch on *BGR565* for the OpenPod firmware; switch to *RGB565*
only if you're targeting a panel wired the other way.
:::

<UIBuilder />

## Using the export

- **sprites.h** — the sprite pixel arrays (and 1-bit masks for `play` / `pause` /
  `speaker`), a `Sprite` struct, and `blitSprite` / `blitSpriteToDisplay` helpers,
  in the same shape as `scripts/gen-sprites.py` produces.
- **theme.h** — the `COLOR_*` macros for the palette you picked.

The three tinted glyphs (`play`, `pause`, `speaker`) ship both a mask and a
pre-tinted pixel array, so you can either blit them through the mask with a runtime
tint, or blit the pixels directly.
