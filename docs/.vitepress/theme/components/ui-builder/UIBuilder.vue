<script lang="ts" setup>
import { computed, onMounted, ref, shallowRef, watch } from 'vue';
import { rasterizeBootLogo, rasterizeBootSquash, type Wordmark } from './logo';
import { renderWordmark } from './wordmark';
import { buildSlides } from './screenStates';
import { usePalette } from './usePalette';
import PalettePanel from './PalettePanel.vue';
import ExportButton from './ExportButton.vue';
import ScreenSlideshow from './ScreenSlideshow.vue';
import SpriteSheet from './SpriteSheet.vue';
import TitleScreenDetails from './TitleScreenDetails.vue';
import SpecNotes from './SpecNotes.vue';

type Mode = 'preview' | 'spec';

const store = usePalette();
const { palette } = store;

const mode = ref<Mode>('preview');
const spec = computed(() => mode.value === 'spec');
const zoom = ref(2);
const grid = ref(false);
const playing = ref(true);
const index = ref(0);

const slides = computed(() => buildSlides({ grid: spec.value && grid.value, playing: playing.value }));
const group = computed(() => slides.value[index.value].group);

// The logo frames never depend on the theme; the wordmark needs the web font,
// so it is rendered once on mount (null until then).
const logoRaster = rasterizeBootLogo();
const squashRaster = rasterizeBootSquash();
const wordmark = shallowRef<Wordmark | null>(null);

// The mode lives in the URL hash so a Spec view can be linked.
onMounted(() => {
  if (location.hash === '#spec') mode.value = 'spec';
  renderWordmark()
    .then((w) => (wordmark.value = w))
    .catch((e) => console.error('Wordmark failed', e));
});
watch(mode, (m) => {
  history.replaceState(history.state, '', m === 'spec' ? '#spec' : location.pathname + location.search);
});

const saveLabel = computed(() =>
  store.dirty.value ? 'Unsaved changes' : store.hasSaved.value ? 'Saved in this browser' : 'Default palette',
);
const saveFailed = ref(false);
function onSave() {
  saveFailed.value = !store.save();
}
</script>

<template>
  <div class="ub">
    <div class="ub-bar">
      <div class="seg" role="tablist" aria-label="Mode">
        <button type="button" :class="{ on: mode === 'preview' }" @click="mode = 'preview'">Preview</button>
        <button type="button" :class="{ on: mode === 'spec' }" @click="mode = 'spec'">Spec</button>
      </div>
      <div class="options">
        <label class="opt">
          <input v-model="playing" type="checkbox" />
          Playing
        </label>
        <template v-if="spec">
          <label class="opt">
            <input v-model="grid" type="checkbox" />
            Chunk grid
          </label>
          <label class="opt">
            Zoom
            <select v-model.number="zoom">
              <option :value="1">1×</option>
              <option :value="2">2×</option>
              <option :value="3">3×</option>
            </select>
          </label>
        </template>
      </div>
    </div>

    <div class="ub-body">
      <aside class="ub-side">
        <div class="side-head">
          <h3>Palette</h3>
          <span class="save-state" :class="{ dirty: store.dirty.value }">{{ saveLabel }}</span>
        </div>
        <div class="side-scroll">
          <PalettePanel :store="store" :spec="spec" />
        </div>
        <div class="side-foot">
          <div class="save-row">
            <button type="button" class="btn primary" :disabled="!store.dirty.value" @click="onSave">Save</button>
            <button type="button" class="btn" :disabled="!store.dirty.value" @click="store.revert()">Revert</button>
            <button type="button" class="btn" :disabled="store.isDefault.value" @click="store.reset()">Defaults</button>
          </div>
          <p v-if="saveFailed" class="save-error">Could not save: browser storage is unavailable.</p>
          <ExportButton :palette="palette" :logo="logoRaster" :squash="squashRaster" :wordmark="wordmark" />
        </div>
      </aside>

      <main class="ub-main">
        <ScreenSlideshow
          v-model:index="index"
          :slides="slides"
          :palette="palette"
          :zoom="spec ? zoom : 2"
          :spec="spec"
          :logo="logoRaster"
          :squash="squashRaster"
          :wordmark="wordmark"
        />

        <template v-if="spec">
          <section v-if="group === 'title'" class="block">
            <h2>Title screen</h2>
            <TitleScreenDetails :palette="palette" :logo="logoRaster" :squash="squashRaster" :wordmark="wordmark" />
          </section>
          <section v-else class="block">
            <SpecNotes :group="group" />
          </section>
          <section class="block">
            <h2>Sprites</h2>
            <SpriteSheet :palette="palette" />
          </section>
        </template>
      </main>
    </div>
  </div>
