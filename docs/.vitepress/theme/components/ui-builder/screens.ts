// Canvas renderers for the 320x240 opentone screens previewed by the builder.
//
// Each draw* function paints a whole screen at 1x in screen pixels, using the
// same geometry as the firmware (see GEOMETRY in spec.ts). Glyphs and bars go
// through the sprite definitions so the preview matches the exported arrays.

import { hexToRgb, type Palette } from './palette';
import { BT_ROWS, drawGlyph, drawSpriteAt, findSprite, rr, type GlyphDef } from './sprites';

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

/**
 * Scrollbar at x=313. `top`/`h` is the track; thumb follows the design formula.
 * `wide` (fast scroll) widens the thumb to 7px at x=312.
 */
function scrollbar(
  ctx: Ctx,
  p: Palette,
  top: number,
  h: number,
  visible: number,
  total: number,
  first: number,
  wide = false,
) {
  if (total <= visible) return;
  ctx.fillStyle = p.scrollTrack;
  rr(ctx, 313, top, 5, h, 2);
  const th = Math.max(24, Math.round((h * visible) / total));
  const maxScroll = Math.max(1, total - visible);
  const ty = top + Math.round((h - th) * Math.min(1, first / maxScroll));
  ctx.fillStyle = p.accent;
  rr(ctx, wide ? 312 : 313, ty, wide ? 7 : 5, th, 2);
}

// --- fast-scroll overlay --------------------------------------------------------------

/** Label font size (Sans 600) per letter count. */
export const FAST_SCROLL_SIZES: Record<number, number> = { 1: 60, 2: 46, 3: 36 };

/** 1 letter up to 150 items, 2 up to 1500, 3 beyond. */
export function fastScrollLetterCount(total: number): number {
  return total > 1500 ? 3 : total > 150 ? 2 : 1;
}

/** First letters of `title`: non-letters stripped, uppercased, `#` if nothing is left. */
export function fastScrollLabel(title: string, total: number): string {
  return title.replace(/[^A-Za-z]/g, '').slice(0, fastScrollLetterCount(total)).toUpperCase() || '#';
}

/** 120×90 box at 100,90 (exactly chunks 3–5): SURFACE, 2px ACCENT border inset, r=6. */
function fastScrollOverlay(ctx: Ctx, p: Palette, label: string) {
  const x = 100;
  const y = 90;
  const w = 120;
  const h = 90;
  ctx.fillStyle = p.accent;
  rr(ctx, x, y, w, h, 6);
  ctx.fillStyle = p.surface;
  rr(ctx, x + 2, y + 2, w - 4, h - 4, 4);
  const size = FAST_SCROLL_SIZES[label.length] ?? 60;
  ctx.font = `600 ${size}px ${SANS}`;
  setSpacing(ctx, size * 0.02);
  ctx.fillStyle = p.text;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'alphabetic';
  // Centre the cap height, not the line box: the label is A–Z only.
  const m = ctx.measureText(label);
  const capH = m.actualBoundingBoxAscent;
  ctx.fillText(label, x + w / 2, y + h / 2 + capH / 2);
  setSpacing(ctx, 0);
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
  /** Bluetooth icon state, and the codec tag (hidden on the built-in output) */
  bt?: BtIconState;
  codec?: string;
  toast?: ToastSpec;
  /** Podcast episode: time left on the right instead of the length, no SHUF/RPT */
  podcast?: PodcastPlaying;
}

export interface PodcastPlaying {
  /** Header index: the episode number ("2/10") or the position in the show list */
  index: string;
  title: string;
  /** Show (or series) folder name, DIM */
  show: string;
  /** Series: the show folder name; dated: the full date ("Mon, 12 Oct 2026", Mono) */
  third: string;
  thirdMono?: boolean;
  elapsed: string;
  /** "−11:03" */
  left: string;
  /** Progress 0–1 */
  frac: number;
}

function coverPlaceholder(ctx: Ctx, p: Palette, x: number, y: number, size: number, label = 'cover') {
  // 135deg stripes, 4px each, measured across the stripe. Written as one
  // ImageData: per-pixel fillRect is far too slow for live color edits.
  const img = ctx.createImageData(size, size);
  const a = hexToRgb(p.surface);
  const b = hexToRgb(p.ribbon);
  for (let py = 0; py < size; py++) {
    for (let px = 0; px < size; px++) {
      const c = ((px + py) / Math.SQRT2) % 8 < 4 ? a : b;
      const o = (py * size + px) * 4;
      img.data[o] = c[0];
      img.data[o + 1] = c[1];
      img.data[o + 2] = c[2];
      img.data[o + 3] = 255;
    }
  }
  ctx.putImageData(img, x, y);
  strokeBox(ctx, x, y, size, size, 0, p.line);
  text(ctx, label, x + size / 2, y + size / 2 - 14, `{s} ${MONO}`, 10, p.muted, { align: 'center', lh: 14 });
  text(ctx, '160×160', x + size / 2, y + size / 2, `{s} ${MONO}`, 10, p.muted, { align: 'center', lh: 14 });
}

