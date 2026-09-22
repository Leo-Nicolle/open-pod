// Canvas renderers for the 320x240 OpenPod screens previewed by the builder.
//
// Each draw* function paints a whole screen at 1x in screen pixels, using the
// same geometry as the firmware (see GEOMETRY in spec.ts). Glyphs and bars go
// through the sprite definitions so the preview matches the exported arrays.

import type { Palette } from './palette';
import { drawGlyph, drawSpriteAt, findSprite, rr, type GlyphDef } from './sprites';

export const W = 320;
export const H = 240;
export const ROW_H = 30;
const MONO = '"IBM Plex Mono", "Courier New", ui-monospace, monospace';
const SANS = '"IBM Plex Sans", system-ui, -apple-system, sans-serif';

type Ctx = CanvasRenderingContext2D;

// --- primitives -----------------------------------------------------------------

function setSpacing(ctx: Ctx, px: number) {
  const c = ctx as Ctx & { letterSpacing?: string };
  if ('letterSpacing' in c) c.letterSpacing = `${px}px`;
}

/** Draw single-line text. `y` is the top of a CSS line box of height `lh`. */
function text(
  ctx: Ctx,
  s: string,
  x: number,
  y: number,
  font: string,
  size: number,
  fill: string,
  opts: { align?: CanvasTextAlign; lh?: number; maxW?: number; spacing?: number } = {},
) {
  const lh = opts.lh ?? size;
  ctx.font = `${font.replace('{s}', `${size}px`)}`;
  setSpacing(ctx, opts.spacing ?? 0);
  ctx.fillStyle = fill;
  ctx.textAlign = opts.align ?? 'left';
  ctx.textBaseline = 'middle';
  const out = opts.maxW ? ellipsize(ctx, s, opts.maxW) : s;
  ctx.fillText(out, x, y + lh / 2);
  setSpacing(ctx, 0);
  return ctx.measureText(out).width;
}

function ellipsize(ctx: Ctx, s: string, maxW: number): string {
  if (ctx.measureText(s).width <= maxW) return s;
  let lo = 0;
  let hi = s.length;
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1;
    if (ctx.measureText(s.slice(0, mid).trimEnd() + '…').width <= maxW) lo = mid;
    else hi = mid - 1;
  }
  return s.slice(0, lo).trimEnd() + '…';
}

/** Greedy word wrap into at most `lines` lines, ellipsizing the last one. */
function wrap(ctx: Ctx, s: string, maxW: number, lines: number): string[] {
  const words = s.split(' ');
  const out: string[] = [];
  let cur = '';
  while (words.length) {
    const next = cur ? cur + ' ' + words[0] : words[0];
    if (ctx.measureText(next).width <= maxW || !cur) {
      cur = next;
      words.shift();
    } else {
      out.push(cur);
      cur = '';
      if (out.length === lines - 1) break;
    }
  }
  const rest = [cur, ...words].filter(Boolean).join(' ');
  if (rest) out.push(ellipsize(ctx, rest, maxW));
  return out;
}

function strokeBox(ctx: Ctx, x: number, y: number, w: number, h: number, r: number, color: string) {
  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  ctx.beginPath();
  const c = ctx as Ctx & { roundRect?: (...a: unknown[]) => void };
  if (typeof c.roundRect === 'function') c.roundRect(x + 0.5, y + 0.5, w - 1, h - 1, r);
  else ctx.rect(x + 0.5, y + 0.5, w - 1, h - 1);
  ctx.stroke();
}

/** Small caps chip (SHUF / RPT / SEEK). Returns its width. */
function chip(
  ctx: Ctx,
  label: string,
  x: number,
  y: number,
  fg: string,
  border: string,
  fill?: string,
): number {
  ctx.font = `500 9px ${SANS}`;
  setSpacing(ctx, 0.72);
  const w = Math.ceil(ctx.measureText(label).width) + 8;
  setSpacing(ctx, 0);
  if (fill) {
    ctx.fillStyle = fill;
    rr(ctx, x, y, w, 14, 2);
  } else {
    strokeBox(ctx, x, y, w, 14, 2, border);
  }
  text(ctx, label, x + 4, y + 1, `500 {s} ${SANS}`, 9, fg, { lh: 12, spacing: 0.72 });
  return w;
}

