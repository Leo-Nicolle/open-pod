<script lang="ts" setup>
import { onBeforeUnmount, onMounted, watch, ref } from 'vue';
import type { Palette } from './palette';
import type { SpriteDef } from './sprites';
import { drawSpriteToCanvas } from './sprites';

const props = defineProps<{
  def: SpriteDef;
  palette: Palette;
  scale: number;
  grid?: boolean;
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
  if (!canvas.value) return;
  drawSpriteToCanvas(canvas.value, props.def, props.palette, props.scale, props.grid ?? false);
}

onMounted(redraw);
watch(() => props.palette, scheduleRedraw, { deep: true });
watch(() => [props.def, props.scale, props.grid], scheduleRedraw);
</script>

<template>
  <canvas ref="canvas" class="sprite-stage" />
</template>

<style scoped>
.sprite-stage {
  image-rendering: pixelated;
  display: block;
}
</style>
