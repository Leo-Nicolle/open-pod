<script setup lang="ts">
// "On the card › Podcasts": the card's shows, or how to get some there.
import { computed } from 'vue';
import Cover from './Cover.vue';
import type { CardShow } from '../api';
import { num, plural } from '../format';
import { canCheck, check, choosePodcasts, setPodcastKeep, store } from '../store';
import { KEEP_MAX, keepLabel, playedCounts } from './podcasts';

const pods = computed(() => store.podcasts);
const folder = computed(() => store.settings?.podcasts ?? null);

const readLine = computed(() => {
  const p = pods.value;
  if (!p?.stateRead || !(p.played || p.started)) return null;
  return { played: plural(p.played, 'episode'), started: p.started ? ` and ${num(p.started)} half-played` : '' };
});

function sub(s: CardShow) {
  const { played, unplayed } = playedCounts(s);
  const state = played && !unplayed ? 'all played' : `${num(unplayed)} unplayed`;
  return `${s.publisher || 'Podcasts folder'} · ${state}`;
}
function playedLine(s: CardShow) {
  const { played, unplayed } = playedCounts(s);
  return !played ? 'none played' : !unplayed ? 'all played' : `${num(played)} played`;
}
function where(s: CardShow) {
  if (s.source) return s.source;
  return s.machines.length ? `from ${s.machines.join(', ')}` : 'already on the card';
}
const setKeep = (s: CardShow, v: string) => setPodcastKeep(s.key, Number(v) || null);
</script>

<template>
  <div v-if="!pods" class="mu" style="padding: 40px 0; text-align: center">Loading…</div>

  <div v-else-if="!pods.shows.length" class="empty">
    <div class="ring" style="width: 56px; height: 56px; border: 1.5px dashed var(--ln2)"></div>
    <template v-if="!folder">
      <div style="font-size: 24px; font-weight: 600; letter-spacing: -0.01em">No podcasts yet</div>
      <p class="mu blurb">
        Choose the folder where you keep podcast episodes. Each folder of episodes becomes a show in the player's
        Podcasts menu, separate from your albums.
      </p>
      <div style="display: flex; gap: 10px; margin-top: 6px">
        <button class="btn primary" @click="choosePodcasts">Choose podcasts folder…</button>
      </div>
    </template>
    <template v-else>
      <div style="font-size: 24px; font-weight: 600; letter-spacing: -0.01em">No podcasts on the card yet</div>
      <p class="mu blurb">
        Your podcasts folder is <span class="mono">{{ folder.path }}</span>. Check what's new to copy its episodes to
        the card.
      </p>
      <div style="display: flex; gap: 10px; margin-top: 6px">
        <button class="btn primary" :disabled="!canCheck" @click="check">Check what's new</button>
      </div>
    </template>
    <div class="mu" style="font-size: 13px; margin-top: 4px">
      Each show keeps all its episodes on the card unless you change it.
    </div>
  </div>

  <template v-else>
    <div v-if="readLine" class="reassure">
      <span class="ring" style="width: 12px; height: 12px; border: 2px solid var(--grn)"></span>
      <span
        >Read from the card just now: <b class="mono" style="font-weight: 500">{{ readLine.played }}</b> finished{{
          readLine.started
        }}.</span
      >
    </div>
    <div style="display: flex; flex-direction: column; border-top: 1px solid var(--ln)">
      <div class="row head">
        <span></span><span>Show</span><span>Source</span><span>On the card</span><span>Keep</span>
      </div>
      <div
        v-for="s in pods.shows"
        :key="s.key"
        class="row show"
        tabindex="0"
        @click="store.podcastShow = s.key"
        @keydown.enter="store.podcastShow = s.key"
      >
        <Cover class="thumb" :path="s.cover" />
        <div style="min-width: 0">
          <div class="ellipsis" style="font-weight: 600; font-size: 14px">{{ s.title }}</div>
          <div class="ellipsis mu" style="font-size: 13px">{{ sub(s) }}</div>
        </div>
        <div style="min-width: 0; display: flex; align-items: center; gap: 8px">
          <span class="badge source" style="flex: none">folder</span>
          <span class="mono mu ellipsis" style="font-size: 12px; min-width: 0" :title="where(s)">{{ where(s) }}</span>
        </div>
        <div>
          <div class="mono" style="font-size: 14px">{{ plural(s.episodes.length, 'episode') }}</div>
          <div class="mu" style="font-size: 12px">{{ playedLine(s) }}</div>
        </div>
        <div @click.stop>
          <label class="keep">
            <select
              :value="s.keep ?? 0"
              :aria-label="`Episodes of ${s.title} kept on the card`"
              @change="setKeep(s, ($event.target as HTMLSelectElement).value)"
            >
              <option :value="0">All</option>
              <option v-for="n in KEEP_MAX" :key="n" :value="n">Latest {{ n }}</option>
              <option v-if="s.keep && s.keep > KEEP_MAX" :value="s.keep">{{ keepLabel(s.keep) }}</option>
            </select>
            <span class="mu" style="font-size: 10px; pointer-events: none">▾</span>
          </label>
        </div>
      </div>
    </div>
    <div class="mu" style="font-size: 13px; line-height: 1.5">
      On the player, shows are under <b style="color: var(--tx); font-weight: 500">Podcasts</b>, separate from your
      albums. Files go in <span class="mono">/Podcasts/</span> on the card, laid out like your podcasts folder.
    </div>
  </template>
</template>

<style scoped>
.empty {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  gap: 12px;
  padding: 56px 20px;
}
.blurb {
  margin: 0;
  font-size: 15px;
  line-height: 1.5;
  max-width: 460px;
  text-wrap: pretty;
  overflow-wrap: anywhere;
}
.row {
  display: grid;
  grid-template-columns: 40px minmax(0, 1fr) minmax(0, 210px) 110px 104px;
  gap: 14px;
  align-items: center;
  padding: 12px 4px;
  border-bottom: 1px solid var(--ln);
}
.row.head {
  padding: 10px 4px;
  font-size: 12px;
  color: var(--mu);
  letter-spacing: 0.06em;
  text-transform: uppercase;
  font-weight: 600;
}
.row.show {
  cursor: pointer;
}
.row.show:hover,
.row.show:focus-visible {
  background: var(--sf);
  outline: none;
}
.keep {
  position: relative;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  height: 28px;
  padding: 0 10px;
  border-radius: 6px;
  border: 1px solid var(--ln2);
  font-size: 13px;
  white-space: nowrap;
  cursor: pointer;
}
.keep:hover {
  background: var(--sf2);
}
.keep select {
  appearance: none;
  border: none;
  background: transparent;
  color: var(--tx);
  font: inherit;
  padding: 0;
  cursor: pointer;
  outline: none;
}
.keep select option {
  background: var(--sf);
  color: var(--tx);
}
.keep:focus-within {
  border-color: var(--tx);
}
</style>
