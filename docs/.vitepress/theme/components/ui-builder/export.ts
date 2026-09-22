// Rasterization + RGB565 export for the UI builder.
//
// The firmware's ILI9341 panel is physically BGR-subpixel-ordered, but its
// MADCTL BGR bit (set in ILI9341_GFX::setRotation) already compensates for
// that in hardware, transparently - software sends plain RGB565, matching
// theme.h's HEX_TO_RGB565 and .ui-work/gen-sprites.py. See
// .agents/screen-red-problem.md for the full diagnosis (D14/D15 pin damage
// was the actual root cause of the original "everything is blue" symptom,
// not a BGR/RGB mismatch).

import type { Palette } from './palette';
import { hexToRgb, PALETTE_META } from './palette';
import { SPRITES, type SpriteDef, type GlyphDef, type OpaqueDef } from './sprites';

export const SUPERSAMPLE = 4;

/** Pack an 8-bit RGB triple into a 16-bit 565 word. */
export function pack565(r: number, g: number, b: number): number {
  return (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)) & 0xffff;
}

/** Pack a "#RRGGBB" color directly into a 16-bit 565 word. */
export function hexTo565(hex: string): number {
  const [r, g, b] = hexToRgb(hex);
  return pack565(r, g, b);
}

export interface RasterResult {
  w: number;
  h: number;
  /** 16-bit packed pixels, length w*h */
  pixels: number[];
  /** 1 bit/px, MSB-first; undefined = opaque */
  mask?: Uint8Array;
  /** RGBA data (length w*h*4), used for PNG previews */
  rgba: Uint8ClampedArray;
}

function packMask(bits: number[]): Uint8Array {
  const out = new Uint8Array(Math.ceil(bits.length / 8));
  for (let i = 0; i < bits.length; i++) {
    if (bits[i]) out[i >> 3] |= 0x80 >> (i & 7);
  }
  return out;
}

function rasterizeGlyph(def: GlyphDef, p: Palette): RasterResult {
  const { w, h, rows, tint } = def;
  const tint565 = hexTo565(p[tint]);
  const [tr, tg, tb] = hexToRgb(p[tint]);
  const pixels = new Array<number>(w * h).fill(0);
  const rgba = new Uint8ClampedArray(w * h * 4);
  const bits = new Array<number>(w * h).fill(0);

  for (let y = 0; y < h; y++) {
    const row = rows[y] ?? '';
    for (let x = 0; x < w; x++) {
      const on = (row[x] ?? '.') !== '.';
      const i = y * w + x;
      if (on) {
        bits[i] = 1;
        pixels[i] = tint565;
        rgba[i * 4] = tr;
        rgba[i * 4 + 1] = tg;
        rgba[i * 4 + 2] = tb;
        rgba[i * 4 + 3] = 255;
      }
      // off pixels stay transparent (alpha 0) and black in the pixel array
    }
  }
  return { w, h, pixels, mask: packMask(bits), rgba };
}

function rasterizeOpaque(def: OpaqueDef, p: Palette): RasterResult {
  const { w, h, draw } = def;
  const S = SUPERSAMPLE;
  const canvas = document.createElement('canvas');
  canvas.width = w * S;
  canvas.height = h * S;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  if (!ctx) throw new Error('2D canvas unavailable');
  ctx.scale(S, S);
  draw(ctx, p);
  const big = ctx.getImageData(0, 0, w * S, h * S).data;

  const rgba = new Uint8ClampedArray(w * h * 4);
  const pixels = new Array<number>(w * h);
  const bits = new Array<number>(w * h).fill(0);
  let hasTransparency = false;
  const n = S * S;

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      // Premultiplied-alpha box downsample so transparent pixels don't
      // darken the shape's anti-aliased edges (canvas returns straight alpha).
      let sr = 0;
      let sg = 0;
      let sb = 0;
      let sa = 0;
      for (let sy = 0; sy < S; sy++) {
        for (let sx = 0; sx < S; sx++) {
          const o = ((y * S + sy) * (w * S) + (x * S + sx)) * 4;
          const a = big[o + 3];
          sr += big[o] * a;
          sg += big[o + 1] * a;
          sb += big[o + 2] * a;
          sa += a;
        }
      }
      const i = y * w + x;
      if (sa === 0) {
        rgba[i * 4] = 0;
        rgba[i * 4 + 1] = 0;
        rgba[i * 4 + 2] = 0;
        rgba[i * 4 + 3] = 0;
        pixels[i] = 0;
        bits[i] = 0;
        hasTransparency = true;
      } else {
        const A = sa / n; // average alpha 0..255
        const R = Math.round(sr / sa);
        const G = Math.round(sg / sa);
        const B = Math.round(sb / sa);
        rgba[i * 4] = R;
        rgba[i * 4 + 1] = G;
        rgba[i * 4 + 2] = B;
        rgba[i * 4 + 3] = Math.round(A);
        pixels[i] = pack565(R, G, B);
        bits[i] = A >= 128 ? 1 : 0;
        if (A < 128) hasTransparency = true;
      }
    }
  }
  return { w, h, pixels, mask: hasTransparency ? packMask(bits) : undefined, rgba };
}