export function drawNowPlaying(ctx: Ctx, p: Palette, o: NowPlayingOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);

  // ---- header (chunk 0) ----
  header(ctx, p);
  const glyph = findSprite(o.playing ? 'pause' : 'play');
  if (glyph && glyph.kind === 'glyph') drawGlyph(ctx, glyph, p, 8, 10);
  const pod = o.podcast;
  text(ctx, pod ? pod.index : '3/12', 24, 8, `{s} ${MONO}`, 11, p.dim, { lh: 14, spacing: 0.22 });
  if (!pod) {
    const sw = chip(ctx, 'SHUF', 62, 8, p.accent, p.accentDark);
    chip(ctx, 'RPT', 62 + sw + 7, 8, p.muted, p.line);
  }
  const clockW = text(ctx, '14:32', 283, 7, `{s} ${MONO}`, 12, p.text, { align: 'right', lh: 16 });
  battery(ctx, p, 290, 9);
  // Bluetooth, option A: icon, 4px, codec tag, 7px, clock, battery.
  let bx = Math.round(283 - clockW - 7);
  if (o.codec) bx -= btTag(ctx, p, o.codec, bx, 9, true) + 4;
  btIcon(ctx, p, o.bt ?? 'connected', bx - 11, 10);

  // ---- album art (8,40 160x160) ----
  coverPlaceholder(ctx, p, 8, 40, 160, pod ? 'show art' : 'cover');

  if (pod) {
    drawPodcastPlaying(ctx, p, pod);
    if (o.toast) drawToast(ctx, p, o.toast);
    if (o.grid) chunkGrid(ctx, p, [210, 30]);
    return;
  }

  // ---- metadata column (180,40 132 wide) ----
  ctx.font = `600 14px ${SANS}`;
  wrap(ctx, 'Everything In Its Right Place', 132, 2).forEach((line, i) =>
    text(ctx, line, 180, 40 + i * 21, `600 {s} ${SANS}`, 14, p.text, { lh: 21, spacing: -0.14 }),
  );
  text(ctx, 'Radiohead', 180, 92, `{s} ${SANS}`, 12, p.dim, { lh: 16, maxW: 132 });
  text(ctx, 'Kid A', 180, 110, `{s} ${SANS}`, 12, p.accent, { lh: 16, maxW: 132 });

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

  if (o.toast) drawToast(ctx, p, o.toast);
  if (o.grid) chunkGrid(ctx, p, [210, 30]);
}

/** Podcast metadata column and bottom band: title clamped to 5 lines, then the time left. */
function drawPodcastPlaying(ctx: Ctx, p: Palette, pod: PodcastPlaying) {
  ctx.font = `600 14px ${SANS}`;
  const lines = wrap(ctx, pod.title, 132, 5);
  lines.forEach((line, i) => text(ctx, line, 180, 40 + i * 16, `600 {s} ${SANS}`, 14, p.text, { lh: 16, spacing: -0.14 }));
  let y = 40 + lines.length * 16 + 8;
  text(ctx, pod.show, 180, y, `{s} ${SANS}`, 12, p.dim, { lh: 16, maxW: 132 });
  y += 16 + 2;
  if (pod.thirdMono) text(ctx, pod.third, 180, y, `{s} ${MONO}`, 11, p.muted, { lh: 14, maxW: 132 });
  else text(ctx, pod.third, 180, y, `{s} ${SANS}`, 12, p.muted, { lh: 16, maxW: 132 });

  text(ctx, pod.elapsed, 8, 212, `{s} ${MONO}`, 12, p.text, { lh: 16 });
  text(ctx, pod.left, 312, 212, `{s} ${MONO}`, 12, p.muted, { align: 'right', lh: 16 });
  ctx.fillStyle = p.line;
  rr(ctx, 8, 232, 304, 6, 3);
  ctx.fillStyle = p.accent;
  rr(ctx, 8, 232, Math.max(6, Math.round(304 * pod.frac)), 6, 3);
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
  /** List length, for the scrollbar and the fast-scroll letter count (default: tracks.length) */
  total?: number;
  /** Track-number column; library lists ("Songs") omit it (default: true) */
  numbered?: boolean;
  /** Wheel spinning fast: letter overlay over chunks 3–5 and a 7px thumb */
  fastScroll?: boolean;
}

