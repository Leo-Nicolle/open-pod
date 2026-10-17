// opentone startup logo: SVG -> indexed sprite -> themed palette -> logo.h.
//
// The logo is drawn as flat polygons in docs/public/opentone_logo.svg, one CSS
// class per colour. Each class becomes a palette index (0 = transparent), so the
// rasterized pixels never depend on the theme: only the 16-entry palette does
// (see .agents/LOGO_PIPELINE.md, "swap the palette, never the pixels").
//
// The SVG's own colours are only used to name the roles and to keep the
// lightness steps between facets: each colour is mapped to a theme key, and
// colours sharing a key form a ramp whose spacing is copied from the SVG (see
// LOGO_ROLES and logoPalette()).
//
// Everything here is DOM-free, so the same code can generate the header from
// Node.

import svgSource from '../../../../public/opentone_logo.svg?raw';
import type { Palette, PaletteKey } from './palette';
import { hexToRgb, rgbToHex } from './palette';

export interface LogoRole {
  /** Fill colour in the source SVG, "#rrggbb" lowercase */
  svg: string;
  name: string;
  /** Theme colour this role follows */
  key: PaletteKey;
  /** Roles with the same ramp name are shaded together (defaults to the role alone) */
  ramp?: string;
  /** The ramp member that takes the theme colour exactly */
  anchor?: boolean;
  role: string;
}

// Mapping from SVG colours to theme colours. A lone role takes its theme colour
// as is. In a ramp, the anchor does, and the others keep their SVG lightness
// offset from the anchor, so the low-poly facets stay readable.
export const LOGO_ROLES: LogoRole[] = [
  { svg: '#161228', name: 'ink', key: 'surface', role: 'Seams, shadows, eye socket' },
  { svg: '#47143a', name: 'body 1', key: 'accent', ramp: 'body', role: 'Darkest: tail, neck, belly' },
  { svg: '#6f1d55', name: 'body 2', key: 'accent', ramp: 'body', role: 'Body facets' },
  { svg: '#972d6f', name: 'body 3', key: 'accent', ramp: 'body', anchor: true, role: 'Body facets (anchor)' },
  { svg: '#cb5297', name: 'body 4', key: 'accent', ramp: 'body', role: 'Lightest: cap, shoulder, chest' },
  { svg: '#96dd42', name: 'wing 1', key: 'accentAlt', ramp: 'wing', anchor: true, role: 'Collar, wing edge (anchor)' },
  { svg: '#60a534', name: 'wing 2', key: 'accentAlt', ramp: 'wing', role: 'Wing facets' },
  { svg: '#2c7038', name: 'wing 3', key: 'accentAlt', ramp: 'wing', role: 'Darkest wing facets' },
  { svg: '#7a4e20', name: 'brown', key: 'accentDark', ramp: 'thigh', anchor: true, role: 'Thighs (anchor)' },
  { svg: '#c89224', name: 'ochre', key: 'accentDark', ramp: 'thigh', role: 'Cheek, thigh, LED centres' },
  { svg: '#e2282c', name: 'signal', key: 'accentDark', role: 'Eye, LEDs, crest, tail feather' },
  { svg: '#e8e0cc', name: 'bone', key: 'text', role: 'Eye ring, beak, claws' },
  { svg: '#6e7274', name: 'grey', key: 'dim', role: 'Beak shade, legs' },
  { svg: '#303a47', name: 'slate', key: 'line', role: 'Lower beak' },
];

// A ramp shifted past these lightness limits is slid back as a whole, so its
// facets stay distinct even for a very light or very dark theme colour.
const L_MIN = 0.06;
const L_MAX = 0.94;

// --- SVG parsing ---------------------------------------------------------------

export interface LogoShape {
  /** Palette index, 1-based (0 is transparent) */
  index: number;
  /** Closed polygons in SVG units */
  polys: [number, number][][];
}