export function rasterize(def: SpriteDef, p: Palette): RasterResult {
  return def.kind === 'glyph' ? rasterizeGlyph(def, p) : rasterizeOpaque(def, p);
}

// --- C header generation ------------------------------------------------------

function fmt565(v: number): string {
  return '0x' + v.toString(16).toUpperCase().padStart(4, '0');
}

function fmtByte(b: number): string {
  return '0x' + b.toString(16).toUpperCase().padStart(2, '0');
}

function emitArray(name: string, values: number[], cols = 12): string {
  const lines: string[] = [];
  for (let i = 0; i < values.length; i += cols) {
    const chunk = values.slice(i, i + cols);
    lines.push('  ' + chunk.map(fmt565).join(', ') + ',');
  }
  return `const uint16_t ${name}[] = {\n${lines.join('\n')}\n};\n`;
}

function emitMask(name: string, mask: Uint8Array, cols = 16): string {
  const bytes = Array.from(mask);
  const lines: string[] = [];
  for (let i = 0; i < bytes.length; i += cols) {
    const chunk = bytes.slice(i, i + cols);
    lines.push('  ' + chunk.map(fmtByte).join(', ') + ',');
  }
  return `const uint8_t ${name}[] = {\n${lines.join('\n')}\n};\n`;
}

function macroName(key: string): string {
  const meta = PALETTE_META.find((m) => m.key === key);
  return meta?.name ?? 'COLOR_' + key.toUpperCase();
}

function hexLiteral(hex: string): string {
  return '0x' + hex.replace('#', '').toUpperCase();
}

/** Generate the firmware theme.h with the current palette. */
export function buildThemeHeader(p: Palette): string {
  const lines: string[] = [];
  lines.push('#pragma once');
  lines.push('// Auto-generated Now Playing theme (see docs/ui-builder).');
  lines.push('// Packing: RGB565 (red in high bits).');
  lines.push('');
  // Every parameter use is individually parenthesized - not just each term -
  // because HEX_TO_RGB565 below passes an unshifted `(hex) & 0xFF`
  // for the channel that lands in the `>> 3` slot here. Since `>>` binds
  // tighter than `&` in C, an unparenthesized `(x >> 3)` receiving that text
  // becomes `hex & (0xFF >> 3)` = `hex & 0x1F` - masking the whole 24-bit hex
  // value instead of shifting the already-extracted byte. See
  // .agents/screen-red-problem.md - a version of this file without the extra
  // parens shipped that exact bug.
  lines.push('#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))');
  lines.push('#define HEX_TO_RGB565(hex) RGB565(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)');
  lines.push('');
  for (const meta of PALETTE_META) {
    lines.push(`#define ${meta.name.padEnd(17)} HEX_TO_RGB565(${hexLiteral(p[meta.key])})`);
  }
  lines.push('');
  return lines.join('\n');
}

