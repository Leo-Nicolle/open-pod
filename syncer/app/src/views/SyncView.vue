<script setup lang="ts">
import { computed } from 'vue';
import Cover from '../components/Cover.vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import { api } from '../api';
import { about, bytes, bytesParts, clock, num, plural, speed } from '../format';
import { card, check, eject, startSync, stopSync, store } from '../store';

const p = computed(() => store.sync.progress);
const r = computed(() => store.sync.report);
const pct = computed(() => (p.value && p.value.bytesTotal ? (p.value.bytesDone / p.value.bytesTotal) * 100 : 0));
const cardName = computed(() => card.value?.name ?? 'the card');
const files = computed(() => (store.sync.episodes ? 'tracks & episodes' : 'tracks'));

const title = computed(
  () =>
    ({ running: 'Syncing', stopped: 'Sync stopped', done: 'Sync finished', error: "Sync didn't finish", idle: 'Sync' })[
      store.sync.state
    ],
);
const sub = computed(() => {
  switch (store.sync.state) {
    case 'running':
      return `Writing to ${cardName.value}. Stopping is safe: the current file finishes and nothing is lost.`;
    case 'stopped':
      return 'The card is safe to eject.';
    case 'done':
      return r.value ? `${cardName.value} now has ${plural(r.value.cardTracks, 'track')} in ${plural(r.value.cardAlbums, 'album')}.` : '';
    case 'error':
      return 'Nothing was lost: everything copied before the problem is saved on the card.';
    default:
      return 'Copies what the card is missing. Check what’s new first.';
  }
});

/** The reader is the limit below ~30 MB/s (USB 2 tops out around there). */
const slow = computed(() => (p.value?.speed ?? 0) > 0 && (p.value?.speed ?? 0) < 30e6);
const leftAfterStop = computed(() => {
  const x = p.value;
  if (!x) return '';
  const s = x.speed || 18e6;
  return `${about((x.bytesTotal - x.bytesDone) / s)} left to copy`;
});
const issues = computed(() => {
  const out: [string, string][] = [];
  for (const w of r.value?.warnings ?? []) out.push(['Cover not readable', w]);
  for (const f of r.value?.failed ?? []) out.push(['File not copied', `${f.path}: ${f.reason}`]);
  for (const f of r.value?.podcasts?.failed ?? []) out.push(['Episode not copied', `${f.path}: ${f.reason}`]);
  return out;
});
const avg = computed(() => (r.value && r.value.seconds ? r.value.bytesWritten / r.value.seconds : 0));
const [doneVal, doneUnit] = [
  computed(() => bytesParts(p.value?.bytesDone ?? 0)[0]),
  computed(() => bytesParts(p.value?.bytesDone ?? 0)[1]),
];
</script>