export interface LogoSource {
  w: number;
  h: number;
  shapes: LogoShape[];
}

function parsePath(d: string): [number, number][][] {
  const tokens = d.match(/[a-zA-Z]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?/g) ?? [];
  const polys: [number, number][][] = [];
  let cur: [number, number][] = [];
  let x = 0;
  let y = 0;
  let cmd = '';
  let i = 0;
  const num = () => parseFloat(tokens[i++]);
  const close = () => {
    if (cur.length > 2) polys.push(cur);
    cur = [];
  };
  while (i < tokens.length) {
    if (/[a-zA-Z]/.test(tokens[i])) cmd = tokens[i++];
    const rel = cmd === cmd.toLowerCase();
    switch (cmd.toLowerCase()) {
      case 'm': {
        close();
        const dx = num();
        const dy = num();
        x = rel ? x + dx : dx;
        y = rel ? y + dy : dy;
        cur.push([x, y]);
        cmd = rel ? 'l' : 'L'; // extra pairs after a moveto are linetos
        break;
      }
      case 'l': {
        const dx = num();
        const dy = num();
        x = rel ? x + dx : dx;
        y = rel ? y + dy : dy;
        cur.push([x, y]);
        break;
      }
      case 'h': {
        const v = num();
        x = rel ? x + v : v;
        cur.push([x, y]);
        break;
      }
      case 'v': {
        const v = num();
        y = rel ? y + v : v;
        cur.push([x, y]);
        break;
      }
      case 'z': {
        if (cur.length) [x, y] = cur[0];
        close();
        break;
      }
      default:
        throw new Error(`opentone_logo.svg: unsupported path command "${cmd}"`);
    }
  }
  close();
  return polys;
}

export function parseLogoSvg(svg: string): LogoSource {
  const vb = svg.match(/viewBox="([^"]+)"/);
  const [, , w, h] = (vb?.[1] ?? '0 0 0 0').split(/[\s,]+/).map(Number);

  const classColor = new Map<string, string>();
  for (const m of svg.matchAll(/\.([\w-]+)\s*\{\s*fill:\s*(#[0-9a-fA-F]{6})/g)) {
    classColor.set(m[1], m[2].toLowerCase());
  }

  const shapes: LogoShape[] = [];
  for (const m of svg.matchAll(/<path\b[^>]*>/g)) {
    const tag = m[0];
    const cls = tag.match(/class="([^"]+)"/)?.[1];
    const fill = tag.match(/fill="(#[0-9a-fA-F]{6})"/)?.[1]?.toLowerCase();
    const d = tag.match(/\sd="([^"]+)"/)?.[1];
    const color = fill ?? (cls ? classColor.get(cls) : undefined);
    if (!d || !color) continue;
    const role = LOGO_ROLES.findIndex((r) => r.svg === color);
    if (role < 0) throw new Error(`opentone_logo.svg: colour ${color} has no entry in LOGO_ROLES`);
    shapes.push({ index: role + 1, polys: parsePath(d) });
  }
  return { w, h, shapes };
}

export const LOGO_SOURCE: LogoSource = parseLogoSvg(svgSource);

// --- rasterization -------------------------------------------------------------

export interface LogoRaster {
  w: number;
  h: number;
  /** Palette index per pixel (0 = transparent), row-major */
  idx: Uint8Array;
}

const SS = 8;

/**
 * Rasterize to `height` px. Each output pixel is an 8x8 block of point samples
 * filled with palette indices in painter's order (no anti-aliasing); it is
 * transparent if under half the samples are covered, else the most frequent
 * index. Edges stay crisp and no colour outside the palette can appear.
 */
