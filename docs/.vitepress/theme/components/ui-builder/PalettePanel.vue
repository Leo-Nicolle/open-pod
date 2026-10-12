<script lang="ts" setup>
import { onBeforeUnmount } from 'vue';
import { PALETTE_META, type PaletteKey, type PaletteMeta } from './palette';
import { hexTo565 } from './export';
import type { PaletteStore } from './usePalette';

const props = defineProps<{
  store: PaletteStore;
  /** show macro names, RGB565 values and descriptions */
  spec: boolean;
}>();

const GROUPS: { title: string; keys: PaletteKey[] }[] = [
  { title: 'Surfaces', keys: ['bg', 'surface', 'line', 'separator', 'scrollTrack', 'ribbon'] },
  { title: 'Text', keys: ['text', 'textHi', 'dim', 'muted', 'disabled'] },
  { title: 'Accent', keys: ['accent', 'accentDark', 'accentAlt'] },
  { title: 'Status', keys: ['barOff', 'barOffHi', 'error', 'destructive'] },
];
const metaByKey = new Map(PALETTE_META.map((m) => [m.key, m]));
const groups = GROUPS.map((g) => ({ title: g.title, metas: g.keys.map((k) => metaByKey.get(k)!) as PaletteMeta[] }));

function to565(hex: string): string {
  return '0x' + hexTo565(hex).toString(16).toUpperCase().padStart(4, '0');
}

// A colour-picker drag fires `input` continuously and every palette change
// repaints the screens, so only apply the value once it settles.
const pending = new Map<PaletteKey, ReturnType<typeof setTimeout>>();
function onPick(key: PaletteKey, e: Event) {
  const value = (e.target as HTMLInputElement).value;
  clearTimeout(pending.get(key));
  pending.set(
    key,
    setTimeout(() => props.store.set(key, value), 40),
  );
}
onBeforeUnmount(() => pending.forEach((t) => clearTimeout(t)));

function onHex(key: PaletteKey, e: Event) {
  const input = e.target as HTMLInputElement;
  props.store.set(key, input.value);
  input.value = props.store.palette[key];
}
</script>

<template>
  <div class="palette">
    <div v-for="g in groups" :key="g.title" class="group">
      <div class="group-title">{{ g.title }}</div>
      <div v-for="m in g.metas" :key="m.key" class="row" :class="{ spec }" :title="spec ? undefined : m.description">
        <label class="swatch" :style="{ background: store.palette[m.key] }" :aria-label="`Pick ${m.label}`">
          <input type="color" :value="store.palette[m.key].toLowerCase()" @input="onPick(m.key, $event)" />
        </label>
        <span class="label">{{ m.label }}</span>
        <input
          class="hex"
          :value="store.palette[m.key]"
          spellcheck="false"
          maxlength="7"
          :aria-label="`${m.label} hex`"
          @change="onHex(m.key, $event)"
        />
        <div v-if="spec" class="detail">
          <code>{{ m.name }}</code>
          <code class="c565">{{ to565(store.palette[m.key]) }}</code>
          <span class="desc">{{ m.description }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.palette {
  display: flex;
  flex-direction: column;
  gap: 14px;
}
.group-title {
  margin-bottom: 4px;
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--vp-c-text-3);
}
.row {
  display: grid;
  grid-template-columns: 22px minmax(0, 1fr) auto;
  align-items: center;
  gap: 4px 10px;
  padding: 3px 0;
}
.row.spec {
  padding: 5px 0;
  border-bottom: 1px solid var(--vp-c-divider);
}
.swatch {
  position: relative;
  width: 22px;
  height: 22px;
  border-radius: 6px;
  border: 1px solid var(--vp-c-divider);
  overflow: hidden;
  cursor: pointer;
}
.swatch input[type='color'] {
  position: absolute;
  inset: -4px;
  width: calc(100% + 8px);
  height: calc(100% + 8px);
  opacity: 0;
  cursor: pointer;
}
.label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 13px;
}
.hex {
  width: 8.5ch;
  padding: 2px 5px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
}
.hex:focus {
  border-color: var(--vp-c-brand-1);
  outline: none;
}
.detail {
  grid-column: 2 / -1;
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: 2px 8px;
  min-width: 0;
  font-size: 11px;
  line-height: 1.4;
  color: var(--vp-c-text-3);
}
.detail code {
  padding: 0;
  background: none;
  font-size: 11px;
  color: var(--vp-c-text-2);
  overflow-wrap: anywhere;
}
.detail .c565 {
  color: var(--vp-c-text-3);
}
.desc {
  flex-basis: 100%;
}
</style>