function battery(ctx: Ctx, p: Palette, x: number, y: number, level = 2) {
  const spr = findSprite('battery');
  if (spr) drawSpriteAt(ctx, spr, p, x, y);
  // fill cells are rects so the level is free
  for (let i = 0; i < 3; i++) {
    ctx.fillStyle = i < level ? p.text : p.line;
    ctx.fillRect(x + 3 + i * 5, y + 3, 4, 5);
  }
}

const CHEVRON: GlyphDef = {
  kind: 'glyph',
  id: 'back',
  w: 7,
  h: 11,
  tint: 'accent',
  rows: [
    '......#',
    '.....##',
    '....###',
    '...####',
    '..#####',
    '.######',
    '..#####',
    '...####',
    '....###',
    '.....##',
    '......#',
  ],
  note: '',
};

function header(ctx: Ctx, p: Palette) {
  ctx.fillStyle = p.surface;
  ctx.fillRect(0, 0, W, 30);
  ctx.fillStyle = p.line;
  ctx.fillRect(0, 29, W, 1);
}

function magnifier(ctx: Ctx, p: Palette, x: number, y: number) {
  ctx.strokeStyle = p.dim;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(x + 5.5, y + 5.5, 4.5, 0, Math.PI * 2);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(x + 8.5, y + 8.5);
  ctx.lineTo(x + 12, y + 12);
  ctx.stroke();
}

function listRow(
  ctx: Ctx,
  p: Palette,
  top: number,
  selected: boolean,
  paint: (fg: string, sub: string) => void,
) {
  if (selected) {
    ctx.fillStyle = p.accentDark;
    ctx.fillRect(0, top, W, ROW_H);
    ctx.fillStyle = p.accent;
    ctx.fillRect(0, top, 3, ROW_H);
  }
  paint(selected ? p.textHi : p.text, selected ? p.text : p.muted);
  ctx.fillStyle = p.separator;
  ctx.fillRect(0, top + ROW_H - 1, W, 1);
}

/** Scrollbar at x=313. `top`/`h` is the track; thumb follows the design formula. */
function scrollbar(ctx: Ctx, p: Palette, top: number, h: number, visible: number, total: number, first: number) {
  if (total <= visible) return;
  ctx.fillStyle = p.scrollTrack;
  rr(ctx, 313, top, 5, h, 2);
  const th = Math.max(24, Math.round((h * visible) / total));
  const maxScroll = Math.max(1, total - visible);
  const ty = top + Math.round((h - th) * Math.min(1, first / maxScroll));
  ctx.fillStyle = p.accent;
  rr(ctx, 313, ty, 5, th, 2);
}

function chunkGrid(ctx: Ctx, p: Palette, band?: [number, number]) {
  ctx.save();
  ctx.globalAlpha = 0.34;
  ctx.fillStyle = p.accent;
  for (let y = 0; y < H; y += ROW_H) ctx.fillRect(0, y, W, 1);
  if (band) {
    ctx.globalAlpha = 0.08;
    ctx.fillRect(0, band[0], W, band[1]);
  }
  ctx.restore();
}

// --- now playing ------------------------------------------------------------------

export type NowPlayingMode = 'play' | 'volume' | 'seek';

export interface NowPlayingOpts {
  mode: NowPlayingMode;
  playing: boolean;
  grid: boolean;
}