export function rasterizeLogo(
  height: number,
  src: LogoSource = LOGO_SOURCE,
  scale: { sx: number; sy: number } = { sx: 1, sy: 1 },
): LogoRaster {
  const s = height / src.h;
  const w = Math.ceil(src.w * s * scale.sx - 1e-9);
  const h = Math.round(height * scale.sy);
  const BW = w * SS;
  const BH = h * SS;
  // SVG units -> sample units
  const kx = s * scale.sx * SS;
  const ky = s * scale.sy * SS;
  const buf = new Uint8Array(BW * BH);
  const xs: number[] = [];

  for (const shape of src.shapes) {
    for (const poly of shape.polys) {
      let minY = Infinity;
      let maxY = -Infinity;
      for (const [, py] of poly) {
        minY = Math.min(minY, py * ky);
        maxY = Math.max(maxY, py * ky);
      }
      const y0 = Math.max(0, Math.ceil(minY - 0.5));
      const y1 = Math.min(BH - 1, Math.floor(maxY - 0.5));
      for (let sy = y0; sy <= y1; sy++) {
        const cy = sy + 0.5;
        xs.length = 0;
        for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
          const ay = poly[j][1] * ky;
          const by = poly[i][1] * ky;
          if (ay <= cy === by <= cy) continue;
          const ax = poly[j][0] * kx;
          const bx = poly[i][0] * kx;
          xs.push(ax + ((cy - ay) / (by - ay)) * (bx - ax));
        }
        xs.sort((a, b) => a - b);
        // even-odd: fill between pairs of crossings, sampling at x + 0.5
        for (let p = 0; p + 1 < xs.length; p += 2) {
          const xa = Math.max(0, Math.ceil(xs[p] - 0.5));
          const xb = Math.min(BW - 1, Math.floor(xs[p + 1] - 0.5));
          if (xb >= xa) buf.fill(shape.index, sy * BW + xa, sy * BW + xb + 1);
        }
      }
    }
  }

  const idx = new Uint8Array(w * h);
  const counts = new Uint16Array(16);
  const half = (SS * SS) / 2;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      counts.fill(0);
      let covered = 0;
      for (let sy = 0; sy < SS; sy++) {
        const row = (y * SS + sy) * BW + x * SS;
        for (let sx = 0; sx < SS; sx++) {
          const v = buf[row + sx];
          if (v) {
            counts[v]++;
            covered++;
          }
        }
      }
      if (covered < half) continue;
      let best = 1;
      for (let v = 2; v < 16; v++) if (counts[v] > counts[best]) best = v;
      idx[y * w + x] = best;
    }
  }
  return { w, h, idx };
}

// --- theming -------------------------------------------------------------------

function rgbToHsl([r, g, b]: [number, number, number]): [number, number, number] {
  r /= 255;
  g /= 255;
  b /= 255;
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const l = (max + min) / 2;
  if (max === min) return [0, 0, l];
  const d = max - min;
  const s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
  let hue: number;
  if (max === r) hue = (g - b) / d + (g < b ? 6 : 0);
  else if (max === g) hue = (b - r) / d + 2;
  else hue = (r - g) / d + 4;
  return [hue / 6, s, l];
}

function hslToRgb([h, s, l]: [number, number, number]): [number, number, number] {
  if (s === 0) return [l * 255, l * 255, l * 255];
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  const ch = (t: number) => {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6) return p + (q - p) * 6 * t;
    if (t < 1 / 2) return q;
    if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
    return p;
  };
  return [ch(h + 1 / 3) * 255, ch(h) * 255, ch(h - 1 / 3) * 255];
}

/**
 * The logo's 16 colours for a theme, as "#RRGGBB" (entry 0, transparent, is the
 * theme background). Each role takes its theme colour's hue and saturation;
 * lightness is the theme colour's, offset by the role's SVG lightness distance
 * to its ramp anchor.
 */