export const VISIBLE_ROWS = 7;

export function drawTrackList(ctx: Ctx, p: Palette, o: TrackListOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);

  header(ctx, p);
  drawGlyph(ctx, CHEVRON, p, 8, 10);
  text(ctx, o.heading, 23, 0, `600 {s} ${SANS}`, 14, p.text, { lh: 30, maxW: TITLE_RIGHT - 23, spacing: -0.14 });
  btIcon(ctx, p, 'on', BT_ICON_X, BT_ICON_Y);
  battery(ctx, p, 290, 9);

  // With an explicit `total` larger than the sample, `tracks` is just the
  // visible window starting at `first`.
  const windowed = o.total !== undefined && o.total > o.tracks.length;
  const total = windowed ? o.total! : o.tracks.length;
  const first = Math.max(0, Math.min(o.first, total - VISIBLE_ROWS));
  const visible = windowed ? o.tracks.slice(0, VISIBLE_ROWS) : o.tracks.slice(first, first + VISIBLE_ROWS);
  const numbered = o.numbered ?? true;
  visible.forEach((t, i) => {
    const top = 30 + i * ROW_H;
    listRow(ctx, p, top, i === o.selected, (fg, sub) => {
      if (numbered) {
        const n = String(first + i + 1).padStart(2, '0');
        text(ctx, n, 28, top + 8, `{s} ${MONO}`, 11, sub, { align: 'right', lh: 14 });
      }
      text(ctx, t.title, 36, top + 6, `{s} ${SANS}`, 14, fg, { lh: 18, maxW: 224 });
      text(ctx, t.dur, 304, top + 8, `{s} ${MONO}`, 11, sub, { align: 'right', lh: 14 });
    });
  });

  scrollbar(ctx, p, 32, 204, VISIBLE_ROWS, total, first, !!o.fastScroll);
  const sel = visible[o.selected];
  if (o.fastScroll && sel) fastScrollOverlay(ctx, p, fastScrollLabel(sel.title, total));
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
  const qw = text(ctx, o.query, 42, 0, `{s} ${SANS}`, 14, p.text, { lh: 30, maxW: TITLE_RIGHT - 8 - 42 });
  if (typing) {
    ctx.fillStyle = p.accent;
    ctx.fillRect(Math.round(42 + qw + 1), 8, 2, 15);
  }
  btIcon(ctx, p, 'on', BT_ICON_X, BT_ICON_Y);
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

