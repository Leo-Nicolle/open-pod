<script lang="ts" setup>
import { onBeforeUnmount, onMounted, watch, ref } from 'vue';
import type { Palette } from './palette';
import {
  drawBtIcons,
  drawForget,
  drawMenuScreen,
  drawNowPlaying,
  drawPodcastList,
  drawRename,
  drawSearch,
  drawTrackList,
  H,
  W,
} from './screens';
import type { Screen } from './screenStates';

const props = defineProps<{
  palette: Palette;
  screen: Screen;
  zoom?: number;
}>();

const canvas = ref<HTMLCanvasElement | null>(null);

let frame = 0;

// Palette edits arrive in bursts (color picker drags); paint at most once per frame.
function scheduleRedraw() {
  if (frame) return;
  frame = requestAnimationFrame(() => {
    frame = 0;
    redraw();
  });
}

onBeforeUnmount(() => cancelAnimationFrame(frame));

function redraw() {
  const el = canvas.value;
  if (!el) return;
  el.width = W;
  el.height = H;
  const ctx = el.getContext('2d');
  if (!ctx) return;
  const s = props.screen;
  if (s.kind === 'nowplaying') drawNowPlaying(ctx, props.palette, s.opts);
  else if (s.kind === 'tracklist') drawTrackList(ctx, props.palette, s.opts);
  else if (s.kind === 'search') drawSearch(ctx, props.palette, s.opts);
  else if (s.kind === 'menu') drawMenuScreen(ctx, props.palette, s.opts);
  else if (s.kind === 'podcast') drawPodcastList(ctx, props.palette, s.opts);
  else if (s.kind === 'forget') drawForget(ctx, props.palette, s.opts);
  else if (s.kind === 'rename') drawRename(ctx, props.palette, s.opts);
  else drawBtIcons(ctx, props.palette, s.opts);
}

onMounted(() => {
  redraw();
  if (typeof document !== 'undefined' && document.fonts) {
    // Re-render once webfonts settle so text uses the intended face.
    document.fonts.ready.then(() => redraw());
  }
});

watch(() => [props.palette, props.screen], scheduleRedraw, { deep: true });
</script>

<template>
  <canvas
    ref="canvas"
    class="screen"
    :style="{ width: `${W * (zoom ?? 1)}px` }"
  />
</template>

<style scoped>
.screen {
  image-rendering: pixelated;
  display: block;
  max-width: 100%;
  height: auto;
  aspect-ratio: 4 / 3;
  border-radius: 4px;
}
</style>
