<script setup lang="ts">
// Album art from the backend's cover:// thumbnails; the design's striped
// tile when there is none.
import { computed, ref, watch } from 'vue';
import { api } from '../api';

const props = defineProps<{ path: string; seed?: number }>();
const failed = ref(false);
watch(() => props.path, () => (failed.value = false));

const tone = computed(() => {
  if (props.seed !== undefined) return props.seed % 5;
  const m = /^tone:(\d+)$/.exec(props.path);
  if (m) return Number(m[1]) % 5;
  let h = 0;
  for (const c of props.path) h = (h * 31 + c.charCodeAt(0)) | 0;
  return Math.abs(h) % 5;
});
// The backend gives a URL, or (in the browser) a promise of one.
const src = ref<string | null>(null);
watch(
  () => props.path,
  async (path) => {
    src.value = null;
    if (!path || path.startsWith('tone:')) return;
    const url = await api.coverUrl(path);
    if (props.path === path) src.value = url;
  },
  { immediate: true },
);
</script>

<template>
  <div class="cover" :class="`tone${tone}`">
    <img v-if="src && !failed" :src="src" alt="" loading="lazy" @error="failed = true" />
    <slot />
  </div>
</template>

<style>
.cover {
  position: relative;
  aspect-ratio: 1 / 1;
  width: 100%;
  border-radius: 6px;
  border: 1px solid var(--ln);
  overflow: hidden;
  background: repeating-linear-gradient(135deg, var(--ta) 0 7px, var(--tb) 7px 14px);
}
.cover img {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.cover.thumb {
  width: 40px;
  height: 40px;
  flex: none;
  border-radius: 4px;
  background: repeating-linear-gradient(135deg, var(--ta) 0 5px, var(--tb) 5px 10px);
}
.tone0 { --ta: #2b2125; --tb: #33272c; }
.tone1 { --ta: #232a1e; --tb: #2a3224; }
.tone2 { --ta: #2e2618; --tb: #362d1d; }
.tone3 { --ta: #22232a; --tb: #292a32; }
.tone4 { --ta: #2c221c; --tb: #342821; }
:root[data-theme='light'] .tone0 { --ta: #e7dde0; --tb: #efe6e8; }
:root[data-theme='light'] .tone1 { --ta: #dde5d3; --tb: #e6ecde; }
:root[data-theme='light'] .tone2 { --ta: #ece1cc; --tb: #f2e9d7; }
:root[data-theme='light'] .tone3 { --ta: #dcdde3; --tb: #e5e6eb; }
:root[data-theme='light'] .tone4 { --ta: #e9ddd3; --tb: #f0e6dd; }
@media (prefers-color-scheme: light) {
  :root:not([data-theme='dark']) .tone0 { --ta: #e7dde0; --tb: #efe6e8; }
  :root:not([data-theme='dark']) .tone1 { --ta: #dde5d3; --tb: #e6ecde; }
  :root:not([data-theme='dark']) .tone2 { --ta: #ece1cc; --tb: #f2e9d7; }
  :root:not([data-theme='dark']) .tone3 { --ta: #dcdde3; --tb: #e5e6eb; }
  :root:not([data-theme='dark']) .tone4 { --ta: #e9ddd3; --tb: #f0e6dd; }
}
</style>
