<script lang="ts" setup>
import { onMounted, watch, ref } from 'vue';
import type { Palette } from './palette';
import { drawGlyph, drawSpriteAt, findSprite, rr } from './sprites';

const props = defineProps<{
  palette: Palette;
  mode: 'play' | 'volume' | 'seek';
  playing: boolean;
  grid: boolean;
  zoom: number;
}>();

const canvas = ref<HTMLCanvasElement | null>(null);

const W = 320;
const H = 240;
const MONO = '"IBM Plex Mono", "Courier New", ui-monospace, monospace';
const SANS = '"IBM Plex Sans", system-ui, -apple-system, sans-serif';

function text(
  ctx: CanvasRenderingContext2D,
  s: string,
  x: number,
  y: number,
  font: string,
  fill: string,
  align: CanvasTextAlign = 'left',
) {
  ctx.font = font;
  ctx.fillStyle = fill;
  ctx.textAlign = align;
  ctx.textBaseline = 'top';
  ctx.fillText(s, x, y);
}

function chip(
  ctx: CanvasRenderingContext2D,
  label: string,
  x: number,
  y: number,
  active: boolean,
  p: Palette,
) {
  const w = label.length * 6 + 8;
  ctx.lineWidth = 1;
  ctx.strokeStyle = active ? p.accent : p.line;
  ctx.beginPath();
  if (typeof (ctx as CanvasRenderingContext2D & { roundRect?: unknown }).roundRect === 'function') {
    (ctx as CanvasRenderingContext2D & { roundRect: (x: number, y: number, w: number, h: number, r: number) => void }).roundRect(
      x + 0.5, y + 0.5, w, 10, 3,
    );
  } else {
    ctx.rect(x + 0.5, y + 0.5, w, 10);
  }
  ctx.stroke();
  text(ctx, label, x + 4, y + 2, `600 8px ${SANS}`, active ? p.accent : p.muted);
}

function border(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, color: string) {
  ctx.fillStyle = color;
  ctx.fillRect(x - 1, y - 1, w + 2, 1); // top
  ctx.fillRect(x - 1, y + h, w + 2, 1); // bottom
  ctx.fillRect(x - 1, y - 1, 1, h + 2); // left
  ctx.fillRect(x + w, y - 1, 1, h + 2); // right
}

function fakeCover(ctx: CanvasRenderingContext2D, x: number, y: number, size: number, p: Palette) {
  const g = ctx.createLinearGradient(x, y, x + size, y + size);
  g.addColorStop(0, p.accentDark);
  g.addColorStop(1, p.accent);
  ctx.fillStyle = g;
  ctx.fillRect(x, y, size, size);
  ctx.strokeStyle = p.text;
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.arc(x + size / 2, y + size / 2, 42, 0, Math.PI * 2);
  ctx.stroke();
  ctx.fillStyle = p.text;
  ctx.beginPath();
  ctx.arc(x + size / 2, y + size / 2, 11, 0, Math.PI * 2);
  ctx.fill();
}

