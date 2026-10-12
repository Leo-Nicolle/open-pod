<script lang="ts" setup>
import { onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { hexToRgb, type Palette } from './palette';
import {
  BOOT,
  bootFrame,
  logoPalette,
  squashPos,
  wordRamp,
  WORDMARK_ACCENT_FROM,
  type LogoRaster,
  type Wordmark,
} from './logo';
import { drawTrackList, H, ROW_H, W } from './screens';

// The title screen at time `t` (ms), composed from the same rasters the
// firmware blits: indexed logo frames, the wordmark coverage strip, and the
// exit wipe into the root menu. Without `t` it loops, like the mock.
const props = defineProps<{
  palette: Palette;
  logo: LogoRaster;
  squash: LogoRaster;
  word: Wordmark | null;
  zoom: number;
  t?: number;
  playing?: boolean;
}>();

// The firmware's root menu (software/src/state/static_menu.cpp).
const ROOT_MENU = ['Tracks', 'Artists', 'Albums', 'Genres', 'Search', 'Settings', 'Now Playing'];

const canvas = ref<HTMLCanvasElement | null>(null);
let raf = 0;
let start = 0;
let pausedAt = 0;
let menu: HTMLCanvasElement | null = null;

function time(now: number): number {
  if (props.t !== undefined) return props.t;
  return ((props.playing === false ? pausedAt : now) - start) % BOOT.PERIOD;
}

function renderMenu() {
  menu = document.createElement('canvas');
  menu.width = W;
  menu.height = H;
  const ctx = menu.getContext('2d');
  if (!ctx) return;
  drawTrackList(ctx, props.palette, {
    heading: 'OpenPod',
    tracks: ROOT_MENU.map((title) => ({ title, dur: '' })),
    first: 0,
    selected: 0,
    grid: false,
    numbered: false,
  });
}

function blitIndexed(img: ImageData, r: LogoRaster, rgb: number[][], ox: number, oy: number) {
  for (let y = 0; y < r.h; y++) {
    const sy = oy + y;
    if (sy < 0 || sy >= H) continue;
    for (let x = 0; x < r.w; x++) {
      const v = r.idx[y * r.w + x];
      const sx = ox + x;
      if (!v || sx < 0 || sx >= W) continue;
      const o = (sy * W + sx) * 4;
      img.data[o] = rgb[v][0];
      img.data[o + 1] = rgb[v][1];
      img.data[o + 2] = rgb[v][2];
    }
  }
}

function draw(t: number) {
  const ctx = canvas.value?.getContext('2d');
  if (!ctx) return;
  const p = props.palette;
  const f = bootFrame(t);
  const img = ctx.createImageData(W, H);
  const [br, bg, bb] = hexToRgb(p.bg);
  for (let i = 0; i < img.data.length; i += 4) {
    img.data[i] = br;
    img.data[i + 1] = bg;
    img.data[i + 2] = bb;
    img.data[i + 3] = 255;
  }

  const rgb = logoPalette(p).map(hexToRgb);
  if (f.logo === 'normal') blitIndexed(img, props.logo, rgb, BOOT.LOGO_X, BOOT.LOGO_Y + f.dy);
  else if (f.logo === 'squash') {
    const s = squashPos(props.logo, props.squash);
    blitIndexed(img, props.squash, rgb, s.x, s.y);
  }

  const word = props.word;
  if (word && f.letters) {
    const ramps = [wordRamp(p, 'text').map(hexToRgb), wordRamp(p, 'accent').map(hexToRgb)];
    for (let x = 0; x < word.split[f.letters]; x++) {
      let glyph = 0;
      while (x >= word.split[glyph + 1]) glyph++;
      const ramp = ramps[glyph < WORDMARK_ACCENT_FROM ? 0 : 1];
      for (let y = 0; y < word.h; y++) {
        const c = ramp[word.cov[y * word.w + x]];
        const o = ((word.y + y) * W + word.x + x) * 4;
        img.data[o] = c[0];
        img.data[o + 1] = c[1];
        img.data[o + 2] = c[2];
      }
    }
  }

  ctx.putImageData(img, 0, 0);
  if (f.chunks && menu) ctx.drawImage(menu, 0, 0, W, f.chunks * ROW_H, 0, 0, W, f.chunks * ROW_H);
}

function loop(now: number) {
  draw(time(now));
  raf = requestAnimationFrame(loop);
}

function setup() {
  const c = canvas.value;
  if (!c) return;
  c.width = W;
  c.height = H;
  c.style.width = `${W * props.zoom}px`;
}

onMounted(() => {
  setup();
  renderMenu();
  start = performance.now();
  pausedAt = start;
  if (props.t === undefined) raf = requestAnimationFrame(loop);
  else draw(props.t);
});

onBeforeUnmount(() => cancelAnimationFrame(raf));

watch(
  () => props.playing,
  (playing) => {
    const now = performance.now();
    if (playing === false) pausedAt = now;
    else start += now - pausedAt; // resume where it stopped
  },
);

// A static frame repaints on input changes; the loop picks them up by itself.
watch(
  () => props.palette,
  () => {
    renderMenu();
    if (props.t !== undefined) draw(props.t);
  },
  { deep: true },
);
watch(
  () => [props.logo, props.squash, props.word, props.t, props.zoom],
  () => {
    setup();
    if (props.t !== undefined) draw(props.t);
  },
);
</script>

<template>
  <canvas ref="canvas" class="boot-stage" />
</template>

<style scoped>
.boot-stage {
  image-rendering: pixelated;
  display: block;
  max-width: 100%;
  height: auto;
  border-radius: 4px;
}
</style>