export function logoPalette(p: Palette): string[] {
  const out: string[] = new Array(16).fill(p.bg);
  const ramps = new Map<string, number[]>();
  LOGO_ROLES.forEach((r, i) => {
    const id = r.ramp ?? r.svg;
    ramps.set(id, [...(ramps.get(id) ?? []), i]);
  });

  for (const roles of ramps.values()) {
    const key = LOGO_ROLES[roles[0]].key;
    const [th, ts, tl] = rgbToHsl(hexToRgb(p[key]));
    const anchor = LOGO_ROLES[roles.find((i) => LOGO_ROLES[i].anchor) ?? roles[0]].svg;
    const al = rgbToHsl(hexToRgb(anchor))[2];
    const ls = roles.map((i) => tl + rgbToHsl(hexToRgb(LOGO_ROLES[i].svg))[2] - al);
    const lo = Math.min(...ls);
    const hi = Math.max(...ls);
    const slide = hi > L_MAX ? L_MAX - hi : lo < L_MIN ? L_MIN - lo : 0;
    roles.forEach((i, n) => {
      // a lone role keeps its theme colour exactly
      const rgb = roles.length === 1 ? hexToRgb(p[key]) : hslToRgb([th, ts, Math.min(1, Math.max(0, ls[n] + slide))]);
      out[i + 1] = rgbToHex(...rgb);
    });
  }
  return out;
}

// --- previews ------------------------------------------------------------------

/** RGBA pixels of a raster under a palette; index 0 is transparent. */
export function logoRgba(r: LogoRaster, pal: string[]): Uint8ClampedArray {
  const rgb = pal.map(hexToRgb);
  const out = new Uint8ClampedArray(r.w * r.h * 4);
  for (let i = 0; i < r.idx.length; i++) {
    const v = r.idx[i];
    if (!v) continue;
    out[i * 4] = rgb[v][0];
    out[i * 4 + 1] = rgb[v][1];
    out[i * 4 + 2] = rgb[v][2];
    out[i * 4 + 3] = 255;
  }
  return out;
}

// --- title screen ------------------------------------------------------------------

// "Drop & hop" intro from the title-screen handoff, in ms from the start of the
// boot screen. The firmware gets these as BOOT_* defines and ports bootFrame().
export const BOOT = {
  LOGO_X: 110,
  LOGO_Y: 44,
  LOGO_H: 122, // native size, no scaling
  SQUASH_SX: 1.07,
  SQUASH_SY: 0.91,
  DROP_START: 300,
  DROP_END: 650,
  DROP_PX: 48,
  SQUASH_END: 750,
  LETTER_START: 900,
  LETTER_STEP: 60,
  HOP_START: 1500,
  HOP_END: 1800,
  HOP_PX: 8,
  WIPE_AT: 2100,
  CHUNK_MS: 45,
  CHUNKS: 8,
  /** Loop period of the builder preview (as in the mock) */
  PERIOD: 3800,
};

export const WORDMARK = 'opentone';
/** Glyphs from this index on ("pod") use the accent colour */
export const WORDMARK_ACCENT_FROM = 4;

export interface Wordmark {
  /** Screen position of the strip's top-left corner */
  x: number;
  y: number;
  w: number;
  h: number;
  /** Coverage 0..15 per pixel, row-major */
  cov: Uint8Array;
  /** Glyph i owns strip columns [split[i], split[i + 1]) */
  split: number[];
}

export interface BootFrame {
  logo: 'none' | 'normal' | 'squash';
  /** Vertical offset of the normal logo from LOGO_Y (drop and hop) */
  dy: number;
  /** Number of wordmark glyphs shown */
  letters: number;
  /** Number of menu chunks drawn by the exit wipe */
  chunks: number;
}

const roundHalfUp = (v: number) => Math.floor(v + 0.5);
const seg = (t: number, a: number, b: number) => Math.max(0, Math.min(1, (t - a) / (b - a)));

