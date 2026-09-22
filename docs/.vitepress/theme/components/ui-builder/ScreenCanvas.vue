<script lang="ts" setup>
import { onMounted, watch, ref } from 'vue';
import type { Palette } from './palette';
import {
  drawNowPlaying,
  drawSearch,
  drawTrackList,
  H,
  W,
  type NowPlayingOpts,
  type SearchOpts,
  type TrackListOpts,
} from './screens';

type Screen =
  | { kind: 'nowplaying'; opts: NowPlayingOpts }
  | { kind: 'tracklist'; opts: TrackListOpts }
  | { kind: 'search'; opts: SearchOpts };

const props = defineProps<{
  palette: Palette;
  screen: Screen;
  zoom?: number;
}>();

const canvas = ref<HTMLCanvasElement | null>(null);

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
  else drawSearch(ctx, props.palette, s.opts);
}

onMounted(() => {
  redraw();
  if (typeof document !== 'undefined' && document.fonts) {
    // Re-render once webfonts settle so text uses the intended face.
    document.fonts.ready.then(() => redraw());
  }
});

watch(() => [props.palette, props.screen], redraw, { deep: true });
</script>

<template>
  <canvas
    ref="canvas"
    class="screen"
    :style="{ width: `${W * (zoom ?? 1)}px`, height: `${H * (zoom ?? 1)}px` }"
  />
</template>

<style scoped>
.screen {
  image-rendering: pixelated;
  display: block;
  border-radius: 4px;
}
</style>