function drawScreen(p: Palette, mode: 'play' | 'volume' | 'seek', playing: boolean, grid: boolean) {
  const el = canvas.value;
  if (!el) return;
  el.width = W;
  el.height = H;
  const ctx = el.getContext('2d');
  if (!ctx) return;

  // background
  ctx.fillStyle = p.bg;
  ctx.fillRect(0, 0, W, H);

  // ---- header (0,0 320x30) ----
  ctx.fillStyle = p.surface;
  ctx.fillRect(0, 0, W, 30);
  ctx.fillStyle = p.line;
  ctx.fillRect(0, 29, W, 1);

  const glyph = findSprite(playing ? 'pause' : 'play');
  if (glyph && glyph.kind === 'glyph') drawGlyph(ctx, glyph, p, 8, 10);

  text(ctx, '3/12', 24, 9, `500 11px ${MONO}`, p.text);
  chip(ctx, 'SHUF', 62, 9, true, p);
  chip(ctx, 'RPT', 110, 9, false, p);
  text(ctx, '12:34', 286, 7, `500 12px ${MONO}`, p.text, 'right');

  const battery = findSprite('battery');
  if (battery) {
    drawSpriteAt(ctx, battery, p, 290, 9);
    // fill cells (2 filled, 1 empty) — drawn as rects so level is free
    ctx.fillStyle = p.text;
    ctx.fillRect(293, 12, 4, 5);
    ctx.fillRect(298, 12, 4, 5);
    ctx.fillStyle = p.line;
    ctx.fillRect(303, 12, 4, 5);
  }

  // ---- album art (8,40 160x160) ----
  const artX = 8;
  const artY = 40;
  const artSize = 160;
  fakeCover(ctx, artX, artY, artSize, p);
  border(ctx, artX, artY, artSize, artSize, p.line);

  // ---- metadata column ----
  text(ctx, 'Bohemian', 180, 40, `600 16px ${SANS}`, p.text);
  text(ctx, 'Rhapsody', 180, 57, `600 16px ${SANS}`, p.text);
  text(ctx, 'Queen', 180, 80, `12px ${SANS}`, p.dim);
  text(ctx, 'A Night at the Opera', 180, 96, `12px ${SANS}`, p.muted);

  // ---- bottom band (0,210 320x30) ----
  if (mode === 'volume') {
    // SURFACE panel replaces the band
    ctx.fillStyle = p.surface;
    ctx.fillRect(0, 210, W, 30);
    const spk = findSprite('speaker');
    if (spk && spk.kind === 'glyph') drawGlyph(ctx, spk, p, 36, 220);
    const vx = 56;
    const vy = 221;
    const vw = 220;
    const vh = 8;
    ctx.fillStyle = p.line;
    rr(ctx, vx, vy, vw, vh, vh / 2);
    ctx.fillStyle = p.accent;
    rr(ctx, vx, vy, Math.round(vw * 0.65), vh, vh / 2);
  } else if (mode === 'seek') {
    text(ctx, '2:53', 8, 212, `500 12px ${MONO}`, p.accent);
    text(ctx, '4:56', 312, 212, `500 12px ${MONO}`, p.muted, 'right');
    const bx = 8;
    const by = 228;
    const bw = 304;
    const bh = 10;
    const frac = 0.62;
    ctx.fillStyle = p.line;
    rr(ctx, bx, by, bw, bh, 2);
    ctx.fillStyle = p.accentDark;
    rr(ctx, bx, by, Math.round(bw * frac), bh, 2);
    const handle = findSprite('handle');
    if (handle) drawSpriteAt(ctx, handle, p, Math.round(bx + frac * bw - 3), by - 2);
  } else {
    text(ctx, '1:23', 8, 212, `500 12px ${MONO}`, p.text);
    text(ctx, '4:56', 312, 212, `500 12px ${MONO}`, p.muted, 'right');
    const bx = 8;
    const by = 232;
    const bw = 304;
    const bh = 6;
    const frac = 0.33;
    ctx.fillStyle = p.line;
    rr(ctx, bx, by, bw, bh, bh / 2);
    ctx.fillStyle = p.accent;
    rr(ctx, bx, by, Math.round(bw * frac), bh, bh / 2);
  }

  // ---- chunk grid overlay ----
  if (grid) {
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.18)';
    ctx.lineWidth = 1;
    for (let y = 0; y <= H; y += 30) {
      ctx.beginPath();
      ctx.moveTo(0, y + 0.5);
      ctx.lineTo(W, y + 0.5);
      ctx.stroke();
    }
    ctx.fillStyle = 'rgba(255, 255, 255, 0.06)';
    ctx.fillRect(0, 210, W, 30);
  }
}

function redraw() {
  drawScreen(props.palette, props.mode, props.playing, props.grid);
}

onMounted(() => {
  redraw();
  if (typeof document !== 'undefined' && document.fonts) {
    // Re-render once webfonts settle so text uses the intended face.
    document.fonts.ready.then(() => redraw());
  }
});

watch(() => props.palette, redraw, { deep: true });
watch(() => [props.mode, props.playing, props.grid], redraw);
</script>

<template>
  <canvas ref="canvas" class="podscreen" :style="{ width: `${320 * zoom}px`, height: `${240 * zoom}px` }" />
</template>

<style scoped>
.podscreen {
  image-rendering: pixelated;
  display: block;
  border-radius: 4px;
}
</style>