<template>
  <ScreenHeader :title="title" :sub="sub">
    <button v-if="store.sync.state === 'running'" class="btn" :disabled="store.sync.stopping" @click="stopSync">
      {{ store.sync.stopping ? 'Stopping…' : 'Stop' }}
    </button>
    <button v-if="store.sync.state === 'stopped'" class="btn primary" @click="startSync">Sync again</button>
    <button v-if="store.sync.state === 'done'" class="btn" @click="store.screen = 'settings'">View report</button>
    <button v-if="store.sync.state === 'idle' || store.sync.state === 'error'" class="btn primary" @click="check">
      Check what's new
    </button>
  </ScreenHeader>

  <div class="body">
    <div v-if="store.sync.state === 'idle'" class="mu" style="font-size: 15px; padding: 40px 0; text-align: center">
      Nothing is syncing. Check what's new to see what the card is missing.
    </div>

    <div v-else-if="store.sync.state === 'error'" class="note red">
      <div style="font-weight: 600; color: var(--red)">The sync stopped with an error</div>
      <p>{{ store.sync.error }}</p>
    </div>

    <template v-else-if="store.sync.state === 'running'">
      <section class="panel big">
        <div class="top">
          <div class="mono pct">{{ Math.floor(pct) }}<span>%</span></div>
          <div class="mono" style="font-size: 17px">
            {{ doneVal }} {{ doneUnit }} <span class="mu">of {{ bytes(p?.bytesTotal ?? 0) }}</span>
          </div>
        </div>
        <div class="bar"><div :style="{ width: pct + '%' }"></div></div>
        <div class="stats">
          <div><div class="k">Speed</div><div class="v mono">{{ p?.speed ? speed(p.speed) : '…' }}</div></div>
          <div><div class="k">Elapsed</div><div class="v mono">{{ clock(p?.elapsed ?? 0) }}</div></div>
          <div><div class="k">Time left</div><div class="v mono">{{ p?.eta != null ? about(p.eta) : '…' }}</div></div>
          <div>
            <div class="k">{{ store.sync.episodes ? 'Tracks & episodes' : 'Tracks' }}</div>
            <div class="v mono">{{ num(p?.filesDone ?? 0) }} <span class="mu">/ {{ num(p?.filesTotal ?? 0) }}</span></div>
          </div>
        </div>
        <div v-if="slow" class="mu sep" style="font-size: 13px">
          USB 2 card reader: about {{ speed(p!.speed!) }}. That's the reader's limit; a USB 3 reader would be several
          times faster.
        </div>
      </section>
      <div class="two">
        <section class="panel now">
          <div class="label">Now</div>
          <div v-if="p?.copying" class="item">
            <div class="row">
              <span style="color: var(--grn); font-weight: 600">Copying</span>
              <span class="mono mu">{{ (p.copying.done / 1e6).toFixed(1) }} / {{ (p.copying.total / 1e6).toFixed(1) }} MB</span>
            </div>
            <div class="mono path">{{ p.copying.path }}</div>
            <div class="bar xs"><div :style="{ width: (p.copying.total ? (p.copying.done / p.copying.total) * 100 : 0) + '%' }"></div></div>
          </div>
          <div v-for="c in p?.converting ?? []" :key="c.worker" class="item conv">
            <div class="row">
              <span style="color: var(--amb); font-weight: 600">Converting</span>
              <span class="mono mu">{{ c.what }}</span>
            </div>
            <div class="mono path">{{ c.path }}</div>
          </div>
          <div v-if="!p?.copying && !p?.converting.length" class="mu" style="font-size: 13px">Preparing…</div>
        </section>
        <section class="panel" style="gap: 6px; padding: 20px 22px">
          <div class="label" style="margin-bottom: 6px">Done so far</div>
          <div v-for="a in p?.recent ?? []" :key="a.dir" class="recent">
            <Cover class="thumb" :path="a.cover" />
            <div style="flex: 1; min-width: 0">
              <div class="ellipsis" style="font-size: 14px; font-weight: 500">{{ a.title }}</div>
              <div class="mu" style="font-size: 12px">{{ a.artist }}</div>
            </div>
            <span class="mono mu" style="font-size: 12px">{{ plural(a.tracks, 'track') }}</span>
          </div>
          <div v-if="!p?.recent.length" class="mu" style="font-size: 13px">Nothing yet.</div>
        </section>
      </div>
    </template>

    <section v-else-if="store.sync.state === 'stopped'" class="panel big" style="gap: 16px">
      <div style="font-size: 21px; font-weight: 600">
        Stopped: <span class="mono" style="font-weight: 500">{{ num(p?.filesDone ?? r?.written ?? 0) }}</span> of
        <span class="mono" style="font-weight: 500">{{ num(p?.filesTotal ?? store.sync.total) }}</span> {{ files }} copied.
        Sync again to continue.
      </div>
      <div class="bar stopped"><div :style="{ width: pct + '%' }"></div></div>
      <div class="mono mu" style="display: flex; justify-content: space-between; font-size: 14px; flex-wrap: wrap; gap: 8px">
        <span>{{ bytes(p?.bytesDone ?? 0) }} of {{ bytes(p?.bytesTotal ?? 0) }}</span><span>{{ leftAfterStop }}</span>
      </div>
      <div class="reassure">
        <span class="ring" style="width: 12px; height: 12px; border: 2px solid var(--grn)"></span>Everything copied so
        far is saved on the card. The next sync starts from track {{ num((p?.filesDone ?? 0) + 1) }}.
      </div>
    </section>

    <template v-else-if="store.sync.state === 'done' && r">
      <div class="strip" style="grid-template-columns: repeat(auto-fit, minmax(160px, 1fr))">
        <div>
          <div class="k">Tracks written</div>
          <div class="v">{{ num(r.written) }}</div>
          <div class="s">{{ bytes(r.bytesWritten) }}</div>
        </div>
        <div>
          <div class="k">Converted</div>
          <div class="v" style="color: var(--amb)">{{ num(r.converted) }}</div>
          <div class="s">for the player</div>
        </div>
        <div>
          <div class="k">Upgraded</div>
          <div class="v" style="color: var(--mag)">{{ num(r.upgraded) }}</div>
          <div class="s">MP3 → FLAC</div>
        </div>
        <div v-if="r.podcasts">
          <div class="k">Podcasts</div>
          <div class="v">{{ num(r.podcasts.copied) }}</div>
          <div class="s">
            new episodes{{ r.podcasts.moved ? `, ${num(r.podcasts.moved)} moved` : ''
            }}{{ r.podcasts.deleted ? `, ${num(r.podcasts.deleted)} removed` : '' }}
          </div>
        </div>
        <div>
          <div class="k">Time taken</div>
          <div class="v">{{ clock(r.seconds) }}</div>
          <div class="s">{{ avg ? `average ${speed(avg)}` : '' }}</div>
        </div>
      </div>
      <section v-if="issues.length" class="panel issues">
        <div style="display: flex; gap: 10px; align-items: center">
          <span class="dot" style="width: 8px; height: 8px; background: var(--amb)"></span>
          <span style="font-weight: 600">{{ plural(issues.length, 'warning') }}</span>
        </div>
        <div v-for="([k, v], i) in issues" :key="i" class="issue" :class="{ sep: i > 0 }">
          <span class="mu" style="width: 150px; flex: none">{{ k }}</span>
          <span style="flex: 1; min-width: 240px; overflow-wrap: anywhere">{{ v }}</span>
        </div>
      </section>
      <section class="eject">
        <div style="flex: 1; min-width: 240px">
          <div style="font-weight: 600; font-size: 16px">Eject the card before unplugging it.</div>
          <div class="mu" style="font-size: 14px; margin-top: 3px">
            {{
              api.desktop
                ? 'Writing is finished. Ejecting makes sure every file is saved.'
                : 'Use your computer’s Eject or Safely Remove first; the browser can’t do it for you.'
            }}
          </div>
        </div>
        <button v-if="api.desktop" class="btn primary" @click="eject">Eject card</button>
      </section>
    </template>
  </div>
