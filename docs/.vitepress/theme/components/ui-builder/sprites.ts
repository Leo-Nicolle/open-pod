// Sprite definitions for the Now Playing UI.
//
// Two kinds:
//  - "glyph"  : a 1-bit shape defined by ASCII rows, tinted with a palette color
//               at blit time. Exported as a pixel array (tinted) + a 1-bit mask.
//  - "opaque" : a full-color sprite drawn by a function. Exported as a pixel
//               array only (mask = nullptr).
//
// Draw functions use canvas 2D and run in "sprite pixel" coordinates (0..w, 0..h);
// callers scale the context (for previews) or supersample then downsample (for
// the RGB565 export).

import type { Palette, PaletteKey } from './palette';

export interface GlyphDef {
  kind: 'glyph';
  id: string;
  w: number;
  h: number;
  tint: PaletteKey;
  rows: string[];
  note: string;
}

export interface OpaqueDef {
  kind: 'opaque';
  id: string;
  w: number;
  h: number;
  note: string;
  draw: (ctx: CanvasRenderingContext2D, p: Palette) => void;
}

export type SpriteDef = GlyphDef | OpaqueDef;

/** Rounded-rect fill with a fallback for contexts lacking roundRect. */
export function rr(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  w: number,
  h: number,
  r: number,
): void {
  ctx.beginPath();
  const c = ctx as CanvasRenderingContext2D & { roundRect?: (...a: unknown[]) => void };
  if (typeof c.roundRect === 'function') {
    c.roundRect(x, y, w, h, r);
  } else {
    ctx.rect(x, y, w, h);
  }
  ctx.fill();
}

/** Draw a glyph (1-bit, hard edges) at (ox, oy) at 1x, tinted with its palette color. */
export function drawGlyph(
  ctx: CanvasRenderingContext2D,
  def: GlyphDef,
  p: Palette,
  ox = 0,
  oy = 0,
): void {
  ctx.fillStyle = p[def.tint];
  for (let y = 0; y < def.h; y++) {
    const row = def.rows[y] ?? '';
    for (let x = 0; x < def.w; x++) {
      if ((row[x] ?? '.') !== '.') ctx.fillRect(ox + x, oy + y, 1, 1);
    }
  }
}

/** Draw any sprite translated to (x, y). */
export function drawSpriteAt(
  ctx: CanvasRenderingContext2D,
  def: SpriteDef,
  p: Palette,
  x: number,
  y: number,
): void {
  ctx.save();
  ctx.translate(x, y);
  if (def.kind === 'glyph') drawGlyph(ctx, def, p, 0, 0);
  else def.draw(ctx, p);
  ctx.restore();
}

/** Render a sprite to a canvas at `scale`x (used by the live preview). */
export function drawSpriteToCanvas(
  canvas: HTMLCanvasElement,
  def: SpriteDef,
  p: Palette,
  scale: number,
  grid: boolean,
): void {
  canvas.width = def.w * scale;
  canvas.height = def.h * scale;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.save();
  ctx.scale(scale, scale);
  if (def.kind === 'glyph') drawGlyph(ctx, def, p, 0, 0);
  else def.draw(ctx, p);
  ctx.restore();

  if (grid && scale > 1) {
    ctx.strokeStyle = 'rgba(242, 237, 230, 0.08)';
    ctx.lineWidth = 1;
    for (let x = 1; x < def.w; x++) {
      ctx.beginPath();
      ctx.moveTo(x * scale + 0.5, 0);
      ctx.lineTo(x * scale + 0.5, def.h * scale);
      ctx.stroke();
    }
    for (let y = 1; y < def.h; y++) {
      ctx.beginPath();
      ctx.moveTo(0, y * scale + 0.5);
      ctx.lineTo(def.w * scale, y * scale + 0.5);
      ctx.stroke();
    }
  }
}

const PLAY_ROWS = [
  '#........',
  '##.......',
  '###......',
  '####.....',
  '#####....',
  '######...',
  '#####....',
  '####.....',
  '###......',
  '##.......',
  '#........',
];

const PAUSE_ROWS = Array(11).fill('###..###.');

