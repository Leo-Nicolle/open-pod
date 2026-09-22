<script lang="ts" setup>
import { computed, reactive, ref } from 'vue';
import { NButton, NSelect, NSwitch, NTag } from 'naive-ui';
import { DEFAULT_PALETTE, PALETTE_META, PRESETS, normalizeHex, type Palette, type PaletteKey } from './palette';
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
const presetName = ref<string>(PRESETS[1].name);
// RGB565 by default: the ILI9341's MADCTL BGR bit already compensates for
// this panel's physically BGR-ordered subpixels in hardware (see
// .agents/screen-red-problem.md), so software should send plain RGB565.
// The BGR565 toggle stays available for a panel wired/configured the other
// way, but it is NOT the default for this hardware - flipping it on is what
// caused a repeated "why does the UI look wrong again" regression here.
const bgr = ref(false);
const zoom = ref(1);
const grid = ref(false);
const playing = ref(true);
const status = ref('');
let statusTimer: ReturnType<typeof setTimeout> | undefined;

const presetOptions = PRESETS.map((p) => ({ label: p.name, value: p.name }));
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

const presetScreens = computed(() =>
  PRESETS.map((p) => ({
    preset: p,
    palette: { ...palette, accent: p.accent, accentDark: p.accentDark, accentAlt: p.accentAlt },
    screen: { kind: 'nowplaying' as const, opts: { mode: 'seek' as const, playing: playing.value, grid: false } },
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
  const v = hexTo565(hex, bgr.value);
  return '0x' + v.toString(16).toUpperCase().padStart(4, '0');
}

function setColor(key: PaletteKey, e: Event) {
  palette[key] = normalizeHex((e.target as HTMLInputElement).value);
}

function setHex(key: PaletteKey, e: Event) {
  palette[key] = normalizeHex((e.target as HTMLInputElement).value);
}

function applyPreset(name: string) {
  const p = PRESETS.find((x) => x.name === name);
  if (!p) return;
  presetName.value = name;
  palette.accent = p.accent;
  palette.accentDark = p.accentDark;
  palette.accentAlt = p.accentAlt;
}

function reset() {
  Object.assign(palette, DEFAULT_PALETTE);
  presetName.value = PRESETS[1].name;
}

function flash(msg: string) {
  status.value = msg;
  if (statusTimer) clearTimeout(statusTimer);
  statusTimer = setTimeout(() => (status.value = ''), 2200);
}

function exportSprites() {
  downloadText('sprites.h', buildSpritesHeader(palette, bgr.value));
  flash('Downloaded sprites.h (' + (bgr.value ? 'BGR565' : 'RGB565') + ')');
}

function exportTheme() {
  downloadText('theme.h', buildThemeHeader(palette, bgr.value));
  flash('Downloaded theme.h');
}

async function exportPngs() {
  await exportAllPngs(palette);
  flash('Downloaded sprite PNGs');
}

async function copySprites() {
  const ok = await copyToClipboard(buildSpritesHeader(palette, bgr.value));
  flash(ok ? 'Copied sprites.h to clipboard' : 'Copy failed');
}

async function copyTheme() {
  const ok = await copyToClipboard(buildThemeHeader(palette, bgr.value));
  flash(ok ? 'Copied theme.h to clipboard' : 'Copy failed');
}

const packingLabel = computed(() => (bgr.value ? 'BGR565 (blue in high bits)' : 'RGB565 (red in high bits)'));
</script>

<template>
  <div class="ub">
    <aside class="ub-controls">
      <section>
        <h3>Preset</h3>
        <n-select v-model:value="presetName" :options="presetOptions" @update:value="applyPreset" />
        <div class="preset-swatches">
          <button
            v-for="p in PRESETS"
            :key="p.name"
            class="preset-dot"
            :title="p.name"
            :style="{ background: p.accent, borderColor: p.accentDark }"
            @click="applyPreset(p.name)"
          />
        </div>
      </section>

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
          <span>Packing</span>
          <n-tag size="small" :type="bgr ? 'success' : 'warning'">{{ packingLabel }}</n-tag>
          <n-switch v-model:value="bgr" />
        </div>
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
        <h3>Presets <span class="muted">(same screen, different accents — click to apply)</span></h3>
        <div class="states">
          <figure
            v-for="s in presetScreens"
            :key="s.preset.name"
            class="state preset-state"
            :class="{ active: s.preset.name === presetName }"
            @click="applyPreset(s.preset.name)"
          >
            <ScreenCanvas :palette="s.palette" :screen="s.screen" />
            <figcaption>
              <span class="mini-swatches">
                <i :style="{ background: s.preset.accent }" />
                <i :style="{ background: s.preset.accentDark }" />
                <i :style="{ background: s.preset.accentAlt }" />
              </span>
              {{ s.preset.name }}
            </figcaption>
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

.preset-swatches {
  display: flex;
  gap: 6px;
  margin-top: 10px;
}

.preset-dot {
  width: 14px;
  height: 14px;
  padding: 0;
  border-radius: 4px;
  border: 2px solid;
  cursor: pointer;
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

.preset-state {
  cursor: pointer;
  padding: 6px;
  margin: -6px;
  border-radius: 8px;
  border: 1px solid transparent;
}

.preset-state.active {
  border-color: var(--vp-c-brand-1);
}

.preset-state figcaption {
  display: flex;
  align-items: center;
  gap: 8px;
}

.mini-swatches {
  display: inline-flex;
  gap: 2px;
}

.mini-swatches i {
  width: 10px;
  height: 10px;
  border-radius: 2px;
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
