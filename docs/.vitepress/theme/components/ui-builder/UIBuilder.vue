<script lang="ts" setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue';
import { NButton, NSelect, NSwitch } from 'naive-ui';
import { DEFAULT_PALETTE, PALETTE_META, normalizeHex, type Palette, type PaletteKey } from './palette';
import { SPRITES } from './sprites';
import {
  buildSpritesHeader,
  buildThemeHeader,
  copyToClipboard,
  exportAllPngs,
  hexTo565,
  downloadText,
} from './export';
import ScreenCanvas from './ScreenCanvas.vue';
import SpriteStage from './SpriteStage.vue';
import { KID_A, KID_RESULTS, RADIOHEAD } from './screens';
import { GEOMETRY, REDRAW_NOTES } from './spec';

const palette = reactive<Palette>({ ...DEFAULT_PALETTE });
const STORAGE_KEY = 'openpod-ui-builder-palette';
const zoom = ref(1);
const grid = ref(false);
const playing = ref(true);
const status = ref('');
let statusTimer: ReturnType<typeof setTimeout> | undefined;

const zoomOptions = [
  { label: '1×', value: 1 },
  { label: '2×', value: 2 },
  { label: '3×', value: 3 },
];
const states = [
  { key: 'play' as const, label: 'playing' },
  { key: 'volume' as const, label: 'volume (wheel scrub, 1.5s timeout)' },
  { key: 'seek' as const, label: 'seek (center click → wheel scrubs)' },
];

const nowPlaying = computed(() =>
  states.map((s) => ({
    label: s.label,
    screen: { kind: 'nowplaying' as const, opts: { mode: s.key, playing: playing.value, grid: grid.value } },
  })),
);

const trackLists = computed(() => [
  {
    label: 'top of list',
    screen: {
      kind: 'tracklist' as const,
      opts: { heading: 'Kid A', tracks: KID_A, first: 0, selected: 2, grid: grid.value },
    },
  },
  {
    label: 'artist menu, scrolled, last row selected',
    screen: {
      kind: 'tracklist' as const,
      opts: { heading: 'Radiohead', tracks: RADIOHEAD, first: 5, selected: 6, grid: grid.value },
    },
  },
]);

const searches = computed(() => {
  const base = { query: 'kid', letter: 'K', results: KID_RESULTS, total: 24, selected: 0, first: 0, grid: grid.value };
  return [
    { label: 'typing — ribbon + 6 results', screen: { kind: 'search' as const, opts: { ...base, mode: 'typing' as const } } },
    {
      label: 'browsing results — 7 rows',
      screen: { kind: 'search' as const, opts: { ...base, mode: 'browsing' as const, selected: 2 } },
    },
    {
      label: 'no matches',
      screen: {
        kind: 'search' as const,
        opts: { ...base, mode: 'typing' as const, query: 'kidz', letter: 'Z', results: [], total: 0 },
      },
    },
  ];
});

function to565(hex: string): string {
  const v = hexTo565(hex);
  return '0x' + v.toString(16).toUpperCase().padStart(4, '0');
}

/** Trailing-edge debounce; `cancel` drops a pending call. */
function debounce<A extends unknown[]>(fn: (...args: A) => void, ms: number) {
  let t: ReturnType<typeof setTimeout> | undefined;
  const run = (...args: A) => {
    if (t) clearTimeout(t);
    t = setTimeout(() => {
      t = undefined;
      fn(...args);
    }, ms);
  };
  run.cancel = () => t && clearTimeout(t);
  return run;
}

// A color-picker drag fires `input` continuously and every palette change
// repaints all screens and sprites, so only apply the value once it settles.
const colorInputs = new Map<PaletteKey, ReturnType<typeof debounce<[string]>>>();

function setColor(key: PaletteKey, e: Event) {
  const value = (e.target as HTMLInputElement).value;
  let apply = colorInputs.get(key);
  if (!apply) {
    apply = debounce((v: string) => (palette[key] = normalizeHex(v)), 40);
    colorInputs.set(key, apply);
  }
  apply(value);
}

function setHex(key: PaletteKey, e: Event) {
  palette[key] = normalizeHex((e.target as HTMLInputElement).value);
}

function reset() {
  Object.assign(palette, DEFAULT_PALETTE);
}

// Remember the last palette per browser. Storage can be unavailable (private
// mode, blocked site data), so every access is guarded. Loaded on mount rather
// than at setup so SSR and the first client render agree.
function loadPalette() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null') as Partial<Palette> | null;
    if (!saved) return;
    for (const { key } of PALETTE_META) {
      const v = saved[key];
      if (typeof v === 'string') palette[key] = normalizeHex(v);
    }
  } catch {
    // ignore unreadable or corrupt storage
  }
}

