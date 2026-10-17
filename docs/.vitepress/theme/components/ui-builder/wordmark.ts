// "opentone" wordmark of the title screen, rendered by the browser and baked
// into logo.h as 4-bit coverage (the firmware has no 28px font).
//
// Layout follows the title-screen handoff: IBM Plex Sans 600 at 28px,
// letter-spacing -0.01em, centred on the screen in a line box of height 32
// whose top is at y=174. As in the mock, every letter is its own flex item, so
// there is no kerning between letters: each pen step is the glyph's advance
// plus the letter-spacing. Browser only (needs canvas).
//
// The font is the repo's own IBM Plex Sans SemiBold (the TTF the firmware fonts
// are converted from, scripts/), loaded under a private family name: a missing
// web font must fail the export, not silently bake a fallback face.

import plexSemiBoldUrl from './fonts/IBMPlexSans-SemiBold.ttf?url';
import { WORDMARK, type Wordmark } from './logo';

const SIZE = 28;
const FAMILY = 'OpenpodWordmark';
const FONT = `600 ${SIZE}px "${FAMILY}"`;
const LETTER_SPACING = -0.01 * SIZE;
const LINE_TOP = 174;
const LINE_HEIGHT = 32;
const SCREEN_W = 320;
const SCREEN_H = 240;

export async function renderWordmark(): Promise<Wordmark> {
  if (![...document.fonts].some((f) => f.family === FAMILY && f.status === 'loaded')) {
    const face = new FontFace(FAMILY, `url(${plexSemiBoldUrl})`, { weight: '600' });
    document.fonts.add(await face.load());
  }

  const canvas = document.createElement('canvas');
  canvas.width = SCREEN_W;
  canvas.height = SCREEN_H;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  if (!ctx) throw new Error('2D canvas unavailable');
  ctx.font = FONT;
  ctx.textBaseline = 'alphabetic';
  ctx.fillStyle = '#fff';

  const chars = [...WORDMARK];
  const pens: number[] = [];
  let pen = 0;
  for (const ch of chars) {
    pens.push(pen);
    pen += ctx.measureText(ch).width + LETTER_SPACING;
  }
  const left = (SCREEN_W - pen) / 2;
  const m = ctx.measureText(WORDMARK);
  const asc = m.fontBoundingBoxAscent;
  const desc = m.fontBoundingBoxDescent;
  const baseline = LINE_TOP + (LINE_HEIGHT - (asc + desc)) / 2 + asc;
  chars.forEach((ch, i) => ctx.fillText(ch, left + pens[i], baseline));

  // Crop to the ink.
  const a = ctx.getImageData(0, 0, SCREEN_W, SCREEN_H).data;
  let x0 = SCREEN_W;
  let y0 = SCREEN_H;
  let x1 = -1;
  let y1 = -1;
  for (let y = 0; y < SCREEN_H; y++) {
    for (let x = 0; x < SCREEN_W; x++) {
      if (a[(y * SCREEN_W + x) * 4 + 3] < 9) continue; // rounds to coverage 0
      x0 = Math.min(x0, x);
      x1 = Math.max(x1, x);
      y0 = Math.min(y0, y);
      y1 = Math.max(y1, y);
    }
  }
  if (x1 < 0) throw new Error('wordmark rendered empty');
  const w = x1 - x0 + 1;
  const h = y1 - y0 + 1;
  const cov = new Uint8Array(w * h);
  const colSum = new Array<number>(w).fill(0);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const v = Math.round((a[((y0 + y) * SCREEN_W + x0 + x) * 4 + 3] / 255) * 15);
      cov[y * w + x] = v;
      colSum[x] += v;
    }
  }

  // Glyph bands partition the strip; each boundary sits on the emptiest
  // column near the pen position, so revealing a glyph never cuts a neighbour.
  const split = [0];
  for (let i = 1; i < chars.length; i++) {
    const c = Math.round(left + pens[i]) - x0;
    let best = -1;
    for (let x = Math.max(split[i - 1] + 1, c - 2); x <= Math.min(w - 1, c + 2); x++) {
      if (best < 0 || colSum[x] < colSum[best] || (colSum[x] === colSum[best] && Math.abs(x - c) < Math.abs(best - c))) {
        best = x;
      }
    }
    split.push(best < 0 ? Math.min(w - 1, split[i - 1] + 1) : best);
  }
  split.push(w);

  return { x: x0, y: y0, w, h, cov, split };
}