function coverPlaceholder(ctx: Ctx, p: Palette, x: number, y: number, size: number) {
  // 135deg stripes, 4px each, measured across the stripe
  for (let py = 0; py < size; py++) {
    for (let px = 0; px < size; px++) {
      const t = ((px + py) / Math.SQRT2) % 8;
      ctx.fillStyle = t < 4 ? p.surface : p.ribbon;
      ctx.fillRect(x + px, y + py, 1, 1);
    }
  }
  strokeBox(ctx, x, y, size, size, 0, p.line);
  text(ctx, 'cover', x + size / 2, y + size / 2 - 14, `{s} ${MONO}`, 10, p.muted, { align: 'center', lh: 14 });
  text(ctx, '160×160', x + size / 2, y + size / 2, `{s} ${MONO}`, 10, p.muted, { align: 'center', lh: 14 });
}

export function drawNowPlaying(ctx: Ctx, p: Palette, o: NowPlayingOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);

  // ---- header (chunk 0) ----
  header(ctx, p);
  const glyph = findSprite(o.playing ? 'pause' : 'play');
  if (glyph && glyph.kind === 'glyph') drawGlyph(ctx, glyph, p, 8, 10);
  text(ctx, '3/12', 24, 8, `{s} ${MONO}`, 11, p.dim, { lh: 14, spacing: 0.22 });
  const sw = chip(ctx, 'SHUF', 62, 8, p.accent, p.accentDark);
  chip(ctx, 'RPT', 62 + sw + 7, 8, p.muted, p.line);
  text(ctx, '14:32', 283, 7, `{s} ${MONO}`, 12, p.text, { align: 'right', lh: 16 });
  battery(ctx, p, 290, 9);

  // ---- album art (8,40 160x160) ----
  coverPlaceholder(ctx, p, 8, 40, 160);

  // ---- metadata column (180,40 132 wide) ----
  ctx.font = `600 14px ${SANS}`;
  wrap(ctx, 'Everything In Its Right Place', 132, 2).forEach((line, i) =>
    text(ctx, line, 180, 40 + i * 16, `600 {s} ${SANS}`, 14, p.text, { lh: 16, spacing: -0.14 }),
  );
  text(ctx, 'Radiohead', 180, 80, `{s} ${SANS}`, 12, p.dim, { lh: 16, maxW: 132 });
  text(ctx, 'Kid A', 180, 96, `{s} ${SANS}`, 11, p.muted, { lh: 14, maxW: 132 });

  // ---- bottom band (0,210 320x30): everything that moves ----
  if (o.mode === 'volume') {
    ctx.fillStyle = p.surface;
    ctx.fillRect(0, 210, W, 30);
    ctx.fillStyle = p.line;
    ctx.fillRect(0, 210, W, 1);
    const spk = findSprite('speaker');
    if (spk && spk.kind === 'glyph') drawGlyph(ctx, spk, p, 8, 219);
    text(ctx, 'VOL', 24, 218, `{s} ${MONO}`, 11, p.muted, { lh: 12 });
    ctx.fillStyle = p.line;
    rr(ctx, 56, 221, 220, 8, 2);
    ctx.fillStyle = p.accent;
    rr(ctx, 56, 221, Math.round(220 * 0.72), 8, 2);
    text(ctx, '72', 312, 217, `{s} ${MONO}`, 12, p.text, { align: 'right', lh: 16 });
  } else if (o.mode === 'seek') {
    let x = 8;
    x += text(ctx, '2:07', x, 212, `{s} ${MONO}`, 12, p.accent, { lh: 16 }) + 6;
    x += chip(ctx, 'SEEK', x, 213, p.bg, p.accentAlt, p.accentAlt) + 6;
    text(ctx, '+44s', x, 212, `{s} ${MONO}`, 11, p.muted, { lh: 16 });
    text(ctx, '4:05', 312, 212, `{s} ${MONO}`, 12, p.muted, { align: 'right', lh: 16 });
    const frac = 127 / 245;
    ctx.fillStyle = p.line;
    rr(ctx, 8, 228, 304, 10, 2);
    ctx.fillStyle = p.accentDark;
    rr(ctx, 8, 228, Math.round(304 * frac), 10, 2);
    const handle = findSprite('handle');
    if (handle) drawSpriteAt(ctx, handle, p, Math.round(8 + frac * 304 - 3), 226);
  } else {
    text(ctx, '1:23', 8, 212, `{s} ${MONO}`, 12, p.text, { lh: 16 });
    text(ctx, '4:05', 312, 212, `{s} ${MONO}`, 12, p.muted, { align: 'right', lh: 16 });
    ctx.fillStyle = p.line;
    rr(ctx, 8, 232, 304, 6, 3);
    ctx.fillStyle = p.accent;
    rr(ctx, 8, 232, Math.round(304 * (83 / 245)), 6, 3);
  }

  if (o.grid) chunkGrid(ctx, p, [210, 30]);
}