onMounted(() => {
  loadPalette();
  watch(palette, savePalette, { deep: true });
});

function persistPalette() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(palette));
  } catch {
    // storage unavailable
  }
}

const savePalette = debounce(persistPalette, 300);

onBeforeUnmount(() => {
  colorInputs.forEach((apply) => apply.cancel());
  // flush instead of dropping an edit made just before leaving the page
  savePalette.cancel();
  persistPalette();
});

function flash(msg: string) {
  status.value = msg;
  if (statusTimer) clearTimeout(statusTimer);
  statusTimer = setTimeout(() => (status.value = ''), 2200);
}

function exportSprites() {
  downloadText('sprites.h', buildSpritesHeader(palette));
  flash('Downloaded sprites.h');
}

function exportTheme() {
  downloadText('theme.h', buildThemeHeader(palette));
  flash('Downloaded theme.h');
}

async function exportPngs() {
  await exportAllPngs(palette);
  flash('Downloaded sprite PNGs');
}

async function copySprites() {
  const ok = await copyToClipboard(buildSpritesHeader(palette));
  flash(ok ? 'Copied sprites.h to clipboard' : 'Copy failed');
}

async function copyTheme() {
  const ok = await copyToClipboard(buildThemeHeader(palette));
  flash(ok ? 'Copied theme.h to clipboard' : 'Copy failed');
}
</script>

<template>
  <div class="ub">
    <aside class="ub-controls">
      <section>
        <h3>Palette</h3>
        <div class="palette-list">
          <div v-for="meta in PALETTE_META" :key="meta.key" class="palette-row">
            <label class="swatch" :style="{ background: palette[meta.key] }">
              <input type="color" :value="palette[meta.key]" @input="setColor(meta.key, $event)" />
            </label>
            <div class="palette-info">
              <div class="palette-line">
                <span class="palette-name">{{ meta.name }}</span>
                <input
                  class="palette-hex"
                  :value="palette[meta.key]"
                  spellcheck="false"
                  @change="setHex(meta.key, $event)"
                />
                <span class="palette-565">{{ to565(palette[meta.key]) }}</span>
              </div>
              <div class="palette-desc">{{ meta.description }}</div>
            </div>
          </div>
        </div>
        <n-button size="tiny" quaternary @click="reset">Reset to defaults</n-button>
      </section>

      <section>
        <h3>Options</h3>
        <div class="option-row">
          <span>Screen zoom</span>
          <n-select v-model:value="zoom" :options="zoomOptions" style="width: 90px" />
        </div>
        <div class="option-row">
          <span>Chunk grid</span>
          <n-switch v-model:value="grid" />
        </div>
        <div class="option-row">
          <span>Playing</span>
          <n-switch v-model:value="playing" />
        </div>
      </section>

      <section>
        <h3>Export</h3>
        <div class="export-buttons">
          <n-button type="primary" @click="exportSprites">Download sprites.h</n-button>
          <n-button @click="exportTheme">Download theme.h</n-button>
          <n-button @click="copySprites">Copy sprites.h</n-button>
          <n-button @click="copyTheme">Copy theme.h</n-button>
          <n-button @click="exportPngs">Download PNG previews</n-button>
        </div>
        <div v-if="status" class="status">{{ status }}</div>
      </section>
    </aside>

    <main class="ub-preview">
      <section>
        <h3>Now playing</h3>
        <p class="lede">
          160px cover on the left, metadata column on the right, and every changing element confined
          to a single 30px chunk at the bottom of the screen, so a progress tick or a volume change
          costs one 320×30 buffer push instead of a full body redraw.
        </p>
        <div class="states">
          <figure v-for="s in nowPlaying" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Track list</h3>
        <p class="lede">
          Seven rows of exactly 30px fill y=30…240, so every row is one CHUNK_HEIGHT buffer on a chunk
          boundary. Moving the highlight redraws two rows, not the screen; scrolling redraws all seven
          but reuses one buffer seven times.
        </p>
        <div class="states">
          <figure v-for="s in trackLists" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Search</h3>
        <p class="lede">
          The query lives in the header, and the letter ribbon under it is the wheel's input surface:
          scrolling moves the cursor through the alphabet, a click appends. While typing, the ribbon
          occupies chunk 1 and six results fit below; clicking into the list drops the ribbon and a
          seventh row takes its place.
        </p>
        <div class="states">
          <figure v-for="s in searches" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Sprites <span class="muted">(1× and 6×, pixel grid)</span></h3>
        <div class="sprites">
          <figure v-for="def in SPRITES" :key="def.id" class="sprite">
            <div class="sprite-stage" :style="{ background: palette.bg }">
              <SpriteStage :def="def" :palette="palette" :scale="1" />
              <SpriteStage :def="def" :palette="palette" :scale="6" :grid="true" />
            </div>
            <figcaption>
              <strong>{{ def.id }}</strong>
              <span class="muted">{{ def.w }}×{{ def.h }}</span>
            </figcaption>
            <p class="sprite-note">{{ def.note }}</p>
          </figure>
        </div>
      </section>

      <section>
        <h3>Geometry</h3>
        <div class="geometry">
          <div v-for="g in GEOMETRY" :key="g.el + g.box" class="geometry-row">
            <span>{{ g.el }}</span>
            <span class="geometry-box">{{ g.box }}</span>
            <span class="muted">{{ g.note }}</span>
          </div>
        </div>
      </section>

      <section>
        <h3>Redraw budget</h3>
        <ol class="notes">
          <li v-for="n in REDRAW_NOTES" :key="n.i">
            <span class="note-i">{{ n.i }}</span>
            <span>{{ n.t }}</span>
          </li>
        </ol>
      </section>
    </main>
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
</style>

