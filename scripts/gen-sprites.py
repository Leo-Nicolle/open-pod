#!/usr/bin/env python3
"""
Generates the Now Playing UI sprites for the OpenPod firmware.

Sprites are emitted as plain RGB565 uint16 arrays, matching theme.h's
COLOR_* macros (HEX_TO_RGB565). The ILI9341 is configured with the MADCTL
BGR bit set (see ILI9341_GFX::setRotation -> 0xE8 / 0x48) to compensate for
this panel's physically BGR-ordered subpixels in hardware, transparently -
software on both sides (theme.h and here) sends standard RGB565 and lets the
controller handle the panel's subpixel order. (Historical note: sprites used
to be packed BGR565 to match a since-removed workaround in theme.h for what
turned out to be an unrelated dead-GPIO-pin bug - see
.agents/screen-red-problem.md. Don't reintroduce it.)

Outputs:
  software/src/ui/sprites.h        - sprite arrays + Sprite struct + blit helpers
  software/src/ui/sprites/*.png    - scaled previews of every sprite
  docs/nowplaying-mockup.png       - full 320x240 layout mockup (normal + seek)

Re-run after editing:  python3 scripts/gen-sprites.py
"""

import math
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI_DIR = os.path.join(ROOT, "software", "src", "ui")
SPRITE_PREVIEW_DIR = os.path.join(UI_DIR, "sprites")
DOCS_DIR = os.path.join(ROOT, "docs")
SCRIPTS_DIR = os.path.join(ROOT, "scripts")

SCALE = 4  # supersample factor for anti-aliased edges


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


# Palette mirror of software/src/ui/theme.h (keep in sync).
THEME = {
    "primary": hex_rgb("#1E40AF"),       # COLOR_PRIMARY
    "secondary": hex_rgb("#A1D6B2"),     # COLOR_SECONDARY
    "background": hex_rgb("#F8FAFC"),    # COLOR_BACKGROUND
    "highlight": hex_rgb("#8B5CF6"),     # COLOR_HIGHLIGHT
    "accent": hex_rgb("#10B981"),        # COLOR_ACCENT
    "text": hex_rgb("#374151"),          # COLOR_TEXT
    "track": hex_rgb("#E5E7EB"),         # COLOR_TRACK (new)
    "overlay_bg": hex_rgb("#1F2937"),    # COLOR_OVERLAY_BG (new)
    "overlay_track": hex_rgb("#4B5563"), # COLOR_OVERLAY_TRACK (new)
    "shadow": hex_rgb("#D6DCE5"),        # COLOR_SHADOW (new)
    "white": hex_rgb("#FFFFFF"),
}


def rgb565(rgb):
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def render_opaque(size, bg, draw_fn):
    """Draw `draw_fn` at SCALE and downsample, composited onto solid `bg`."""
    w, h = size
    big = Image.new("RGBA", (w * SCALE, h * SCALE), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), w * SCALE, h * SCALE)
    big = big.resize((w, h), Image.LANCZOS)
    out = Image.new("RGBA", (w, h), bg + (255,))
    out.alpha_composite(big)
    return out.convert("RGB")


def render_alpha(size, draw_fn):
    """Return an RGBA image (with real alpha) for 1-bit-masked sprites."""
    w, h = size
    big = Image.new("RGBA", (w * SCALE, h * SCALE), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), w * SCALE, h * SCALE)
    return big.resize((w, h), Image.LANCZOS)


def capsule(color):
    def draw(d, W, H):
        d.rounded_rectangle([0, 0, W - 1, H - 1], radius=H // 2, fill=color)
    return draw


# --- sprite draw callbacks (coordinates are in supersampled px) -------------

def draw_volume_pill(d, W, H):
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=H // 2,
                        fill=THEME["overlay_bg"],
                        outline=hex_rgb("#3B4453"), width=SCALE)


