<script setup lang="ts">
// One show on "On the card › Podcasts": its keep setting and its episodes,
// with what the next sync does to each.
import { computed } from 'vue';
import Cover from './Cover.vue';
import type { CardShow } from '../api';
import { albumTime, bytes, clock, epDay, plural } from '../format';
import { canCheck, check, setPodcastKeep, setPodcastPlayed, store } from '../store';
import { episodeRows, keepLabel, keepStep } from './podcasts';

const props = defineProps<{ show: CardShow }>();

const rows = computed(() => episodeRows(props.show, store.podSource));
const avg = (f: (e: CardShow['episodes'][number]) => number) =>
  props.show.episodes.reduce((n, e) => n + f(e), 0) / Math.max(1, props.show.episodes.length);
const meta = computed(() => {
  const s = props.show;
  const parts = ['folder', `${plural(s.episodes.length, 'episode')} on the card`];
  if (s.episodes.length) parts.push(`about ${albumTime(avg((e) => e.duration))}`, `${bytes(avg((e) => e.bytes))} each`);
  return parts.join(' · ');
});
const onPlayer = computed(() => ['Podcasts', ...props.show.key.split('/').filter(Boolean)].join(' › '));
const where = computed(() => {
  const s = props.show;
  if (s.source) return s.source;
  return s.machines.length ? `Mirrored from ${s.machines.join(', ')}` : 'Already on the card';
});
// Other years need the year: a wider date column.
const dateCol = computed(() => (rows.value.some((r) => epDay(r.date).length > 6) ? '96px' : '60px'));
const note = computed(() => {
  const k = props.show.keep;
  return k
    ? `Episodes older than the latest ${k} leave the card at the next sync. They stay on this computer.`
    : 'Every episode in the folder goes on the card. Keep fewer to save space: older ones leave the card at the next sync, and stay on this computer.';
});

const step = (dir: 1 | -1) => setPodcastKeep(props.show.key, keepStep(props.show.keep, dir));
</script>

<template>
  <button class="link" style="margin-top: -6px; align-self: flex-start; font-size: 14px" @click="store.podcastShow = null">
    ← Podcasts
  </button>
  <div style="display: flex; gap: 28px; flex-wrap: wrap; align-items: flex-start">
    <Cover :path="show.cover" style="width: 180px; flex: none" />
    <div style="flex: 1; min-width: 280px; display: flex; flex-direction: column; gap: 14px">
      <div>
        <div style="font-size: 28px; font-weight: 600; letter-spacing: -0.02em">{{ show.title }}</div>
        <div v-if="show.publisher" class="mu" style="font-size: 17px; margin-top: 2px">{{ show.publisher }}</div>
      </div>
      <div class="mono mu" style="font-size: 13px">{{ meta }}</div>
      <div class="kv">
        <span class="mu">Keep on the card</span>
        <div style="display: flex; align-items: center; gap: 10px">
          <div class="stepper">
            <button :disabled="show.keep === 1" aria-label="Keep fewer" @click="step(-1)">−</button>
            <span>{{ keepLabel(show.keep) }}</span>
            <button :disabled="show.keep === null" aria-label="Keep more" @click="step(1)">+</button>
          </div>
          <span class="mu" style="font-size: 13px">episodes</span>
        </div>
        <span class="mu">On the player</span><span>{{ onPlayer }}</span>
        <span class="mu">Played status</span><span>Read from the card at every sync</span>
        <span class="mu">Folder</span
        ><span :class="{ mono: !!show.source }" style="font-size: 13px; overflow-wrap: anywhere">{{ where }}</span>
      </div>
      <div style="display: flex; gap: 10px; flex-wrap: wrap">
        <button class="btn sm" :disabled="!canCheck" @click="check">Check for new episodes</button>
      </div>
    </div>
  </div>
  <div style="display: flex; flex-direction: column; border-top: 1px solid var(--ln)">
    <div v-for="(r, i) in rows" :key="i" class="ep" :style="{ gridTemplateColumns: `${dateCol} minmax(0, 1fr) 64px 150px 112px` }">
      <span class="mono mu" style="font-size: 13px; white-space: nowrap">{{ epDay(r.date) || '—' }}</span>
      <span class="ellipsis" :title="r.title">{{ r.title }}</span>
      <span class="mono" style="font-size: 13px; text-align: right">{{ r.duration ? clock(r.duration) : '' }}</span>
      <div style="display: flex; justify-content: flex-end">
        <span v-if="r.status === 'next'" class="badge new" style="white-space: nowrap">new · next sync</span>
        <div v-else-if="r.status === 'progress'" style="display: flex; flex-direction: column; align-items: flex-end; gap: 4px">
          <span class="mono" style="font-size: 12px">{{ albumTime(r.left) }} left</span>
          <div class="minibar"><div :style="{ width: `${Math.round(r.done * 100)}%` }"></div></div>
        </div>
        <span v-else-if="r.status === 'played'" class="badge played">played</span>
        <span v-else-if="r.status === 'on'" class="mono mu" style="font-size: 12px">on the card</span>
        <span v-else-if="r.status === 'leave'" class="badge leave" style="white-space: nowrap">leaves the card</span>
        <span v-else class="mono mu" style="font-size: 12px; white-space: nowrap">on this computer</span>
      </div>
      <div style="display: flex; justify-content: flex-end">
        <button v-if="r.path" class="btn xxs" style="white-space: nowrap" @click="setPodcastPlayed(r.path, !r.played)">
          {{ r.played ? 'Mark unplayed' : 'Mark played' }}
        </button>
      </div>
    </div>
  </div>
  <div class="mu" style="font-size: 13px; line-height: 1.5; text-wrap: pretty">
    {{ note }}
    <template v-if="!store.podSource && store.settings?.podcasts">
      Check what's new to also list the episodes in your folder that aren't on the card.
    </template>
  </div>
</template>

<style scoped>
.kv {
  display: grid;
  grid-template-columns: max-content minmax(0, 1fr);
  gap: 10px 18px;
  font-size: 14px;
  border-top: 1px solid var(--ln);
  padding-top: 14px;
  align-items: center;
}
.stepper {
  display: flex;
  align-items: center;
  border: 1px solid var(--ln2);
  border-radius: 8px;
  overflow: hidden;
  height: 32px;
}
.stepper button {
  width: 32px;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  font-family: 'IBM Plex Mono', ui-monospace, monospace;
  font-size: 14px;
  background: none;
  border: none;
  color: var(--tx);
  user-select: none;
}
.stepper button:hover:not(:disabled) {
  background: var(--sf2);
}
.stepper button:disabled {
  color: var(--mu);
  cursor: not-allowed;
}
.stepper span {
  padding: 0 12px;
  height: 100%;
  display: flex;
  align-items: center;
  font-size: 14px;
  font-weight: 500;
  border-left: 1px solid var(--ln2);
  border-right: 1px solid var(--ln2);
  white-space: nowrap;
  min-width: 58px;
  justify-content: center;
}
.ep {
  display: grid;
  gap: 12px;
  align-items: center;
  padding: 10px 4px;
  border-bottom: 1px solid var(--ln);
  font-size: 14px;
  min-height: 24px;
}
.minibar {
  width: 84px;
  height: 3px;
  border-radius: 2px;
  background: var(--sf2);
}
.minibar > div {
  height: 100%;
  border-radius: 2px;
  background: var(--mag);
}
</style>