export function bootFrame(t: number): BootFrame {
  const B = BOOT;
  let logo: BootFrame['logo'] = t < B.DROP_START ? 'none' : 'normal';
  let dy = 0;
  if (t >= B.DROP_START && t < B.DROP_END) {
    const q = 1 - seg(t, B.DROP_START, B.DROP_END);
    dy = roundHalfUp(-B.DROP_PX * q * q); // -48 * (1 - easeOut(p))
  } else if (t >= B.DROP_END && t < B.SQUASH_END) {
    logo = 'squash';
  } else if (t >= B.HOP_START && t < B.HOP_END) {
    dy = roundHalfUp(-B.HOP_PX * Math.sin(Math.PI * seg(t, B.HOP_START, B.HOP_END)));
  }
  const letters = t < B.LETTER_START ? 0 : Math.min(WORDMARK.length, Math.floor((t - B.LETTER_START) / B.LETTER_STEP) + 1);
  const chunks = t < B.WIPE_AT ? 0 : Math.min(B.CHUNKS, Math.floor((t - B.WIPE_AT) / B.CHUNK_MS) + 1);
  return { logo, dy, letters, chunks };
}

/** Squash frame position: same bottom edge as the logo, centred on it. */
export function squashPos(logo: LogoRaster, squash: LogoRaster): { x: number; y: number } {
  return {
    x: BOOT.LOGO_X + Math.floor((logo.w - squash.w) / 2),
    y: BOOT.LOGO_Y + logo.h - squash.h,
  };
}

export const rasterizeBootLogo = () => rasterizeLogo(BOOT.LOGO_H);
export const rasterizeBootSquash = () =>
  rasterizeLogo(BOOT.LOGO_H, LOGO_SOURCE, { sx: BOOT.SQUASH_SX, sy: BOOT.SQUASH_SY });

/** 16-step ramp from the background to a theme colour, indexed by coverage. */
export function wordRamp(p: Palette, key: PaletteKey): string[] {
  const [br, bg, bb] = hexToRgb(p.bg);
  const [fr, fg, fb] = hexToRgb(p[key]);
  return Array.from({ length: 16 }, (_, k) => {
    const a = k / 15;
    return rgbToHex(br + (fr - br) * a, bg + (fg - bg) * a, bb + (fb - bb) * a);
  });
}

// --- C header ------------------------------------------------------------------

function pack565(hex: string): number {
  const [r, g, b] = hexToRgb(hex);
  return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

const hex4 = (v: number) => '0x' + v.toString(16).toUpperCase().padStart(4, '0');
const hex2 = (v: number) => '0x' + v.toString(16).toUpperCase().padStart(2, '0');

function pack4bpp(w: number, h: number, px: Uint8Array): { stride: number; bytes: number[] } {
  const stride = Math.ceil(w / 2);
  const bytes: number[] = [];
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < stride * 2; x += 2) {
      const a = px[y * w + x];
      const b = x + 1 < w ? px[y * w + x + 1] : 0;
      bytes.push((a << 4) | b);
    }
  }
  return { stride, bytes };
}

function emitBytes(L: string[], name: string, bytes: number[], cols: number): void {
  L.push(`static const uint8_t ${name}[${bytes.length}] = {`);
  for (let i = 0; i < bytes.length; i += cols) L.push('  ' + bytes.slice(i, i + cols).map(hex2).join(',') + ',');
  L.push('};');
  L.push('');
}

function emitImage(L: string[], name: string, r: LogoRaster): void {
  const { stride, bytes } = pack4bpp(r.w, r.h, r.idx);
  emitBytes(L, `${name}_DATA`, bytes, stride);
  L.push(`static const LogoImage ${name} = {${r.w}, ${r.h}, ${stride}, ${name}_DATA};`);
  L.push('');
}