// --- track list -------------------------------------------------------------------

export interface Track {
  title: string;
  dur: string;
}

export interface TrackListOpts {
  heading: string;
  tracks: Track[];
  /** Index of the first visible track */
  first: number;
  /** Selected row, relative to the visible window (0…6) */
  selected: number;
  grid: boolean;
}

export const VISIBLE_ROWS = 7;

export function drawTrackList(ctx: Ctx, p: Palette, o: TrackListOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);

  header(ctx, p);
  drawGlyph(ctx, CHEVRON, p, 8, 10);
  text(ctx, o.heading, 23, 0, `600 {s} ${SANS}`, 14, p.text, { lh: 30, maxW: 282 - 23, spacing: -0.14 });
  battery(ctx, p, 290, 9);

  const first = Math.max(0, Math.min(o.first, o.tracks.length - VISIBLE_ROWS));
  o.tracks.slice(first, first + VISIBLE_ROWS).forEach((t, i) => {
    const top = 30 + i * ROW_H;
    listRow(ctx, p, top, i === o.selected, (fg, sub) => {
      const n = String(first + i + 1).padStart(2, '0');
      text(ctx, n, 28, top + 8, `{s} ${MONO}`, 11, sub, { align: 'right', lh: 14 });
      text(ctx, t.title, 36, top + 6, `{s} ${SANS}`, 14, fg, { lh: 18, maxW: 224 });
      text(ctx, t.dur, 304, top + 8, `{s} ${MONO}`, 11, sub, { align: 'right', lh: 14 });
    });
  });

  scrollbar(ctx, p, 32, 204, VISIBLE_ROWS, o.tracks.length, first);
  if (o.grid) chunkGrid(ctx, p);
}

// --- search -------------------------------------------------------------------------

export type ResultKind = 'TRK' | 'ALB' | 'ART';

export interface SearchResult {
  kind: ResultKind;
  title: string;
  sub: string;
}

export interface SearchOpts {
  mode: 'typing' | 'browsing';
  query: string;
  /** Letter under the ribbon cursor */
  letter: string;
  results: SearchResult[];
  /** Total match count (drives the scrollbar) */
  total: number;
  selected: number;
  first: number;
  grid: boolean;
}

const ALPHA = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('');