</template>

<style scoped>
.big {
  padding: 26px 28px;
  gap: 18px;
}
.top {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 16px;
  flex-wrap: wrap;
}
.pct {
  font-size: 56px;
  font-weight: 500;
  letter-spacing: -0.04em;
  line-height: 1;
}
.pct span {
  font-size: 26px;
  color: var(--mu);
}
.stats {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
  gap: 14px;
}
.stats .k {
  font-size: 12px;
  color: var(--mu);
}
.stats .v {
  font-size: 19px;
  margin-top: 2px;
}
.sep {
  border-top: 1px solid var(--ln);
  padding-top: 14px;
}
.two {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 20px;
  align-items: start;
}
.now {
  padding: 20px 22px;
}
.item {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.item.conv,
.item + .item {
  border-top: 1px solid var(--ln);
  padding-top: 14px;
}
.row {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
  gap: 12px;
}
.path {
  font-size: 13px;
  line-height: 1.5;
  overflow-wrap: anywhere;
}
.recent {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 6px 0;
}
.issues {
  padding: 18px 22px;
  gap: 12px;
}
.issue {
  display: flex;
  gap: 14px;
  font-size: 14px;
  line-height: 1.5;
  flex-wrap: wrap;
}
.issue.sep {
  border-top: 1px solid var(--ln);
  padding-top: 12px;
}
.eject {
  border-radius: 12px;
  background: var(--sf2);
  padding: 18px 22px;
  display: flex;
  gap: 16px;
  align-items: center;
  flex-wrap: wrap;
}
</style>