const SPEAKER_ROWS = [
  '.....##.....',
  '....###..#..',
  '..####...#..',
  '.#####.#.#..',
  '.#####.#.#..',
  '.#####.#.#..',
  '.#####.#.#..',
  '..####...#..',
  '....###..#..',
  '.....##.....',
];

export const SPRITES: SpriteDef[] = [
  {
    kind: 'glyph',
    id: 'play',
    w: 9,
    h: 11,
    tint: 'accent',
    rows: PLAY_ROWS,
    note: 'Header transport glyph. 1-bit mask, tinted COLOR_ACCENT at blit time.',
  },
  {
    kind: 'glyph',
    id: 'pause',
    w: 9,
    h: 11,
    tint: 'accent',
    rows: PAUSE_ROWS,
    note: 'Same box as play so the header never reflows.',
  },
  {
    kind: 'glyph',
    id: 'speaker',
    w: 12,
    h: 10,
    tint: 'text',
    rows: SPEAKER_ROWS,
    note: 'Volume band icon, tinted COLOR_TEXT.',
  },
  {
    kind: 'opaque',
    id: 'battery',
    w: 22,
    h: 11,
    note: 'Header battery outline + nub. The 3 fill cells (4×5 each) are drawn as rects so the level is free.',
    draw(ctx, p) {
      ctx.fillStyle = p.muted;
      ctx.fillRect(0, 0, 20, 11); // outer frame
      ctx.fillStyle = p.bg;
      ctx.fillRect(1, 1, 18, 9); // inner cutout
      ctx.fillStyle = p.muted;
      ctx.fillRect(20, 3, 2, 5); // nub
    },
  },
  {
    kind: 'opaque',
    id: 'bar6',
    w: 60,
    h: 6,
    note: 'Idle progress bar (composed preview). Store bar6_slice instead and stretch the 1px mid.',
    draw(ctx, p) {
      ctx.fillStyle = p.line;
      rr(ctx, 0, 0, 60, 6, 3);
      ctx.fillStyle = p.accent;
      rr(ctx, 0, 0, 26, 6, 3);
    },
  },
  {
    kind: 'opaque',
    id: 'bar6_slice',
    w: 26,
    h: 6,
    note: '9-slice atlas: 3px round caps + repeatable mid, in fill (accent) and track (line) variants.',
    draw(ctx, p) {
      ctx.fillStyle = p.accent;
      rr(ctx, 0, 0, 3, 6, 3); // left cap (fill)
      ctx.fillRect(5, 0, 6, 6); // mid (fill)
      ctx.fillStyle = p.line;
      rr(ctx, 13, 0, 3, 6, 3); // right cap (track)
      ctx.fillRect(18, 0, 6, 6); // mid (track)
    },
  },
  {
    kind: 'opaque',
    id: 'bar10',
    w: 60,
    h: 10,
    note: 'Seek-mode bar: 10px tall, 2px radius, fill drops to COLOR_ACCENT_DK so the handle reads on top.',
    draw(ctx, p) {
      ctx.fillStyle = p.line;
      rr(ctx, 0, 0, 60, 10, 2);
      ctx.fillStyle = p.accentDark;
      rr(ctx, 0, 0, 31, 10, 2);
    },
  },
  {
    kind: 'opaque',
    id: 'handle',
    w: 7,
    h: 14,
    note: 'Seek handle with a 1px notch. Blit at x = 8 + pos*304 - 3, y = 226.',
    draw(ctx, p) {
      ctx.fillStyle = p.accentAlt;
      rr(ctx, 0, 0, 7, 14, 2);
      ctx.fillStyle = p.bg;
      ctx.fillRect(3, 4, 1, 6);
    },
  },
  {
    kind: 'opaque',
    id: 'tick',
    w: 3,
    h: 30,
    note: 'Optional chapter/marker tick drawn over the band.',
    draw(ctx, p) {
      ctx.fillStyle = p.line;
      ctx.fillRect(1, 0, 1, 30);
      ctx.fillStyle = p.accent;
      ctx.fillRect(0, 0, 3, 4);
      ctx.fillRect(0, 26, 3, 4);
    },
  },
];

export function findSprite(id: string): SpriteDef | undefined {
  return SPRITES.find((s) => s.id === id);
}