/** Generate the firmware sprites.h with the current palette. */
export function buildSpritesHeader(p: Palette): string {
  const results = SPRITES.map((def) => ({ def, data: rasterize(def, p) }));

  const lines: string[] = [];
  lines.push('#pragma once');
  lines.push('// Auto-generated Now Playing sprites (see docs/ui-builder).');
  lines.push('// Packing: RGB565 (red in high bits).');
  lines.push('// Const arrays live in flash (.rodata) and are read directly.');
  lines.push('#include <stdint.h>');
  lines.push('');
  lines.push('class ILI9341_GFX;');
  lines.push('');
  lines.push('struct Sprite {');
  lines.push('  uint16_t w;');
  lines.push('  uint16_t h;');
  lines.push('  const uint16_t* pixels;');
  lines.push('  const uint8_t* mask;  // 1 bit/px, MSB-first; nullptr = opaque');
  lines.push('};');
  lines.push('');

  const decls: string[] = [];
  for (const { def, data } of results) {
    lines.push(`// ${def.id}  ${def.w}x${def.h}${def.kind === 'glyph' ? '  (1-bit mask)' : ''}`);
    lines.push(emitArray('spr_' + def.id, data.pixels));
    if (data.mask) {
      lines.push(emitMask('spr_' + def.id, data.mask));
    }
    const maskName = data.mask ? 'spr_' + def.id + '_mask' : 'nullptr';
    decls.push(`static const Sprite SPR_${def.id.toUpperCase()} = {${def.w}, ${def.h}, spr_${def.id}, ${maskName}};`);
  }

  lines.push('// Named sprite instances');
  lines.push(...decls);
  lines.push('');

  // Blit helpers (same shape as scripts/gen-sprites.py).
  lines.push('// Blit into a caller-owned RGB565 buffer (chunk rendering), clipped.');
  lines.push('static inline void blitSprite(uint16_t* buf, int bufW, int bufH,');
  lines.push('                              const Sprite& s, int x, int y) {');
  lines.push('  for (int sy = 0; sy < (int)s.h; ++sy) {');
  lines.push('    int dy = y + sy;');
  lines.push('    if (dy < 0 || dy >= bufH) continue;');
  lines.push('    for (int sx = 0; sx < (int)s.w; ++sx) {');
  lines.push('      int dx = x + sx;');
  lines.push('      if (dx < 0 || dx >= bufW) continue;');
  lines.push('      if (s.mask) {');
  lines.push('        int bit = sy * s.w + sx;');
  lines.push('        if (!(s.mask[bit >> 3] & (0x80 >> (bit & 7)))) continue;');
  lines.push('      }');
  lines.push('      buf[dy * bufW + dx] = s.pixels[sy * s.w + sx];');
  lines.push('    }');
  lines.push('  }');
  lines.push('}');
  lines.push('');
  lines.push('// Blit directly to the display (for overlays outside the chunk buffer).');
  lines.push('static inline void blitSpriteToDisplay(ILI9341_GFX* d, const Sprite& s,');
  lines.push('                                        int x, int y) {');
  lines.push('  for (int sy = 0; sy < (int)s.h; ++sy) {');
  lines.push('    int runStart = -1, runLen = 0;');
  lines.push('    for (int sx = 0; sx <= (int)s.w; ++sx) {');
  lines.push('      bool opaque = sx < (int)s.w;');
  lines.push('      if (opaque && s.mask) {');
  lines.push('        int bit = sy * s.w + sx;');
  lines.push('        opaque = s.mask[bit >> 3] & (0x80 >> (bit & 7));');
  lines.push('      }');
  lines.push('      if (opaque) {');
  lines.push('        if (runStart < 0) runStart = sx;');
  lines.push('        runLen++;');
  lines.push('      } else {');
  lines.push('        if (runStart >= 0) {');
  lines.push('          d->setWindow(x + runStart, y + sy, x + runStart + runLen - 1, y + sy);');
  lines.push('          d->pushPixels((uint16_t*)&s.pixels[sy * s.w + runStart], runLen);');
  lines.push('          runStart = -1;');
  lines.push('          runLen = 0;');
  lines.push('        }');
  lines.push('      }');
  lines.push('    }');
  lines.push('  }');
  lines.push('}');
  lines.push('');
  return lines.join('\n');
}

// --- downloads -----------------------------------------------------------------

export function downloadText(filename: string, text: string): void {
  const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
  downloadBlob(filename, blob);
}

export function downloadBlob(filename: string, blob: Blob): void {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

function rasterToCanvas(data: RasterResult): HTMLCanvasElement {
  const canvas = document.createElement('canvas');
  canvas.width = data.w;
  canvas.height = data.h;
  const ctx = canvas.getContext('2d');
  if (!ctx) throw new Error('2D canvas unavailable');
  ctx.putImageData(new ImageData(data.rgba, data.w, data.h), 0, 0);
  return canvas;
}

function checker(w: number, h: number, cell = 8): HTMLCanvasElement {
  const c = document.createElement('canvas');
  c.width = w;
  c.height = h;
  const ctx = c.getContext('2d')!;
  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, w, h);
  ctx.fillStyle = '#dcdcdc';
  for (let y = 0; y < h; y += cell) {
    for (let x = 0; x < w; x += cell) {
      if (((x / cell) + (y / cell)) % 2 === 0) ctx.fillRect(x, y, cell, cell);
    }
  }
  return c;
}

/** Produce a zoomed PNG (nearest-neighbour) of a rasterized sprite. */
export function spriteToPngBlob(def: SpriteDef, data: RasterResult, zoom = 8): Promise<Blob> {
  const src = rasterToCanvas(data);
  const out = document.createElement('canvas');
  out.width = data.w * zoom;
  out.height = data.h * zoom;
  const ctx = out.getContext('2d')!;
  // checkerboard behind 1-bit masks so transparency is visible
  ctx.drawImage(checker(out.width, out.height), 0, 0);
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(src, 0, 0, out.width, out.height);
  return new Promise((resolve) => {
    out.toBlob((b) => resolve(b ?? new Blob()), 'image/png');
  });
}

/** Export every sprite as a PNG, triggering one download per file. */
export async function exportAllPngs(p: Palette): Promise<void> {
  for (const def of SPRITES) {
    const data = rasterize(def, p);
    const blob = await spriteToPngBlob(def, data, 8);
    downloadBlob(`sprite-${def.id}.png`, blob);
  }
}

export async function copyToClipboard(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    // Fallback for environments without the async clipboard API.
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      const ok = document.execCommand('copy');
      document.body.removeChild(ta);
      return ok;
    } catch {
      return false;
    }
  }
}