<style scoped>
.ub {
  display: grid;
  grid-template-columns: 320px minmax(0, 1fr);
  gap: 32px;
  align-items: start;
  font-size: 14px;
  color: var(--vp-c-text-1);
}

h3 {
  margin: 0 0 12px;
  font-size: 12px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--vp-c-text-2);
  font-weight: 600;
}

section {
  margin-bottom: 28px;
}

.ub-controls {
  position: sticky;
  top: 24px;
  display: flex;
  flex-direction: column;
  gap: 4px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  padding: 20px;
}

.palette-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.palette-row {
  display: flex;
  gap: 10px;
  align-items: flex-start;
}

.swatch {
  flex: none;
  width: 38px;
  height: 38px;
  border-radius: 7px;
  border: 1px solid var(--vp-c-divider);
  cursor: pointer;
  position: relative;
  overflow: hidden;
}

.swatch input[type='color'] {
  position: absolute;
  inset: -4px;
  width: calc(100% + 8px);
  height: calc(100% + 8px);
  opacity: 0;
  cursor: pointer;
}

.palette-info {
  flex: 1;
  min-width: 0;
}

.palette-line {
  display: flex;
  align-items: center;
  gap: 6px;
}

.palette-name {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
  font-weight: 600;
  white-space: nowrap;
}

.palette-hex {
  width: 76px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
  padding: 2px 4px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}

.palette-565 {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  color: var(--vp-c-text-3);
}

.palette-desc {
  font-size: 11px;
  color: var(--vp-c-text-3);
  line-height: 1.35;
  margin-top: 1px;
}

.option-row {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 6px 0;
}

.option-row > span {
  flex: 1;
  font-size: 13px;
}

.export-buttons {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.status {
  margin-top: 10px;
  font-size: 12px;
  color: var(--vp-c-brand-1);
}

.ub-preview {
  min-width: 0;
}

.states {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}

.state {
  margin: 0;
}

.state figcaption {
  margin-top: 6px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  color: var(--vp-c-text-2);
}

.sprites {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: 16px;
}

.sprite {
  margin: 0;
  padding: 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}

.sprite-stage {
  max-width: 100%;
  box-sizing: border-box;
  overflow-x: auto;
  padding: 10px;
  border-radius: 6px;
  display: inline-flex;
  align-items: flex-end;
  gap: 16px;
}

.lede {
  margin: -4px 0 16px;
  max-width: 760px;
  font-size: 13px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}

.geometry {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
  max-width: 900px;
}

.geometry-row {
  display: grid;
  grid-template-columns: 1.1fr 1fr 1.3fr;
  gap: 16px;
  padding: 8px 14px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  line-height: 16px;
}

.geometry-row:last-child {
  border-bottom: none;
}

.geometry-box {
  color: var(--vp-c-brand-1);
}

.notes {
  list-style: none;
  margin: 0;
  padding: 0;
  max-width: 820px;
}

.notes li {
  display: grid;
  grid-template-columns: 26px 1fr;
  gap: 12px;
  margin: 0 0 10px;
  font-size: 13px;
  line-height: 20px;
  color: var(--vp-c-text-2);
}

.note-i {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  color: var(--vp-c-text-3);
}

.sprite figcaption {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-top: 10px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
}

.sprite-note {
  margin: 6px 0 0;
  font-size: 11px;
  line-height: 1.45;
  color: var(--vp-c-text-3);
}

.muted {
  color: var(--vp-c-text-3);
  font-weight: 400;
}

@media (max-width: 900px) {
  .ub {
    grid-template-columns: 1fr;
  }
  .ub-controls {
    position: static;
  }
}
</style>