// Library list ("Songs"): alphabetical, no track numbers. Used as the visible
// window of a long list in the fast-scroll preview.
export const LIBRARY: Track[] = [
  { title: 'Rabbit In Your Headlights', dur: '6:59' },
  { title: 'Race For The Prize', dur: '4:18' },
  { title: 'Radio Ga Ga', dur: '5:48' },
  { title: 'Rain Dogs', dur: '2:56' },
  { title: 'Ramble On', dur: '4:35' },
  { title: 'Range Life', dur: '4:54' },
  { title: 'Rapture', dur: '5:33' },
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

// --- bluetooth ----------------------------------------------------------------------
// Settings and the Bluetooth screens (.agents/bluetooth-design/, README.md).
// Same grid as the track list: every row is one chunk, and the header icon has
// a fixed 11×11 box so a state change repaints only that box. The firmware
// mirrors this in software/src/ui/menu_view.hpp + bt_widgets.hpp.

export type BtIconState = 'off' | 'on' | 'connecting' | 'connected';

/** Header icon box, and where header titles stop. */
export const BT_ICON_X = 270;
export const BT_ICON_Y = 10;
const TITLE_RIGHT = BT_ICON_X - 8;

/**
 * The 11×11 icon: rune at x+2, plus two 2×3 ticks when connected. `blinkOn`
 * is the "connected" half of the connecting blink (500 ms each).
 */
export function btIcon(
  ctx: Ctx,
  p: Palette,
  state: BtIconState,
  x: number,
  y: number,
  opts: { blinkOn?: boolean; rune?: string; ticks?: string; bg?: string } = {},
) {
  const connected = state === 'connected' || (state === 'connecting' && !!opts.blinkOn);
  ctx.fillStyle = opts.bg ?? p.surface;
  ctx.fillRect(x, y, 11, 11);
  ctx.fillStyle = opts.rune ?? (state === 'off' ? p.muted : connected ? p.accent : p.text);
  BT_ROWS.forEach((row, ry) => {
    for (let rx = 0; rx < row.length; rx++) if (row[rx] === '#') ctx.fillRect(x + 2 + rx, y + ry, 1, 1);
  });
  if (connected || opts.ticks) {
    ctx.fillStyle = opts.ticks ?? p.accent;
    ctx.fillRect(x, y + 4, 2, 3);
    ctx.fillRect(x + 9, y + 4, 2, 3);
  }
}

/** Key/codec tag: 9px caps-ish, 1px border, r=2. Returns its width; `right` puts its right edge at x. */
function btTag(ctx: Ctx, p: Palette, label: string, x: number, y: number, right = false, fg = p.dim): number {
  ctx.font = `9px ${SANS}`;
  setSpacing(ctx, 0.72);
  const w = Math.ceil(ctx.measureText(label).width) + 7;
  setSpacing(ctx, 0);
  const left = right ? x - w : x;
  strokeBox(ctx, left, y, w, 13, 2, p.line);
  text(ctx, label, left + 3.5, y + 0.5, `{s} ${SANS}`, 9, fg, { lh: 12, spacing: 0.72 });
  return w;
}

/** 3 bars (3px, 1px gap, 4/7/10 tall) in an 11×10 box. */
export function signalBars(ctx: Ctx, p: Palette, x: number, y: number, bars: number, hi = false) {
  [4, 7, 10].forEach((h, i) => {
    ctx.fillStyle = i < bars ? (hi ? p.text : p.dim) : hi ? p.barOffHi : p.barOff;
    ctx.fillRect(x + i * 4, y + 10 - h, 3, h);
  });
}

export function rssiBars(rssi: number): number {
  return rssi >= -60 ? 3 : rssi >= -75 ? 2 : 1;
}

/** 22×12 switch, radius 6, 8×8 knob. */
function toggle(ctx: Ctx, p: Palette, x: number, y: number, on: boolean) {
  if (on) {
    ctx.fillStyle = p.accent;
    rr(ctx, x, y, 22, 12, 6);
  } else {
    strokeBox(ctx, x, y, 22, 12, 6, p.dim);
  }
  ctx.fillStyle = on ? p.bg : p.dim;
  ctx.beginPath();
  ctx.arc(on ? x + 22 - 2 - 4 : x + 2 + 4, y + 6, 4, 0, Math.PI * 2);
  ctx.fill();
}

const CHEVRON_R: GlyphDef = {
  kind: 'glyph',
  id: 'chevron_r_inline',
  w: 4,
  h: 8,
  tint: 'muted',
  rows: ['#...', '##..', '###.', '####', '####', '###.', '##..', '#...'],
  note: '',
};

/** Three 3×3 dots, 3px apart; the first `lit` are on. */
function scanDots(ctx: Ctx, p: Palette, x: number, y: number, lit: number) {
  for (let i = 0; i < 3; i++) {
    ctx.fillStyle = i < lit ? p.textHi : p.barOffHi;
    ctx.fillRect(x + i * 6, y, 3, 3);
  }
}

export type Tone = 'accent' | 'dim' | 'dimmer' | 'cream' | 'count' | 'disabled';

function toneColor(p: Palette, tone: Tone, hi: boolean): string {
  switch (tone) {
    case 'accent':
      return hi ? p.accentAlt : p.accent;
    case 'dimmer':
      return hi ? p.text : p.muted;
    case 'cream':
      return p.accentAlt;
    case 'count':
      return hi ? p.accentAlt : p.dim;
    case 'disabled':
      return p.disabled;
    default:
      return hi ? p.text : p.dim;
  }
}

export interface MenuRowSpec {
  label: string;
  /** readonly / disabled rows are skipped by the cursor */
  kind?: 'normal' | 'readonly' | 'disabled' | 'destructive' | 'failed';
  /** Scanning / Connecting: 1–3 lit dots after the label */
  dots?: number;
  note?: string;
  noteTone?: Tone;
  value?: string;
  valueTone?: Tone;
  mono?: boolean;
  bars?: number;
  trail?: 'chevron' | 'switchOn' | 'switchOff';
  /** Scan progress line width, 0–320 */
  progress?: number;
  /** Label in Mono 13 (a MAC address) */
  monoLabel?: boolean;
}

/** One row: fill, edge, label at x=10, status right-aligned at x=304 (8px apart). */
function menuRow(ctx: Ctx, p: Palette, top: number, r: MenuRowSpec, selected: boolean) {
  const failed = r.kind === 'failed';
  const lit = selected || failed;
  if (failed || selected) {
    ctx.fillStyle = failed ? p.error : p.accentDark;
    ctx.fillRect(0, top, W, ROW_H);
    ctx.fillStyle = failed ? p.accentAlt : p.accent;
    ctx.fillRect(0, top, 3, ROW_H);
  }
  let x = 304;
  if (r.trail === 'chevron') {
    x -= 4;
    drawGlyph(ctx, { ...CHEVRON_R, tint: lit ? 'text' : 'muted' }, p, x, top + 11);
    x -= 8;
  } else if (r.trail) {
    x -= 22;
    toggle(ctx, p, x, top + 9, r.trail === 'switchOn');
    x -= 8;
  }
  if (r.bars) {
    x -= 11;
    signalBars(ctx, p, x, top + 10, r.bars, lit);
    x -= 8;
  }
  if (r.value) {
    const font = r.mono ? `{s} ${MONO}` : `{s} ${SANS}`;
    ctx.font = font.replace('{s}', '11px');
    x -= ctx.measureText(r.value).width;
    text(ctx, r.value, x, top + 8, font, 11, toneColor(p, r.valueTone ?? 'dim', lit), { lh: 14 });
    x -= 8;
  }
  if (r.note) {
    ctx.font = `11px ${SANS}`;
    const shown = ellipsize(ctx, r.note, Math.max(0, x - 140));
    x -= ctx.measureText(shown).width;
    text(ctx, shown, x, top + 8, `{s} ${SANS}`, 11, toneColor(p, r.noteTone ?? 'dim', lit), { lh: 14 });
    x -= 8;
  }
  const fg =
    r.kind === 'readonly'
      ? p.muted
      : r.kind === 'disabled'
        ? p.disabled
        : r.kind === 'destructive'
          ? p.destructive
          : lit
            ? p.textHi
            : p.text;
  const dotsW = r.dots ? 6 + 15 : 0;
  const lw = r.monoLabel
    ? text(ctx, r.label, 10, top + 6, `{s} ${MONO}`, 13, p.dim, { lh: 18, maxW: x - 10 - dotsW })
    : text(ctx, r.label, 10, top + 6, `{s} ${SANS}`, 14, fg, { lh: 18, maxW: x - 10 - dotsW });
  if (r.dots) scanDots(ctx, p, Math.round(10 + lw + 6), top + 17, r.dots);
  if (r.progress) {
    ctx.fillStyle = p.bg;
    ctx.fillRect(0, top + ROW_H - 3, W, 2);
    ctx.fillStyle = p.accent;
    ctx.fillRect(0, top + ROW_H - 3, Math.round(r.progress), 2);
  }
  if (!lit) {
    ctx.fillStyle = p.separator;
    ctx.fillRect(0, top + ROW_H - 1, W, 1);
  }
}

export interface ToastSpec {
  tone: 'success' | 'neutral' | 'error';
  /** Plain text before / after the device name, which is drawn bold */
  before: string;
  name?: string;
  after?: string;
  /** Bluetooth rune before the text (default). Podcast toasts have none: the text starts at x=8. */
  icon?: boolean;
}

/** The toast takes over the header chunk for 2.5 s; never a list row. */
export function drawToast(ctx: Ctx, p: Palette, t: ToastSpec) {
  const fill = t.tone === 'success' ? p.accentDark : t.tone === 'error' ? p.error : p.line;
  const fg = t.tone === 'neutral' ? p.text : p.textHi;
  ctx.fillStyle = fill;
  ctx.fillRect(0, 0, W, 30);
  const icon = t.icon ?? true;
  if (t.tone === 'success') {
    ctx.fillStyle = p.accent;
    ctx.fillRect(0, 28, W, 2);
    if (icon) btIcon(ctx, p, 'connected', 8, 10, { rune: p.accentAlt, ticks: p.accentAlt, bg: fill });
  } else if (icon) {
    btIcon(ctx, p, 'on', 8, 10, { rune: p.text, bg: fill });
  }
  const x0 = icon ? 27 : 8;
  const maxW = W - 8 - x0;
  ctx.font = `13px ${SANS}`;
  const fixed = ctx.measureText(t.before + (t.after ?? '')).width;
  let x = x0;
  x += text(ctx, t.before, x, 0, `{s} ${SANS}`, 13, fg, { lh: 30 });
  if (t.name) x += text(ctx, t.name, x, 0, `600 {s} ${SANS}`, 13, fg, { lh: 30, maxW: Math.max(0, maxW - fixed) });
  if (t.after) text(ctx, t.after, x, 0, `{s} ${SANS}`, 13, fg, { lh: 30 });
}

function menuHeader(ctx: Ctx, p: Palette, title: string, icon: BtIconState, blinkOn = false) {
  header(ctx, p);
  drawGlyph(ctx, CHEVRON, p, 8, 10);
  text(ctx, title, 23, 0, `600 {s} ${SANS}`, 14, p.text, { lh: 30, maxW: TITLE_RIGHT - 23, spacing: -0.14 });
  btIcon(ctx, p, icon, BT_ICON_X, BT_ICON_Y, { blinkOn });
  battery(ctx, p, 290, 9);
}

function centeredHint(ctx: Ctx, p: Palette, fromChunk: number, title: string, sub: string) {
  const areaTop = fromChunk * ROW_H;
  const top = areaTop + Math.round((H - areaTop - 38) / 2);
  text(ctx, title, W / 2, top, `{s} ${SANS}`, 14, p.dim, { align: 'center', lh: 18 });
  text(ctx, sub, W / 2, top + 24, `{s} ${MONO}`, 11, p.muted, { align: 'center', lh: 14 });
}

export interface MenuScreenOpts {
  title: string;
  rows: MenuRowSpec[];
  selected: number;
  first?: number;
  /** Centered hint over the empty rows, from screen chunk `fromChunk` down */
  hint?: { fromChunk: number; title: string; sub: string };
  icon: BtIconState;
  blinkOn?: boolean;
  toast?: ToastSpec;
  grid: boolean;
}

/** Settings, the device list and the device screen: header + up to 7 rows. */
export function drawMenuScreen(ctx: Ctx, p: Palette, o: MenuScreenOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  menuHeader(ctx, p, o.title, o.icon, o.blinkOn);
  const first = o.first ?? 0;
  o.rows.slice(first, first + VISIBLE_ROWS).forEach((r, i) => menuRow(ctx, p, 30 + i * ROW_H, r, first + i === o.selected));
  if (o.hint) centeredHint(ctx, p, o.hint.fromChunk, o.hint.title, o.hint.sub);
  scrollbar(ctx, p, 32, 204, VISIBLE_ROWS, o.rows.length, first);
  if (o.toast) drawToast(ctx, p, o.toast);
  if (o.grid) chunkGrid(ctx, p);
}

// --- podcasts -------------------------------------------------------------------------

/** Episode / folder state, read by shape so it survives the highlight. */
export type PodMark = 'new' | 'progress' | 'none';

export interface PodRowSpec {
  title: string;
  mark: PodMark;
  /** Played episode, or a folder with everything played: DIM title */
  played?: boolean;
  /** Series episode number ("1/10"), Mono in a 28px column after the mark */
  number?: string;
  status?: string;
  /** accent: "N new" and the continue row's time left; dimmer: dates, "N ep", durations */
  statusTone?: 'accent' | 'dimmer';
  statusMono?: boolean;
  chevron?: boolean;
  /** In-progress episode: 2px underline, position / duration (0–1) */
  progress?: number;
}

export interface PodcastListOpts {
  title: string;
  rows: PodRowSpec[];
  selected: number;
  first?: number;
  toast?: ToastSpec;
  grid: boolean;
}

/** 6×6 mark at x=10: filled (new), 1px outline with the left 3px filled (in progress), nothing (played). */
function podMark(ctx: Ctx, color: string, x: number, y: number, mark: PodMark) {
  if (mark === 'none') return;
  ctx.fillStyle = color;
  if (mark === 'new') {
    ctx.fillRect(x, y, 6, 6);
    return;
  }
  ctx.fillRect(x, y, 3, 6);
  ctx.fillRect(x + 3, y, 3, 1);
  ctx.fillRect(x + 3, y + 5, 3, 1);
  ctx.fillRect(x + 5, y, 1, 6);
}

/** One podcast row: mark, [number], title from x=24, status and chevron right-aligned at x=304. */
function podRow(ctx: Ctx, p: Palette, top: number, r: PodRowSpec, hi: boolean) {
  if (hi) {
    ctx.fillStyle = p.accentDark;
    ctx.fillRect(0, top, W, ROW_H);
    ctx.fillStyle = p.accent;
    ctx.fillRect(0, top, 3, ROW_H);
  }
  podMark(ctx, hi ? p.accentAlt : p.accent, 10, top + 12, r.mark);
  let x = 304;
  if (r.chevron) {
    x -= 4;
    drawGlyph(ctx, { ...CHEVRON_R, tint: hi ? 'text' : 'muted' }, p, x, top + 11);
    x -= 8;
  }
  if (r.status) {
    const font = r.statusMono ? `{s} ${MONO}` : `{s} ${SANS}`;
    ctx.font = font.replace('{s}', '11px');
    x -= ctx.measureText(r.status).width;
    text(ctx, r.status, x, top + 8, font, 11, toneColor(p, r.statusTone ?? 'dimmer', hi), { lh: 14 });
    x -= 8;
  }
  let tx = 24;
  if (r.number) {
    text(ctx, r.number, tx, top + 8, `{s} ${MONO}`, 11, hi ? p.text : p.muted, { lh: 14 });
    tx += 28 + 8;
  }
  const fg = hi ? p.textHi : r.played ? p.dim : p.text;
  text(ctx, r.title, tx, top + 6, `{s} ${SANS}`, 14, fg, { lh: 18, maxW: x - tx });
  ctx.fillStyle = p.separator;
  ctx.fillRect(0, top + ROW_H - 1, W, 1);
  if (r.progress !== undefined) {
    ctx.fillStyle = hi ? p.bg : p.scrollTrack;
    ctx.fillRect(0, top + ROW_H - 2, W, 2);
    ctx.fillStyle = p.accent;
    ctx.fillRect(0, top + ROW_H - 2, Math.round(W * r.progress), 2);
  }
}

/** Podcasts root, publisher, show and series screens: the track list's grid with state marks. */
export function drawPodcastList(ctx: Ctx, p: Palette, o: PodcastListOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  menuHeader(ctx, p, o.title, 'on');
  const first = o.first ?? 0;
  o.rows.slice(first, first + VISIBLE_ROWS).forEach((r, i) => podRow(ctx, p, 30 + i * ROW_H, r, first + i === o.selected));
  scrollbar(ctx, p, 32, 204, VISIBLE_ROWS, o.rows.length, first);
  if (o.toast) drawToast(ctx, p, o.toast);
  if (o.grid) chunkGrid(ctx, p);
}

export interface ForgetOpts {
  name: string;
  /** 0 = Cancel (preselected), 1 = Forget */
  selected: number;
  icon: BtIconState;
  grid: boolean;
}

/** "Forget device?": text in chunks 1–3, Cancel in chunk 4, Forget in chunk 5. */
export function drawForget(ctx: Ctx, p: Palette, o: ForgetOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  menuHeader(ctx, p, 'Forget device?', o.icon);
  text(ctx, o.name, 10, 44, `600 {s} ${SANS}`, 14, p.text, { lh: 18, maxW: W - 26 });
  ctx.font = `12px ${SANS}`;
  wrap(ctx, 'will be unpaired. To use it again, pair it from a new scan.', W - 26, 3).forEach((line, i) =>
    text(ctx, line, 10, 68 + i * 17, `{s} ${SANS}`, 12, p.dim, { lh: 17 }),
  );
  menuRow(ctx, p, 120, { label: 'Cancel' }, o.selected === 0);
  menuRow(ctx, p, 150, { label: 'Forget', kind: 'destructive' }, o.selected === 1);
  if (o.grid) chunkGrid(ctx, p);
}

export interface RenameOpts {
  text: string;
  /** Character under the ribbon cursor (a–z, 0–9) */
  letter: string;
  broadcastName: string;
  icon: BtIconState;
  toast?: ToastSpec;
  grid: boolean;
}

const RENAME_CHARS = 'abcdefghijklmnopqrstuvwxyz0123456789'.split('');

/** Same input as search, lowercase and wrapping, with the key help in chunk 7. */
export function drawRename(ctx: Ctx, p: Palette, o: RenameOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  menuHeader(ctx, p, 'Rename', o.icon);

  // ribbon (chunk 1): the cursor stays centered, the ribbon wraps from 9 to a
  ctx.fillStyle = p.ribbon;
  ctx.fillRect(0, 30, W, 30);
  ctx.fillStyle = p.line;
  ctx.fillRect(0, 59, W, 1);
  const n = RENAME_CHARS.length;
  const ci = Math.max(0, RENAME_CHARS.indexOf(o.letter));
  const x0 = Math.floor((W - (13 * 19 + 12 * 2)) / 2);
  for (let i = 0; i < 13; i++) {
    const c = RENAME_CHARS[(((ci - 6 + i) % n) + n) % n];
    const x = x0 + i * 21;
    const on = i === 6;
    if (on) {
      ctx.fillStyle = p.accent;
      rr(ctx, x, 35, 19, 20, 2);
    }
    text(ctx, c, x + 9.5, 35, `{s} ${MONO}`, 13, on ? p.bg : p.muted, { align: 'center', lh: 20 });
  }

  // text (y 60–179): 30px line + 8 + 14px hint, centered
  const top = 60 + Math.round((120 - 52) / 2);
  const tw = text(ctx, o.text, 12, top, `600 {s} ${SANS}`, 24, p.text, { lh: 30, maxW: W - 24 - 4 });
  ctx.fillStyle = p.accent;
  ctx.fillRect(Math.round(12 + tw + 2), top + 3, 2, 24);
  const count = `${o.text.length}/24`;
  ctx.font = `11px ${MONO}`;
  const cw = ctx.measureText(count).width;
  text(ctx, `empty name restores “${o.broadcastName}”`, 12, top + 38, `{s} ${MONO}`, 11, p.muted, {
    lh: 14,
    maxW: W - 24 - cw - 8,
  });
  text(ctx, count, W - 12, top + 38, `{s} ${MONO}`, 11, p.muted, { align: 'right', lh: 14 });

  // footer (chunk 7)
  ctx.fillStyle = p.ribbon;
  ctx.fillRect(0, 210, W, 30);
  ctx.fillStyle = p.line;
  ctx.fillRect(0, 210, W, 1);
  const keys: [string, string][] = [
    ['MENU', 'cancel'],
    ['BACK', 'delete'],
    ['NEXT', 'space'],
    ['PLAY', 'save'],
  ];
  ctx.font = `10px ${MONO}`;
  const widths = keys.map(([k, l]) => {
    ctx.font = `9px ${SANS}`;
    const tag = Math.ceil(ctx.measureText(k).width + k.length * 0.72) + 7;
    ctx.font = `10px ${MONO}`;
    return tag + 4 + ctx.measureText(l).width;
  });
  const gap = (W - 16 - widths.reduce((a, b) => a + b, 0)) / (keys.length - 1);
  let x = 8;
  keys.forEach(([k, l], i) => {
    const tw2 = btTag(ctx, p, k, x, 218);
    text(ctx, l, x + tw2 + 4, 217, `{s} ${MONO}`, 10, p.dim, { lh: 14 });
    x += widths[i] + gap;
  });

  if (o.toast) drawToast(ctx, p, o.toast);
  if (o.grid) chunkGrid(ctx, p);
}

export interface BtIconsOpts {
  grid: boolean;
}

/** The four header icon states at 1× (left) and 6× (right), and the signal bars. */
export function drawBtIcons(ctx: Ctx, p: Palette, _o: BtIconsOpts) {
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);
  const states: [BtIconState, string, boolean][] = [
    ['off', 'Off — radio disabled', false],
    ['on', 'On, not connected', false],
    ['connecting', 'Connecting — blinks 500 ms', true],
    ['connected', 'Connected — ticks', false],
  ];
  states.forEach(([state, label, blink], i) => {
    const top = i * 45 + 6;
    ctx.fillStyle = p.surface;
    ctx.fillRect(0, top, W, 36);
    // 1× in its header box, then 6× (the connecting row shows both halves)
    btIcon(ctx, p, state, 12, top + 12);
    ctx.save();
    ctx.translate(40, top - 3);
    ctx.scale(3.5, 3.5);
    btIcon(ctx, p, state, 0, 0, { blinkOn: false });
    if (blink) btIcon(ctx, p, state, 13, 0, { blinkOn: true });
    ctx.restore();
    text(ctx, label, 140, top, `{s} ${SANS}`, 12, p.text, { lh: 36 });
  });
  const top = 4 * 45 + 10;
  text(ctx, 'signal', 12, top, `{s} ${MONO}`, 11, p.muted, { lh: 40 });
  [1, 2, 3].forEach((b, i) => {
    ctx.save();
    ctx.translate(70 + i * 62, top + 5);
    ctx.scale(3, 3);
    signalBars(ctx, p, 0, 0, b);
    ctx.restore();
  });
  text(ctx, '< −75 · −75…−60 · ≥ −60 dBm', 12, top + 32, `{s} ${MONO}`, 9, p.muted, { lh: 12 });
}