export function drawSearch(ctx: Ctx, p: Palette, o: SearchOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  const typing = o.mode === 'typing';

  // ---- header: chevron · magnifier · query + caret · battery ----
  header(ctx, p);
  drawGlyph(ctx, CHEVRON, p, 8, 10);
  magnifier(ctx, p, 23, 9);
  const qw = text(ctx, o.query, 42, 0, `{s} ${SANS}`, 14, p.text, { lh: 30, maxW: 240 });
  if (typing) {
    ctx.fillStyle = p.accent;
    ctx.fillRect(Math.round(42 + qw + 1), 8, 2, 15);
  }
  battery(ctx, p, 290, 9);

  // ---- letter ribbon (chunk 1, typing only) ----
  if (typing) {
    ctx.fillStyle = p.ribbon;
    ctx.fillRect(0, 30, W, 30);
    ctx.fillStyle = p.line;
    ctx.fillRect(0, 59, W, 1);
    const cursor = (o.letter || 'A').toUpperCase();
    const ci = Math.max(0, ALPHA.indexOf(cursor));
    const start = Math.max(0, Math.min(13, ci - 6));
    const x0 = Math.floor((W - (13 * 19 + 12 * 2)) / 2);
    ALPHA.slice(start, start + 13).forEach((c, i) => {
      const x = x0 + i * 21;
      const on = c === cursor;
      if (on) {
        ctx.fillStyle = p.accent;
        rr(ctx, x, 35, 19, 20, 2);
      }
      text(ctx, c, x + 9.5, 35, `{s} ${MONO}`, 13, on ? p.bg : p.muted, { align: 'center', lh: 20 });
    });
  }

  // ---- results ----
  const listTop = typing ? 60 : 30;
  const listH = H - listTop;
  const capacity = typing ? 6 : 7;

  if (o.results.length === 0) {
    const cy = listTop + listH / 2;
    text(ctx, 'No matches', W / 2, cy - 19, `{s} ${SANS}`, 14, p.text, { align: 'center', lh: 18 });
    text(ctx, 'scroll to edit the query', W / 2, cy + 5, `{s} ${MONO}`, 11, p.dim, { align: 'center', lh: 14 });
  }

  o.results.slice(0, capacity).forEach((r, i) => {
    const top = listTop + i * ROW_H;
    const on = !typing && i === o.selected;
    listRow(ctx, p, top, on, (fg, sub) => {
      strokeBox(ctx, 10, top + 9, 22, 14, 2, on ? p.text : p.line);
      text(ctx, r.kind, 21, top + 10, `{s} ${MONO}`, 9, sub, { align: 'center', lh: 12, spacing: 0.54 });
      text(ctx, r.title, 40, top + 6, `{s} ${SANS}`, 14, fg, { lh: 18, maxW: 150 });
      text(ctx, r.sub, 304, top + 8, `{s} ${SANS}`, 11, sub, { align: 'right', lh: 14, maxW: 108 });
    });
  });

  scrollbar(ctx, p, listTop + 2, listH - 8, capacity, o.results.length ? o.total : 0, o.first);
  if (o.grid) chunkGrid(ctx, p);
}

// --- sample content -------------------------------------------------------------------

export const KID_A: Track[] = [
  { title: 'Everything In Its Right Place', dur: '4:11' },
  { title: 'Kid A', dur: '4:44' },
  { title: 'The National Anthem', dur: '5:51' },
  { title: 'How To Disappear Completely', dur: '5:56' },
  { title: 'Treefingers', dur: '3:42' },
  { title: 'Optimistic', dur: '5:15' },
  { title: 'In Limbo', dur: '3:31' },
  { title: 'Idioteque', dur: '5:09' },
  { title: 'Morning Bell', dur: '4:35' },
  { title: 'Motion Picture Soundtrack', dur: '7:01' },
];

export const RADIOHEAD: Track[] = [
  { title: 'Airbag', dur: '4:44' },
  { title: 'Paranoid Android', dur: '6:23' },
  { title: 'Subterranean Homesick Alien', dur: '4:27' },
  { title: 'Exit Music (For a Film)', dur: '4:24' },
  { title: 'Let Down', dur: '4:59' },
  { title: 'Karma Police', dur: '4:21' },
  { title: 'Fitter Happier', dur: '1:57' },
  { title: 'Electioneering', dur: '3:50' },
  { title: 'Climbing Up the Walls', dur: '4:45' },
  { title: 'No Surprises', dur: '3:48' },
  { title: 'Lucky', dur: '4:19' },
  { title: 'The Tourist', dur: '5:24' },
];

export const KID_RESULTS: SearchResult[] = [
  { kind: 'TRK', title: 'Kid A', sub: 'Radiohead' },
  { kind: 'ALB', title: 'Kid A', sub: '2000 · 10 tracks' },
  { kind: 'TRK', title: 'Kid Fears', sub: 'Indigo Girls' },
  { kind: 'TRK', title: 'Kids', sub: 'MGMT' },
  { kind: 'ART', title: 'Kidnap', sub: '4 albums' },
  { kind: 'TRK', title: 'Kid Charlemagne', sub: 'Steely Dan' },
  { kind: 'TRK', title: 'Kidsticks', sub: 'Beth Orton' },
];