def draw_speaker(d, W, H):
    s = SCALE
    white = THEME["white"]
    # body (driver)
    d.rectangle([10 * s, 5 * s, 13 * s, 9 * s], fill=white)
    # cone (opens left)
    d.polygon([(10 * s, 7 * s), (5 * s, 4 * s), (5 * s, 10 * s)], fill=white)
    # sound waves
    d.arc([0 * s, 4 * s, 4 * s, 10 * s], 270, 90, fill=white, width=max(1, s))
    d.arc([-1 * s, 2 * s, 2 * s, 12 * s], 270, 90, fill=white, width=max(1, s))


def draw_play(d, W, H):
    s = SCALE
    d.polygon([(6 * s, 4 * s), (6 * s, 12 * s), (13 * s, 8 * s)],
              fill=THEME["primary"])


def draw_pause(d, W, H):
    s = SCALE
    d.rounded_rectangle([5 * s, 4 * s, 8 * s, 12 * s], radius=s,
                        fill=THEME["primary"])
    d.rounded_rectangle([11 * s, 4 * s, 14 * s, 12 * s], radius=s,
                        fill=THEME["primary"])


def draw_knob(d, W, H):
    s = SCALE
    c = 8 * s
    d.ellipse([c - 7 * s, c - 7 * s, c + 7 * s, c + 7 * s],
              fill=THEME["text"])
    d.ellipse([c - 5 * s, c - 5 * s, c + 5 * s, c + 5 * s],
              fill=THEME["white"])


# --- sprite definitions ------------------------------------------------------

# (name, size, kind, draw_fn, [bg if opaque])
# kind: "opaque" -> pixels baked on bg; "alpha" -> 1-bit mask.
SPRITES = [
    ("progress_track", (280, 8), "opaque", capsule(THEME["track"]),
     THEME["background"]),
    ("progress_fill", (280, 8), "opaque", capsule(THEME["primary"]),
     THEME["background"]),
    ("volume_pill", (200, 16), "opaque", draw_volume_pill, THEME["background"]),
    ("volume_track", (164, 6), "opaque", capsule(THEME["overlay_track"]),
     THEME["overlay_bg"]),
    ("volume_fill", (164, 6), "opaque", capsule(THEME["accent"]),
     THEME["overlay_bg"]),
    ("speaker", (18, 14), "opaque", draw_speaker, THEME["overlay_bg"]),
    ("play", (16, 16), "opaque", draw_play, THEME["background"]),
    ("pause", (16, 16), "opaque", draw_pause, THEME["background"]),
    ("seek_knob", (16, 16), "alpha", draw_knob, None),
]


def emit_array(name, values, cols=12, suffix=""):
    lines = []
    for i in range(0, len(values), cols):
        chunk = values[i:i + cols]
        lines.append("  " + ", ".join("0x%04X" % v for v in chunk) + ",")
    body = "\n".join(lines)
    return ("const uint16_t %s%s[] = {\n%s\n};\n\n"
            % (name, suffix, body))


def emit_mask(name, mask_bytes, cols=16):
    lines = []
    for i in range(0, len(mask_bytes), cols):
        chunk = mask_bytes[i:i + cols]
        lines.append("  " + ", ".join("0x%02X" % b for b in chunk) + ",")
    body = "\n".join(lines)
    return ("const uint8_t %s_mask[] = {\n%s\n};\n\n"
            % (name, body))


def rgba_to_rgb565(img):
    w, h = img.size
    px = img.load()
    return [rgb565(px[x, y][:3]) for y in range(h) for x in range(w)]


