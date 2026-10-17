<script lang="ts" setup>
import { computed } from 'vue';
import { PALETTE_META, type Palette, type PaletteKey } from './palette';
import { BOOT, LOGO_ROLES, logoPalette, type LogoRaster, type Wordmark } from './logo';
import BootStage from './BootStage.vue';

const props = defineProps<{
  palette: Palette;
  logo: LogoRaster;
  squash: LogoRaster;
  wordmark: Wordmark | null;
}>();

const logoColors = computed(() => logoPalette(props.palette));
const bootBytes = computed(() => {
  const img = (w: number, h: number) => Math.ceil(w / 2) * h;
  const w = props.wordmark;
  return img(props.logo.w, props.logo.h) + img(props.squash.w, props.squash.h) + (w ? img(w.w, w.h) : 0);
});
const keyframes = [
  { t: 480, label: '0.48s · drop' },
  { t: 740, label: '0.74s · land squash' },
  { t: 1200, label: '1.2s · wordmark types' },
  { t: 1650, label: '1.65s · hop' },
  { t: 2240, label: '2.24s · chunk wipe' },
];
const s = (ms: number) => (ms / 1000).toFixed(2) + 's';
const steps = computed(() => [
  { t: `0–${s(BOOT.DROP_START)}`, d: 'Blank background. Boot init runs here and in the other idle gaps.' },
  {
    t: `${s(BOOT.DROP_START)}–${s(BOOT.DROP_END)}`,
    d: `Bird drops ${BOOT.DROP_PX}px into place, ease-out. Each frame pushes the union of the old and new logo rects.`,
  },
  {
    t: `${s(BOOT.DROP_END)}–${s(BOOT.SQUASH_END)}`,
    d: `Landing squash: one pre-baked ${props.squash.w}×${props.squash.h} frame, bottom-centre anchored, then the normal sprite.`,
  },
  {
    t: `${s(BOOT.LETTER_START)}–${s(BOOT.LETTER_START + 6 * BOOT.LETTER_STEP)}`,
    d: `"opentone" types in, one glyph every ${BOOT.LETTER_STEP}ms: "open" in TEXT, "pod" in ACCENT.`,
  },
  { t: `${s(BOOT.HOP_START)}–${s(BOOT.HOP_END)}`, d: `A single ${BOOT.HOP_PX}px hop.` },
  {
    t: `${s(BOOT.WIPE_AT)}–${s(BOOT.WIPE_AT + 8 * BOOT.CHUNK_MS)}`,
    d: `Chunk wipe into the menu, ${BOOT.CHUNK_MS}ms per chunk. Held on the last frame until boot init is done; a button press skips ahead.`,
  },
]);
const paletteName = (key: PaletteKey) => PALETTE_META.find((m) => m.key === key)?.name ?? key;
</script>

<template>
  <div class="title-details">
    <p class="bytes">logo.h pixel data: <strong>{{ bootBytes }} B</strong></p>

    <div class="frames">
      <figure v-for="k in keyframes" :key="k.t">
        <BootStage :palette="palette" :logo="logo" :squash="squash" :word="wordmark" :zoom="0.5" :t="k.t" />
        <figcaption>{{ k.label }}</figcaption>
      </figure>
    </div>

    <div class="cols">
      <div class="table">
        <div class="table-title">Timeline</div>
        <div v-for="st in steps" :key="st.t" class="step">
          <code>{{ st.t }}</code>
          <span>{{ st.d }}</span>
        </div>
      </div>
      <div class="table">
        <div class="table-title">Logo colours → theme</div>
        <div v-for="(role, i) in LOGO_ROLES" :key="role.svg" class="role">
          <span class="chip" :style="{ background: role.svg }" :title="role.svg" />
          <span class="arrow">→</span>
          <span class="chip" :style="{ background: logoColors[i + 1] }" :title="logoColors[i + 1]" />
          <span class="role-name">{{ role.name }}</span>
          <code>{{ paletteName(role.key) }}</code>
          <span class="role-desc">{{ role.role }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.title-details {
  display: flex;
  flex-direction: column;
  gap: 16px;
}
.bytes {
  margin: 0;
  font-size: 13px;
  color: var(--vp-c-text-2);
}
.frames {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
  gap: 10px;
}
.frames figure {
  margin: 0;
}
.frames figcaption {
  margin-top: 4px;
  font-family: var(--vp-font-family-mono);
  font-size: 11px;
  color: var(--vp-c-text-3);
}
.frames :deep(canvas) {
  width: 100% !important;
}
.cols {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 16px;
  align-items: start;
}
.table {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
  font-size: 12px;
}
.table-title {
  padding: 8px 12px;
  border-bottom: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  font-weight: 600;
  color: var(--vp-c-text-2);
}
.step {
  display: grid;
  grid-template-columns: 92px minmax(0, 1fr);
  gap: 12px;
  padding: 7px 12px;
  border-bottom: 1px solid var(--vp-c-divider);
  line-height: 1.45;
  color: var(--vp-c-text-2);
}
.step:last-child,
.role:last-child {
  border-bottom: none;
}
code {
  padding: 0;
  background: none;
  font-size: 11px;
  color: var(--vp-c-brand-1);
  overflow-wrap: anywhere;
}
.role {
  display: grid;
  grid-template-columns: 16px 10px 16px 52px minmax(0, auto) minmax(0, 1fr);
  align-items: center;
  gap: 8px;
  padding: 4px 12px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-family: var(--vp-font-family-mono);
  font-size: 11px;
}
.chip {
  width: 16px;
  height: 16px;
  border-radius: 4px;
  border: 1px solid var(--vp-c-divider);
}
.arrow {
  color: var(--vp-c-text-3);
  text-align: center;
}
.role-desc {
  font-family: var(--vp-font-family-base);
  color: var(--vp-c-text-3);
}
</style>