</template>

<style>
/* Full-bleed the builder past the .vp-doc content column, centered on the
   viewport (the page sets `aside: false` so the column itself is centered). */
.ub {
  position: relative;
  left: 50%;
  transform: translateX(-50%);
  width: min(1280px, calc(100vw - 48px));
}
@media (max-width: 640px) {
  .ub {
    width: calc(100vw - 32px);
  }
}
</style>

<style scoped>
.ub {
  font-size: 14px;
  color: var(--vp-c-text-1);
}
.ub :deep(button) {
  cursor: pointer;
}

/* ---------------------------------------------------------------- top bar */
.ub-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 10px 20px;
  margin-bottom: 20px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--vp-c-divider);
}
.seg {
  display: inline-flex;
  padding: 3px;
  border-radius: 10px;
  background: var(--vp-c-default-soft);
}
.seg button {
  padding: 6px 18px;
  border-radius: 7px;
  font-size: 14px;
  font-weight: 500;
  color: var(--vp-c-text-2);
}
.seg button.on {
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.12);
}
.options {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px 18px;
}
.opt {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  color: var(--vp-c-text-2);
  cursor: pointer;
}
.opt select {
  padding: 2px 6px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}

/* ----------------------------------------------------------------- layout */
.ub-body {
  display: grid;
  grid-template-columns: 280px minmax(0, 1fr);
  gap: 32px;
  align-items: start;
}
.ub-side {
  position: sticky;
  top: calc(var(--vp-nav-height, 64px) + 16px);
  display: flex;
  flex-direction: column;
  max-height: calc(100vh - var(--vp-nav-height, 64px) - 32px);
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  background: var(--vp-c-bg-soft);
  overflow: hidden;
}
.side-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
  padding: 14px 16px 10px;
}
.side-head h3 {
  margin: 0;
  font-size: 15px;
  font-weight: 600;
}
.save-state {
  font-size: 11px;
  color: var(--vp-c-text-3);
  white-space: nowrap;
}
.save-state.dirty {
  color: var(--vp-c-warning-1);
}
.save-state.dirty::before {
  content: '● ';
}
.side-scroll {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: 0 16px 12px;
}
.side-foot {
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 12px 16px 14px;
  border-top: 1px solid var(--vp-c-divider);
}
.save-row {
  display: flex;
  gap: 6px;
}
.btn {
  flex: 1;
  padding: 5px 8px;
  border-radius: 7px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
  font-size: 12.5px;
  color: var(--vp-c-text-1);
  transition: border-color 0.2s;
}
.btn:hover:not(:disabled) {
  border-color: var(--vp-c-brand-1);
}
.btn.primary:not(:disabled) {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
  font-weight: 600;
}
.btn:disabled {
  opacity: 0.45;
  cursor: default;
}
.save-error {
  margin: 0;
  font-size: 11px;
  color: var(--vp-c-danger-1);
}

.ub-main {
  min-width: 0;
}
.block {
  margin-top: 40px;
  padding-top: 24px;
  border-top: 1px solid var(--vp-c-divider);
}
.block h2 {
  margin: 0 0 14px;
  padding: 0;
  border: 0;
  font-size: 18px;
  font-weight: 600;
}

@media (max-width: 900px) {
  .ub-body {
    grid-template-columns: minmax(0, 1fr);
  }
  .ub-side {
    position: static;
    max-height: none;
    order: 2;
  }
}
</style>