// Sample devices for the previews.
export interface SampleDevice {
  name: string;
  status: 'connected' | 'connecting' | 'paired' | 'found';
  rssi: number;
  mono?: boolean;
}

export const BT_DEVICES: SampleDevice[] = [
  { name: 'Sony WH-1000XM4', status: 'connected', rssi: -54 },
  { name: 'Kitchen', status: 'paired', rssi: -71 },
  { name: 'Car Audio', status: 'paired', rssi: -80 },
  { name: 'Bose QC45', status: 'found', rssi: -58 },
  { name: 'Marshall Acton II', status: 'found', rssi: -62 },
  { name: 'AirPods Pro', status: 'found', rssi: -69 },
  { name: 'UE BOOM 3', status: 'found', rssi: -77 },
  { name: '00:1A:7D:DA:71:13', status: 'found', rssi: -88, mono: true },
];

/** Device rows for the Bluetooth list. */
export function btDeviceRows(devices: SampleDevice[]): MenuRowSpec[] {
  return devices.map((d) => {
    const row: MenuRowSpec = { label: d.name, monoLabel: d.mono };
    if (d.status === 'connected') return { ...row, note: 'Connected', noteTone: 'accent' };
    if (d.status === 'connecting') return { ...row, note: 'Connecting', noteTone: 'dim' };
    if (d.status === 'paired') return { ...row, note: 'Paired', noteTone: 'dimmer' };
    return { ...row, bars: rssiBars(d.rssi) };
  });
}