/** Generate the firmware logo.h: logo frames, wordmark, palettes and intro timeline. */
export function buildLogoHeader(p: Palette, logo: LogoRaster, squash: LogoRaster, word: Wordmark): string {
  const pal = logoPalette(p);
  const sq = squashPos(logo, squash);
  const B = BOOT;
  const L: string[] = [];
  L.push('#pragma once');
  L.push('// Auto-generated opentone title screen (see docs/ui-builder).');
  L.push('// Logo source: docs/public/opentone_logo.svg. Images are 4 bits per pixel,');
  L.push('// high nibble = left pixel, rows padded to a whole byte. Logo pixels are');
  L.push('// palette indices (0 = transparent); wordmark pixels are coverage 0..15,');
  L.push('// looked up in a BG->colour ramp. Colours come from the theme at export.');
  L.push('#include <stdint.h>');
  L.push('#include "rendering/ILI9341_GFX.h"');
  L.push('');
  L.push('struct LogoImage {');
  L.push('  uint16_t w;');
  L.push('  uint16_t h;');
  L.push('  uint16_t stride;');
  L.push('  const uint8_t* data;');
  L.push('};');
  L.push('');
  L.push('// --- intro timeline ("Drop & hop"), ms from the start of the boot screen');
  const defs: [string, number][] = [
    ['OPENPOD_BOOT_BG', pack565(p.bg)],
    ['OPENPOD_LOGO_X', B.LOGO_X],
    ['OPENPOD_LOGO_Y', B.LOGO_Y],
    ['OPENPOD_SQUASH_X', sq.x],
    ['OPENPOD_SQUASH_Y', sq.y],
    ['BOOT_DROP_START', B.DROP_START],
    ['BOOT_DROP_END', B.DROP_END],
    ['BOOT_DROP_PX', B.DROP_PX],
    ['BOOT_SQUASH_END', B.SQUASH_END],
    ['BOOT_LETTER_START', B.LETTER_START],
    ['BOOT_LETTER_STEP', B.LETTER_STEP],
    ['BOOT_HOP_START', B.HOP_START],
    ['BOOT_HOP_END', B.HOP_END],
    ['BOOT_HOP_PX', B.HOP_PX],
    ['BOOT_WIPE_AT', B.WIPE_AT],
    ['BOOT_CHUNK_MS', B.CHUNK_MS],
  ];
  for (const [n, v] of defs) L.push(`#define ${n.padEnd(18)} ${n === 'OPENPOD_BOOT_BG' ? hex4(v) : v}`);
  L.push('');
  L.push('// RGB565. [0] is unused (transparent); the blit fills it with the background.');
  L.push('static const uint16_t OPENPOD_LOGO_PALETTE[16] = {');
  L.push(`  ${hex4(pack565(pal[0]))}, // transparent`);
  LOGO_ROLES.forEach((role, i) => {
    L.push(`  ${hex4(pack565(pal[i + 1]))}, // ${String(i + 1).padStart(2)} ${role.name.padEnd(7)} ${pal[i + 1]}  <- ${role.key}`);
  });
  for (let i = LOGO_ROLES.length + 1; i < 16; i++) L.push(`  0x0000, // ${String(i).padStart(2)} unused`);
  L.push('};');
  L.push('');
  L.push(`// Logo ${logo.w}x${logo.h}, and the landing squash frame ${squash.w}x${squash.h}.`);
  emitImage(L, 'OPENPOD_LOGO', logo);
  emitImage(L, 'OPENPOD_LOGO_SQUASH', squash);

  L.push(`// Wordmark "${WORDMARK}" ${word.w}x${word.h} at (${word.x}, ${word.y}). Glyph i covers`);
  L.push('// strip columns [SPLIT[i], SPLIT[i+1]); glyphs from ACCENT_FROM on use the accent ramp.');
  L.push(`#define OPENPOD_WORD_X ${word.x}`);
  L.push(`#define OPENPOD_WORD_Y ${word.y}`);
  L.push(`#define OPENPOD_WORD_W ${word.w}`);
  L.push(`#define OPENPOD_WORD_H ${word.h}`);
  L.push(`#define OPENPOD_WORD_STRIDE ${Math.ceil(word.w / 2)}`);
  L.push(`#define OPENPOD_WORD_GLYPHS ${WORDMARK.length}`);
  L.push(`#define OPENPOD_WORD_ACCENT_FROM ${WORDMARK_ACCENT_FROM}`);
  L.push(`static const uint8_t OPENPOD_WORD_SPLIT[${word.split.length}] = {${word.split.join(', ')}};`);
  for (const [name, key] of [['TEXT', 'text'], ['ACCENT', 'accent']] as const) {
    L.push(`static const uint16_t OPENPOD_WORD_RAMP_${name}[16] = {`);
    L.push('  ' + wordRamp(p, key).map((c) => hex4(pack565(c))).join(', ') + ',');
    L.push('};');
  }
  L.push('');
  emitBytes(L, 'OPENPOD_WORD_DATA', pack4bpp(word.w, word.h, word.cov).bytes, Math.ceil(word.w / 2));

  L.push('// Push the rect (rx, ry, rw, rh) with img drawn at (ix, iy) and bg everywhere');
  L.push('// else, one row per window write. Covering the union of the previous and the');
  L.push('// new position clears and redraws a moving logo in a single pass.');
  L.push('static inline void blitLogoImage(ILI9341_GFX* d, const LogoImage& img, int ix, int iy,');
  L.push('                                 int rx, int ry, int rw, int rh, uint16_t bg) {');
  L.push('  uint16_t row[320];');
  L.push('  if (rx < 0) {');
  L.push('    rw += rx;');
  L.push('    rx = 0;');
  L.push('  }');
  L.push('  if (rx + rw > 320) rw = 320 - rx;');
  L.push('  if (rw <= 0) return;');
  L.push('  for (int y = ry; y < ry + rh; ++y) {');
  L.push('    if (y < 0 || y >= 240) continue;');
  L.push('    int ly = y - iy;');
  L.push('    bool inRow = ly >= 0 && ly < (int)img.h;');
  L.push('    const uint8_t* src = img.data + ly * img.stride;');
  L.push('    for (int x = 0; x < rw; ++x) {');
  L.push('      int lx = rx + x - ix;');
  L.push('      uint16_t c = bg;');
  L.push('      if (inRow && lx >= 0 && lx < (int)img.w) {');
  L.push('        uint8_t i = (lx & 1) ? (src[lx >> 1] & 0x0F) : (src[lx >> 1] >> 4);');
  L.push('        if (i) c = OPENPOD_LOGO_PALETTE[i];');
  L.push('      }');
  L.push('      row[x] = c;');
  L.push('    }');
  L.push('    d->pushWindow(rx, y, rw, 1, row);');
  L.push('  }');
  L.push('}');
  L.push('');
  L.push('// Draw wordmark glyph i (its whole column band, background included).');
  L.push('static inline void drawWordmarkGlyph(ILI9341_GFX* d, int i) {');
  L.push('  const uint16_t* ramp =');
  L.push('      i < OPENPOD_WORD_ACCENT_FROM ? OPENPOD_WORD_RAMP_TEXT : OPENPOD_WORD_RAMP_ACCENT;');
  L.push('  int x0 = OPENPOD_WORD_SPLIT[i], x1 = OPENPOD_WORD_SPLIT[i + 1];');
  L.push('  uint16_t row[OPENPOD_WORD_W];');
  L.push('  for (int y = 0; y < OPENPOD_WORD_H; ++y) {');
  L.push('    const uint8_t* src = OPENPOD_WORD_DATA + y * OPENPOD_WORD_STRIDE;');
  L.push('    for (int x = x0; x < x1; ++x) {');
  L.push('      uint8_t v = (x & 1) ? (src[x >> 1] & 0x0F) : (src[x >> 1] >> 4);');
  L.push('      row[x - x0] = ramp[v];');
  L.push('    }');
  L.push('    d->pushWindow(OPENPOD_WORD_X + x0, OPENPOD_WORD_Y + y, x1 - x0, 1, row);');
  L.push('  }');
  L.push('}');
  L.push('');
  return L.join('\n');
}
