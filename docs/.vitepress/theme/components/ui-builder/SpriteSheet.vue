<script lang="ts" setup>
import type { Palette } from './palette';
import { PALETTE_META } from './palette';
import { findSprite, type SpriteDef } from './sprites';
import SpriteStage from './SpriteStage.vue';

defineProps<{ palette: Palette }>();

const GROUPS: { title: string; ids: string[] }[] = [
  { title: 'Glyphs', ids: ['play', 'pause', 'speaker', 'bt', 'chevron_r', 'tick'] },
  { title: 'Bars and handles', ids: ['bar6', 'bar6_slice', 'bar10', 'handle'] },
  { title: 'Status', ids: ['battery'] },
];
const groups = GROUPS.map((g) => ({
  title: g.title,
  sprites: g.ids.map((id) => findSprite(id)).filter((d): d is SpriteDef => !!d),
}));

const macro = (key: string) => PALETTE_META.find((m) => m.key === key)?.name ?? key;
const badge = (d: SpriteDef) => (d.kind === 'glyph' ? `mask · ${macro(d.tint)}` : 'RGB565 pixels');
/** largest integer zoom that keeps the preview within ~120px */
const zoomFor = (d: SpriteDef) => Math.max(2, Math.min(8, Math.floor(120 / Math.max(d.w, d.h))));
</script>

<template>
  <div class="sheet">
    <div v-for="g in groups" :key="g.title" class="group">
      <h4>{{ g.title }}</h4>
      <div class="cards">
        <figure v-for="d in g.sprites" :key="d.id" class="card">
          <div class="stage" :style="{ background: palette.bg }">
            <SpriteStage :def="d" :palette="palette" :scale="zoomFor(d)" :grid="true" />
            <SpriteStage :def="d" :palette="palette" :scale="1" class="actual" />
          </div>
          <figcaption>
            <div class="title">
              <code>{{ d.id }}</code>
              <span class="size">{{ d.w }}×{{ d.h }}</span>
            </div>
            <span class="badge" :class="d.kind">{{ badge(d) }}</span>
            <p class="note">{{ d.note }}</p>
          </figcaption>
        </figure>
      </div>
    </div>
  </div>
</template>

<style scoped>
.sheet {
  display: flex;
  flex-direction: column;
  gap: 20px;
}
h4 {
  margin: 0 0 10px;
  font-size: 13px;
  font-weight: 600;
  color: var(--vp-c-text-2);
}
.cards {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 12px;
}
.card {
  display: flex;
  flex-direction: column;
  margin: 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  overflow: hidden;
  background: var(--vp-c-bg-soft);
}
.stage {
  display: flex;
  align-items: flex-end;
  justify-content: center;
  gap: 14px;
  height: 140px;
  padding: 10px;
  overflow: hidden;
}
.stage .actual {
  align-self: flex-end;
}
figcaption {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding: 10px 12px 12px;
}
.title {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 8px;
}
.title code {
  padding: 0;
  background: none;
  font-size: 13px;
  font-weight: 600;
  color: var(--vp-c-text-1);
}
.size {
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
  color: var(--vp-c-text-3);
}
.badge {
  align-self: flex-start;
  padding: 1px 8px;
  border-radius: 999px;
  font-family: var(--vp-font-family-mono);
  font-size: 10.5px;
  background: var(--vp-c-default-soft);
  color: var(--vp-c-text-2);
}
.badge.glyph {
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
}
.note {
  margin: 0;
  font-size: 12px;
  line-height: 1.45;
  color: var(--vp-c-text-3);
}
</style>
