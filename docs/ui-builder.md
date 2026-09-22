---
aside: false
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

Tweak the OpenPod color scheme and export the sprites as RGB565 arrays that
drop straight into the firmware. The previews render the real 320×240 layouts (now
playing, track list and search), and every sprite is rasterized from the same
definitions that the export uses, so what you see is byte-for-byte what you get.

::: tip Panel wiring
The ILI9341's subpixels are physically BGR, but the MADCTL `BGR` bit already
compensates for that in hardware, so the firmware sends plain **RGB565** — the same
convention as `theme.h`'s `HEX_TO_RGB565()` macro. Keep the **Packing** switch on
*RGB565* for OpenPod; switch to *BGR565* only for a panel configured the other way.
:::

<UIBuilder />

## Using the export

- **sprites.h** — the sprite pixel arrays (and 1-bit masks for `play` / `pause` /
  `speaker`), a `Sprite` struct, and `blitSprite` / `blitSpriteToDisplay` helpers,
  in the same shape as `scripts/gen-sprites.py` produces.
- **theme.h** — the `COLOR_*` macros for the palette you picked, including the list
  colors (`COLOR_TEXT_HI`, `COLOR_SEPARATOR`, `COLOR_SCROLL_TRACK`, `COLOR_RIBBON`).

The three tinted glyphs (`play`, `pause`, `speaker`) ship both a mask and a
pre-tinted pixel array, so you can either blit them through the mask with a runtime
tint, or blit the pixels directly.