def rgba_to_mask(img, threshold=128):
    w, h = img.size
    px = img.load()
    bits = []
    for y in range(h):
        for x in range(w):
            bits.append(1 if px[x, y][3] >= threshold else 0)
    out = bytearray((len(bits) + 7) // 8)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 0x80 >> (i & 7)
    return bytes(out)


def build_sprites():
    results = {}
    for name, size, kind, draw_fn, bg in SPRITES:
        if kind == "opaque":
            img = render_opaque(size, bg, draw_fn)
            results[name] = {
                "size": size,
                "pixels": rgba_to_rgb565(img),
                "mask": None,
                "preview": img,
            }
        else:
            img = render_alpha(size, draw_fn)
            rgb = Image.new("RGB", size, (0, 0, 0))
            rgb.paste(img, (0, 0), img)
            results[name] = {
                "size": size,
                "pixels": rgba_to_rgb565(rgb),
                "mask": rgba_to_mask(img),
                "preview": rgb,
                "rgba": img,
            }
    return results


def write_header(sprites):
    lines = []
    lines.append("#pragma once")
    lines.append("// Auto-generated UI sprites for the Now Playing screen.")
    lines.append("// Plain RGB565 packing, matching theme.h's HEX_TO_RGB565 - the ILI9341's")
    lines.append("// MADCTL BGR bit handles this panel's BGR subpixels in hardware, so")
    lines.append("// software never swaps channels. Regenerate: python3 scripts/gen-sprites.py")
    lines.append("// Const arrays live in flash (.rodata) and are read directly, matching")
    lines.append("// the font tables in software/src/fonts (STM32 flat address space).")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("class ILI9341_GFX;")
    lines.append("")
    lines.append("struct Sprite {")
    lines.append("  uint16_t w;")
    lines.append("  uint16_t h;")
    lines.append("  const uint16_t* pixels;")
    lines.append("  const uint8_t* mask;  // 1 bit/px, MSB-first; nullptr = opaque")
    lines.append("};")
    lines.append("")

    for name, size, kind, _, _ in SPRITES:
        data = sprites[name]
        lines.append("// %s  %dx%d" % (name, size[0], size[1]))
        lines.append(emit_array("spr_" + name, data["pixels"]))
        if data["mask"] is not None:
            lines.append(emit_mask("spr_" + name, data["mask"]))

    # Build the sprite table and friendly constants.
    decls = []
    for name, size, kind, _, _ in SPRITES:
        mask = "spr_%s_mask" % name if sprites[name]["mask"] is not None else "nullptr"
        decls.append(
            "static const Sprite SPR_%s = {%d, %d, spr_%s, %s};"
            % (name.upper(), size[0], size[1], name, mask))
    lines.append("")
    lines.append("// Named sprite instances")
    lines.extend(decls)
    lines.append("")

    # Blit helpers
    lines.append("// Blit into a caller-owned RGB565 buffer (chunk rendering), clipped.")
    lines.append("static inline void blitSprite(uint16_t* buf, int bufW, int bufH,")
    lines.append("                              const Sprite& s, int x, int y) {")
    lines.append("  for (int sy = 0; sy < (int)s.h; ++sy) {")
    lines.append("    int dy = y + sy;")
    lines.append("    if (dy < 0 || dy >= bufH) continue;")
    lines.append("    for (int sx = 0; sx < (int)s.w; ++sx) {")
    lines.append("      int dx = x + sx;")
    lines.append("      if (dx < 0 || dx >= bufW) continue;")
    lines.append("      if (s.mask) {")
    lines.append("        int bit = sy * s.w + sx;")
    lines.append("        if (!(s.mask[bit >> 3] & (0x80 >> (bit & 7)))) continue;")
    lines.append("      }")
    lines.append("      buf[dy * bufW + dx] = s.pixels[sy * s.w + sx];")
    lines.append("    }")
    lines.append("  }")
    lines.append("}")
    lines.append("")
    lines.append("// Blit directly to the display (for overlays outside the chunk buffer).")
    lines.append("// Requires ILI9341_GFX to be complete; include ILI9341_GFX.h first.")
    lines.append("static inline void blitSpriteToDisplay(ILI9341_GFX* d, const Sprite& s,")
    lines.append("                                        int x, int y) {")
    lines.append("  for (int sy = 0; sy < (int)s.h; ++sy) {")
    lines.append("    int runStart = -1, runLen = 0;")
    lines.append("    for (int sx = 0; sx <= (int)s.w; ++sx) {")
    lines.append("      bool opaque = sx < (int)s.w;")
    lines.append("      if (opaque && s.mask) {")
    lines.append("        int bit = sy * s.w + sx;")
    lines.append("        opaque = s.mask[bit >> 3] & (0x80 >> (bit & 7));")
    lines.append("      }")
    lines.append("      if (opaque) {")
    lines.append("        if (runStart < 0) runStart = sx;")
    lines.append("        runLen++;")
    lines.append("      } else {")
    lines.append("        if (runStart >= 0) {")
    lines.append("          d->setWindow(x + runStart, y + sy, x + runStart + runLen - 1, y + sy);")
    lines.append("          d->pushPixels((uint16_t*)&s.pixels[sy * s.w + runStart], runLen);")
    lines.append("          runStart = -1;")
    lines.append("          runLen = 0;")
    lines.append("        }")
    lines.append("      }")
    lines.append("    }")
    lines.append("  }")
    lines.append("}")

    path = os.path.join(UI_DIR, "sprites.h")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s" % path)


def write_previews(sprites):
    os.makedirs(SPRITE_PREVIEW_DIR, exist_ok=True)
    for name, data in sprites.items():
        w, h = data["size"]
        zoom = 8 if w <= 32 else 4
        img = data["preview"].resize((w * zoom, h * zoom), Image.NEAREST)
        # draw a faint grid / checker to reveal transparency
        out = Image.new("RGB", img.size, (200, 200, 200))
        if data["mask"] is not None:
            out = checker(w * zoom, h * zoom)
            mask = data["rgba"].resize((w * zoom, h * zoom), Image.NEAREST)
            out.paste(img, (0, 0), mask)
        else:
            out = img
        out.save(os.path.join(SPRITE_PREVIEW_DIR, name + ".png"))
        print("wrote sprite preview %s.png" % name)


def checker(w, h, cell=8):
    img = Image.new("RGB", (w, h), (255, 255, 255))
    d = ImageDraw.Draw(img)
    for y in range(0, h, cell):
        for x in range(0, w, cell):
            if (x // cell + y // cell) % 2 == 0:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(220, 220, 220))
    return img


# --- full-screen mockup ------------------------------------------------------

def load_font(size, bold=False):
    path = os.path.join(SCRIPTS_DIR,
                        "IBMPlexSans-Bold.ttf" if bold else "IBMPlexSans-Regular.ttf")
    return ImageFont.truetype(path, size)


def fake_cover(size=128):
    # A plausible gradient + circle motif so the mockup reads as an album.
    img = Image.new("RGB", (size, size))
    d = ImageDraw.Draw(img)
    for y in range(size):
        t = y / size
        r = int(30 + 200 * t)
        g = int(60 + 120 * t)
        b = int(150 + 60 * t)
        d.line([(0, y), (size, y)], fill=(r, g, b))
    d.ellipse([size // 4, size // 3, 3 * size // 4, 2 * size // 3],
              outline=(255, 255, 255), width=6)
    d.ellipse([size // 2 - 12, size // 2 - 12, size // 2 + 12, size // 2 + 12],
              fill=(255, 255, 255))
    return img


def draw_mockup(seek=False, show_volume=True):
    W, H = 320, 240
    img = Image.new("RGB", (W, H), THEME["background"])
    d = ImageDraw.Draw(img)

    # header
    d.rectangle([0, 0, W, 30], fill=THEME["primary"])
    d.line([(0, 30), (W, 30)], fill=THEME["secondary"])
    hdr_font = load_font(16, bold=True)
    d.text((10, 6), "Now Playing", font=hdr_font, fill=THEME["background"])

    # cover drop shadow + border + art
    cover_x, cover_y = 20, 58
    d.rectangle([cover_x + 3, cover_y + 4, cover_x + 128 + 3, cover_y + 128 + 4],
                fill=THEME["shadow"])
    img.paste(fake_cover(), (cover_x, cover_y))
    d.rectangle([cover_x - 1, cover_y - 1, cover_x + 128, cover_y + 128],
                outline=THEME["primary"], width=1)

    # title block (right column)
    title_font = load_font(16, bold=True)
    title = "Bohemian Rhapsody"
    d.text((168, 118), title, font=title_font, fill=THEME["text"])
    d.text((168, 139), "Live at Wembley", font=title_font, fill=THEME["text"])

    # play/pause state indicator
    state = "pause" if seek else "play"
    _draw_icon(d, state, 284, 84)

    # times
    time_font = load_font(12)
    d.text((20, 190), "1:23", font=time_font, fill=THEME["text"])
    d.text((300 - 40, 190), "4:56", font=time_font, fill=THEME["secondary"])

    # progress bar
    bar_x, bar_y, bar_w, bar_h = 20, 206, 280, 8
    fill_color = THEME["accent"] if seek else THEME["primary"]
    frac = 0.33 if not seek else 0.62
    filled = int(bar_w * frac)
    d.rounded_rectangle([bar_x, bar_y, bar_x + bar_w - 1, bar_y + bar_h - 1],
                        radius=bar_h // 2, fill=THEME["track"])
    if filled > 0:
        d.rounded_rectangle([bar_x, bar_y, bar_x + filled - 1, bar_y + bar_h - 1],
                            radius=bar_h // 2, fill=fill_color)
    # seek knob
    kx = bar_x + filled - 8
    ky = bar_y + bar_h // 2 - 8
    d.ellipse([kx, ky, kx + 15, ky + 15], fill=THEME["text"])
    d.ellipse([kx + 2, ky + 2, kx + 13, ky + 13], fill=THEME["white"])

    # volume overlay
    if show_volume:
        _draw_volume_overlay(d, 0.65)

    return img


def _draw_icon(d, kind, x, y, color=None):
    color = color or THEME["primary"]
    if kind == "play":
        d.polygon([(x + 4, y + 2), (x + 4, y + 14), (x + 13, y + 8)], fill=color)
    else:
        d.rounded_rectangle([x + 4, y + 2, x + 7, y + 14], radius=2, fill=color)
        d.rounded_rectangle([x + 10, y + 2, x + 13, y + 14], radius=2, fill=color)


def _draw_volume_overlay(d, frac):
    px, py, pw, ph = 60, 36, 200, 16
    d.rounded_rectangle([px, py, px + pw - 1, py + ph - 1], radius=ph // 2,
                        fill=THEME["overlay_bg"], outline=hex_rgb("#3B4453"))
    # speaker
    white = THEME["white"]
    d.rectangle([px + 10, py + 5, px + 13, py + 9], fill=white)
    d.polygon([(px + 10, py + 7), (px + 5, py + 4), (px + 5, py + 10)], fill=white)
    # bar
    tx, ty, tw, th = px + 30, py + 5, 164, 6
    d.rounded_rectangle([tx, ty, tx + tw - 1, ty + th - 1], radius=th // 2,
                        fill=THEME["overlay_track"])
    filled = int(tw * frac)
    if filled > 0:
        d.rounded_rectangle([tx, ty, tx + filled - 1, ty + th - 1],
                            radius=th // 2, fill=THEME["accent"])


def main():
    os.makedirs(SPRITE_PREVIEW_DIR, exist_ok=True)
    os.makedirs(DOCS_DIR, exist_ok=True)

    sprites = build_sprites()
    write_header(sprites)
    write_previews(sprites)

    mock = draw_mockup(seek=False, show_volume=True)
    mock.save(os.path.join(DOCS_DIR, "nowplaying-mockup.png"))
    print("wrote docs/nowplaying-mockup.png")

    mock_seek = draw_mockup(seek=True, show_volume=False)
    mock_seek.save(os.path.join(DOCS_DIR, "nowplaying-mockup-seek.png"))
    print("wrote docs/nowplaying-mockup-seek.png")


if __name__ == "__main__":
    main()
