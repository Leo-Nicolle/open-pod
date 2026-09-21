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
import PodScreen from './PodScreen.vue';
import SpriteStage from './SpriteStage.vue';

const palette = reactive<Palette>({ ...DEFAULT_PALETTE });
const presetName = ref<string>(PRESETS[1].name);
const bgr = ref(true); // BGR565 by default (panel is wired BGR)
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
  { key: 'volume' as const, label: 'volume (wheel scrub)' },
  { key: 'seek' as const, label: 'seek (wheel scrubs)' },
];

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
          <span
            v-for="p in PRESETS"
            :key="p.name"
            class="preset-dot"
            :title="p.name"
            :style="{ background: p.accent, borderColor: p.accentDark }"
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
        <h3>Screen states</h3>
        <div class="states">
          <figure v-for="s in states" :key="s.key" class="state">
            <PodScreen :palette="palette" :mode="s.key" :playing="playing" :grid="grid" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Sprites <span class="muted">(6×, pixel grid)</span></h3>
        <div class="sprites">
          <figure v-for="def in SPRITES" :key="def.id" class="sprite">
            <div class="sprite-stage" :style="{ background: palette.bg }">
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
    </main>
  </div>
</template>

<style>
/* Full-bleed the builder past the .vp-doc content column. */
.ub {
  width: min(1280px, calc(100vw - 48px));
  margin-left: auto;
  margin-right: auto;
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
  border-radius: 4px;
  border: 2px solid;
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
  grid-template-columns: repeat(auto-fill, minmax(230px, 1fr));
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
  padding: 10px;
  border-radius: 6px;
  display: inline-block;
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
