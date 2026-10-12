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
.tone0 { --ta: #26222a; --tb: #2c2731; }
.tone1 { --ta: #212722; --tb: #262d27; }
.tone2 { --ta: #232428; --tb: #292a2f; }
.tone3 { --ta: #21252b; --tb: #272b32; }
.tone4 { --ta: #29212a; --tb: #302631; }
:root[data-theme='light'] .tone0 { --ta: #f1eaf0; --tb: #f6f0f5; }
:root[data-theme='light'] .tone1 { --ta: #ebf2e8; --tb: #f1f6ee; }
:root[data-theme='light'] .tone2 { --ta: #efeff2; --tb: #f5f5f7; }
:root[data-theme='light'] .tone3 { --ta: #ebeef3; --tb: #f1f3f7; }
:root[data-theme='light'] .tone4 { --ta: #f3ebf1; --tb: #f8f1f6; }
@media (prefers-color-scheme: light) {
  :root:not([data-theme='dark']) .tone0 { --ta: #f1eaf0; --tb: #f6f0f5; }
  :root:not([data-theme='dark']) .tone1 { --ta: #ebf2e8; --tb: #f1f6ee; }
  :root:not([data-theme='dark']) .tone2 { --ta: #efeff2; --tb: #f5f5f7; }
  :root:not([data-theme='dark']) .tone3 { --ta: #ebeef3; --tb: #f1f3f7; }
  :root:not([data-theme='dark']) .tone4 { --ta: #f3ebf1; --tb: #f8f1f6; }
}
</style>
