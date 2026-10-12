<script lang="ts" setup>
import { computed, onBeforeUnmount, onMounted } from 'vue';
import type { Palette } from './palette';
import type { LogoRaster, Wordmark } from './logo';
import { GROUPS, type Slide } from './screenStates';
import ScreenCanvas from './ScreenCanvas.vue';
import BootStage from './BootStage.vue';

const props = defineProps<{
  slides: Slide[];
  palette: Palette;
  zoom: number;
  spec: boolean;
  logo: LogoRaster;
  squash: LogoRaster;
  wordmark: Wordmark | null;
}>();
const index = defineModel<number>('index', { required: true });

const slide = computed(() => props.slides[index.value]);
const group = computed(() => GROUPS.find((g) => g.id === slide.value.group)!);
const groupSlides = computed(() =>
  props.slides.map((s, i) => ({ s, i })).filter(({ s }) => s.group === slide.value.group),
);
const posInGroup = computed(() => groupSlides.value.findIndex(({ i }) => i === index.value) + 1);

function go(i: number) {
  const n = props.slides.length;
  index.value = (i + n) % n;
}
function goGroup(id: string) {
  const i = props.slides.findIndex((s) => s.group === id);
  if (i >= 0) index.value = i;
}

function onKey(e: KeyboardEvent) {
  const t = e.target as HTMLElement | null;
  if (t && (t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName))) return;
  if (e.key === 'ArrowRight') go(index.value + 1);
  else if (e.key === 'ArrowLeft') go(index.value - 1);
  else return;
  e.preventDefault();
}
onMounted(() => window.addEventListener('keydown', onKey));
onBeforeUnmount(() => window.removeEventListener('keydown', onKey));
</script>

<template>
  <div class="slideshow">
    <nav class="flows" aria-label="Screens">
      <button
        v-for="g in GROUPS"
        :key="g.id"
        type="button"
        class="flow"
        :class="{ on: g.id === slide.group }"
        @click="goGroup(g.id)"
      >
        {{ g.title }}
      </button>
    </nav>

    <div class="stage">
      <button type="button" class="nav prev" aria-label="Previous screen" @click="go(index - 1)">‹</button>
      <div class="frame">
        <BootStage
          v-if="!slide.screen"
          :palette="palette"
          :logo="logo"
          :squash="squash"
          :word="wordmark"
          :zoom="zoom"
          :playing="true"
        />
        <ScreenCanvas v-else :palette="palette" :screen="slide.screen" :zoom="zoom" />
      </div>
      <button type="button" class="nav next" aria-label="Next screen" @click="go(index + 1)">›</button>
    </div>

    <div class="caption">
      <span class="count">{{ posInGroup }} / {{ groupSlides.length }}</span>
      <strong>{{ slide.label }}</strong>
      <span v-if="spec && slide.spec" class="spec">{{ slide.spec }}</span>
    </div>

    <div v-if="groupSlides.length > 1" class="thumbs">
      <button
        v-for="{ s, i } in groupSlides"
        :key="s.id"
        type="button"
        class="thumb"
        :class="{ on: i === index }"
        :title="s.label"
        @click="index = i"
      >
        <ScreenCanvas v-if="s.screen" :palette="palette" :screen="s.screen" :zoom="0.375" />
      </button>
    </div>

    <p class="summary">{{ spec ? group.lede : group.summary }}</p>
  </div>
</template>

<style scoped>
.slideshow {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 14px;
}
.flows {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 6px;
}
.flow {
  padding: 5px 14px;
  border-radius: 999px;
  border: 1px solid var(--vp-c-divider);
  font-size: 13px;
  color: var(--vp-c-text-2);
  transition:
    color 0.2s,
    border-color 0.2s,
    background 0.2s;
}
.flow:hover {
  color: var(--vp-c-text-1);
}
.flow.on {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-weight: 600;
}
.stage {
  display: flex;
  align-items: center;
  gap: 10px;
  max-width: 100%;
}
.frame {
  min-width: 0;
  padding: 10px;
  border-radius: 14px;
  background: #0b0a09;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.25);
}
.nav {
  flex: none;
  width: 36px;
  height: 36px;
  border-radius: 50%;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-size: 22px;
  line-height: 1;
  transition:
    color 0.2s,
    border-color 0.2s;
}
.nav:hover {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
}
.caption {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  align-items: baseline;
  gap: 4px 10px;
  font-size: 14px;
  text-align: center;
}
.count {
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
  color: var(--vp-c-text-3);
}
.spec {
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.thumbs {
  display: flex;
  gap: 8px;
  max-width: 100%;
  overflow-x: auto;
  padding: 4px;
}
.thumb {
  flex: none;
  padding: 3px;
  border-radius: 6px;
  border: 2px solid transparent;
  opacity: 0.6;
  transition:
    opacity 0.2s,
    border-color 0.2s;
}
.thumb:hover {
  opacity: 1;
}
.thumb.on {
  opacity: 1;
  border-color: var(--vp-c-brand-1);
}
.summary {
  max-width: 640px;
  margin: 0;
  font-size: 14px;
  line-height: 1.6;
  text-align: center;
  color: var(--vp-c-text-2);
}
@media (max-width: 560px) {
  .nav {
    display: none;
  }
  .frame {
    padding: 6px;
  }
}
</style>
