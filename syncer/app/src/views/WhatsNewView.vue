<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import AlbumCard from '../components/AlbumCard.vue';
import Cover from '../components/Cover.vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import { api } from '../api';
import { bytes, bytesParts, gb, num, plural, speed } from '../format';
import { canCheck, card, check, startSync, store } from '../store';

type Filter = 'all' | 'new' | 'upgrade' | 'converted' | 'tags';
const filter = ref<Filter>('all');
const showAll = ref(false);
const skipOpen = ref(!api.desktop);

// Checks on arrival, as soon as the card and folders are known.
const autoCheck = () => {
  if (!store.plan.summary && !store.plan.checking && !store.plan.error && canCheck.value) void check();
};
onMounted(autoCheck);
watch(canCheck, autoCheck);

const s = computed(() => store.plan.summary);
const pod = computed(() => s.value?.podcasts);
const podChanges = computed(() => (pod.value ? pod.value.new + pod.value.moved + pod.value.deleted : 0));
/** Episodes removed from the card because they are gone from the folder (not past a keep setting). */
const podGone = computed(() => (pod.value ? pod.value.deleted - (pod.value.leaving ?? 0) : 0));
const upToDate = computed(() => !!s.value && s.value.tracks + s.value.upgrades + s.value.fixes === 0 && !podChanges.value);
const podLine = computed(() => {
  const p = pod.value;
  if (!p) return '';
  const parts = [];
  if (p.new) parts.push(plural(p.new, 'new episode'));
  if (p.leaving) parts.push(`${num(p.leaving)} leave the card`);
  if (p.bytes) parts.push(bytes(p.bytes));
  return parts.join(' · ');
});
const podNote = computed(() => {
  const p = pod.value;
  if (!p) return '';
  const parts = [];
  if (p.leaving) parts.push("Episodes that leave the card are older than their show's keep setting. They stay on this computer.");
  if (podGone.value) parts.push(`${plural(podGone.value, 'episode')} gone from your podcasts folder ${podGone.value === 1 ? 'is' : 'are'} removed from the card.`);
  if (p.moved) parts.push(`${plural(p.moved, 'episode')} moved in your folder ${p.moved === 1 ? 'is' : 'are'} renamed on the card, not copied again.`);
  return parts.join(' ');
});
const showName = (folder: string) => folder.split(' / ').pop() || 'Podcasts';
const showParent = (folder: string) => folder.split(' / ').slice(0, -1).join(' / ');
// free 0: unknown (the browser can't see it).
const noSpace = computed(() => !!s.value && s.value.free > 0 && s.value.bytes > s.value.free);
const [toWrite, toWriteUnit] = [computed(() => bytesParts(s.value?.bytes ?? 0)[0]), computed(() => bytesParts(s.value?.bytes ?? 0)[1])];
const minutes = computed(() => Math.max(1, Math.round((s.value?.estSeconds ?? 0) / 60)));

const sub = computed(() => {
  if (store.plan.checking) return 'Comparing your music folders with the card…';
  if (!s.value) return 'Compare your music folders with the card.';
  if (upToDate.value) return `Compared your music folders with the card just now, in ${store.plan.took.toFixed(1)} s.`;
  const libs = store.settings?.libraries.map((l) => l.path) ?? [];
  const names = libs.length <= 2 ? libs.join(' and ') : `${libs.length} music folders`;
  return `Compared ${names} with the card, just now.`;
});

const counts = computed(() => {
  const a = s.value?.albumList ?? [];
  return {
    all: a.length,
    new: a.filter((x) => x.new).length,
    upgrade: a.filter((x) => x.upgrade).length,
    converted: a.filter((x) => x.converted).length,
    tags: a.filter((x) => x.tagsGuessed).length,
  };
});
const chips: [Filter, string][] = [
  ['all', 'All'],
  ['new', 'New'],
  ['upgrade', 'Upgrades'],
  ['converted', 'Converted'],
  ['tags', 'Tags guessed'],
];
const filtered = computed(() => {
  const a = s.value?.albumList ?? [];
  switch (filter.value) {
    case 'new':
      return a.filter((x) => x.new);
    case 'upgrade':
      return a.filter((x) => x.upgrade);
    case 'converted':
      return a.filter((x) => x.converted);
    case 'tags':
      return a.filter((x) => x.tagsGuessed);
    default:
      return a;
  }
});
const shown = computed(() => (showAll.value ? filtered.value : filtered.value.slice(0, 10)));

const skipped = computed(() => [...(s.value?.skipped ?? []), ...(s.value?.unreadable ?? []), ...(pod.value?.skipped ?? [])]);
const skipSub = computed(() => {
  const parts = [];
  if (s.value?.skipped.length) parts.push(api.desktop ? `${s.value.skipped.length} can't be converted` : `${s.value.skipped.length} need the desktop app`);
  if (s.value?.unreadable.length) parts.push(`${s.value.unreadable.length} unreadable`);
  return parts.join(', ');
});
const skipNote = computed(() => {
  if (s.value?.skipped.length)
    return api.desktop
      ? 'AAC, Ogg and Opus files need ffmpeg to be converted for the player. Install ffmpeg, then check again.'
      : 'The browser can’t convert AAC, Ogg or Opus files. The desktop app converts them to MP3 with ffmpeg.';
  return 'Unreadable files are usually damaged copies. Replace them in your library and check again.';
});
const speedLine = computed(() =>
  s.value?.speedMeasured
    ? `this card's last speed, about ${speed(s.value.writeSpeed)}`
    : `about ${speed(s.value?.writeSpeed ?? 0)} on a USB 2 card reader`,
);
const libTracks = computed(() => store.settings?.libraries.reduce((n, l) => n + l.tracks, 0) ?? 0);
const cardLine = computed(() => {
  const c = card.value;
  if (c?.state.kind !== 'ready') return '';
  return `${num(c.state.tracks)} tracks · ${num(c.state.albums)} albums${c.capacity ? ` · ${gb(c.free)} free` : ''}`;
});

function chooseRemovals() {
  store.screen = 'library';
  store.librarySelect = true;
}
</script>

<template>
  <ScreenHeader title="What's new" :sub="sub">
    <button class="btn" :disabled="store.plan.checking || !canCheck" @click="check">Check again</button>
    <button v-if="s && !upToDate" class="btn primary" :disabled="noSpace" @click="startSync">
      Sync {{ bytes(s.bytes) }}
    </button>
  </ScreenHeader>
  <div class="body">
    <div v-if="store.plan.error" class="note red">
      <div style="font-weight: 600; color: var(--red)">Couldn't compare</div>
      <p>{{ store.plan.error }}</p>
    </div>

    <div v-else-if="!s" class="empty">
      <template v-if="store.plan.checking">
        <div class="ring spin" style="width: 40px; height: 40px; border: 5px solid var(--sf2); border-top-color: var(--pri)"></div>
        <div class="mu">Comparing your music folders with the card…</div>
      </template>
      <template v-else>
        <div class="mu" style="font-size: 15px">Choose a card and a music folder first.</div>
        <button class="btn" @click="store.screen = 'setup'">Card &amp; music</button>
      </template>
    </div>

    <div v-else-if="upToDate" class="empty">
      <div class="ring" style="width: 56px; height: 56px; border: 6px solid var(--grn)"></div>
      <div style="font-size: 24px; font-weight: 600; letter-spacing: -0.01em">Your card is up to date</div>
      <p class="mu" style="margin: 0; font-size: 15px; line-height: 1.5; max-width: 440px; text-wrap: pretty">
        All <span class="mono" style="color: var(--tx)">{{ num(libTracks) }}</span> tracks in your music folders are
        already on the card. Add music to a folder, then check again.
      </p>
      <div class="mono mu" style="font-size: 13px">{{ cardLine }}</div>
    </div>

    <template v-else>
      <div v-if="noSpace" class="nospace">
        <div style="flex: 1; min-width: 260px">
          <div style="font-weight: 600; font-size: 16px; color: var(--red)">Not enough space on the card</div>
          <p>
            This sync needs <b class="mono">{{ bytes(s.bytes) }}</b>, but the card only has
            <b class="mono">{{ bytes(s.free) }}</b> free. Remove some albums from the card, or remove a music folder,
            then check again. Nothing has been copied.
          </p>
        </div>
        <button class="btn primary sm" @click="chooseRemovals">Choose albums to remove</button>
      </div>

      <div class="strip">
        <div>
          <div class="k">To add</div>
          <div class="v">{{ num(s.tracks) }} <small>tracks</small></div>
          <div class="s">in {{ plural(s.albums, 'album') }}</div>
        </div>
        <div>
          <div class="k">Upgrades</div>
          <div class="v" style="color: var(--mag)">{{ num(s.upgrades) }}</div>
          <div class="s">MP3 replaced by FLAC</div>
        </div>
        <div>
          <div class="k">Converted for the player</div>
          <div class="v" style="color: var(--amb)">{{ num(s.conversions) }}</div>
          <div class="s">hi-res FLAC, WAV → FLAC</div>
        </div>
        <div>
          <div class="k">To write</div>
          <div class="v">{{ toWrite }} <small>{{ toWriteUnit }}</small></div>
          <div class="s">
            {{
              !s.free ? 'check the card has room' : noSpace ? `only ${bytes(s.free)} free on the card` : `${gb(s.free)} free on the card`
            }}
          </div>
        </div>
        <div>
          <div class="k">Time</div>
          <div class="v">~{{ minutes }} <small>min</small></div>
          <div class="s">{{ speedLine }}</div>
        </div>
      </div>

      <div v-if="s.alreadyOnCard + s.unchanged > 0" class="reassure">
        <span class="ring" style="width: 12px; height: 12px; border: 2px solid var(--grn)"></span>
        <span
          >Already on the card: <b class="mono" style="font-weight: 500">{{ plural(s.alreadyOnCard + s.unchanged, 'track') }}</b>.
          They won't be copied again.</span
        >
      </div>
      <div v-if="s.fixes" class="mu" style="font-size: 13px; margin-top: -10px">
        {{ plural(s.fixes, 'file') }} already on the card will be converted so the player can play them.
      </div>
      <div v-if="s.removed" class="mu" style="font-size: 13px; margin-top: -10px">
        {{ plural(s.removed, 'file') }} skipped: their albums were removed from the card.
        <button class="link" @click="store.screen = 'library'; store.libraryTab = 'removed'">See Removed</button>
      </div>

      <div v-if="s.albumList.length" style="display: flex; flex-direction: column; gap: 14px">
        <div style="display: flex; align-items: center; gap: 14px; flex-wrap: wrap">
          <h2 style="margin: 0; font-size: 17px; font-weight: 600">Albums to write</h2>
          <div style="display: flex; gap: 6px; flex-wrap: wrap">
            <template v-for="[id, label] in chips" :key="id">
              <button v-if="id === 'all' || counts[id]" class="chip" :class="{ on: filter === id }" @click="filter = id">
                {{ label }} <span class="mono">{{ counts[id] }}</span>
              </button>
            </template>
          </div>
        </div>
        <div class="album-grid">
          <AlbumCard
            v-for="a in shown"
            :key="a.id"
            :title="a.title"
            :artist="a.artist"
            :count="`${a.tracks} tr`"
            :cover="a.cover"
            :is-new="a.new"
            :upgrade="a.upgrade"
            :converted="a.converted"
            :tags-guessed="a.tagsGuessed"
          />
        </div>
        <div v-if="filtered.length > 10" class="mu" style="font-size: 13px">
          <template v-if="!showAll">
            Showing 10 of {{ plural(filtered.length, 'album') }}.
            <button class="link" @click="showAll = true">Show all</button>
          </template>
          <button v-else class="link" @click="showAll = false">Show fewer</button>
        </div>
      </div>

      <div v-if="podChanges" style="display: flex; flex-direction: column; gap: 14px">
        <div style="display: flex; align-items: baseline; gap: 14px; flex-wrap: wrap">
          <h2 style="margin: 0; font-size: 17px; font-weight: 600">Podcasts</h2>
          <span v-if="podLine" class="mono mu" style="font-size: 13px">{{ podLine }}</span>
        </div>
        <div v-if="pod?.shows.length" class="podlist">
          <div v-for="sh in pod.shows" :key="sh.key" class="podrow">
            <Cover class="thumb" :path="sh.cover" />
            <div style="flex: 1; min-width: 140px">
              <div class="ellipsis" style="font-weight: 600; font-size: 14px">{{ showName(sh.folder) }}</div>
              <div class="mu" style="font-size: 13px">{{ showParent(sh.folder) }}</div>
            </div>
            <div style="display: flex; gap: 6px; flex-wrap: wrap; justify-content: flex-end">
              <span v-if="sh.episodes" class="badge new" style="white-space: nowrap">+{{ sh.episodes }} new</span>
              <span v-if="sh.leaving" class="badge leave" style="white-space: nowrap">
                {{ sh.leaving === 1 ? '1 leaves' : `${sh.leaving} leave` }}
              </span>
            </div>
            <span class="mono mu" style="font-size: 13px; width: 64px; text-align: right; flex: none">{{ sh.bytes ? bytes(sh.bytes) : '' }}</span>
          </div>
        </div>
        <div v-if="podNote" class="mu" style="font-size: 13px; line-height: 1.5; text-wrap: pretty">{{ podNote }}</div>
      </div>

      <div v-if="skipped.length" class="skip">
        <button class="skip-head" @click="skipOpen = !skipOpen">
          <span class="mono mu" style="width: 10px">{{ skipOpen ? '▾' : '▸' }}</span>
          <span style="font-weight: 600">{{ plural(skipped.length, 'file') }} skipped</span>
          <span class="mu" style="font-size: 13px">{{ skipSub }}</span>
        </button>
        <div v-if="skipOpen" class="skip-body">
          <div v-for="k in skipped" :key="k.path" class="skip-row">
            <span class="mono" style="flex: 1; min-width: 240px; overflow-wrap: anywhere">{{ k.path }}</span>
            <span class="mu">{{ k.reason }}</span>
          </div>
          <p class="mu" style="margin: 12px 0 0; font-size: 13px; line-height: 1.5; text-wrap: pretty">{{ skipNote }}</p>
        </div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.podlist {
  display: flex;
  flex-direction: column;
  border: 1px solid var(--ln);
  border-radius: 12px;
  background: var(--sf);
  overflow: hidden;
}
.podrow {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 10px 16px;
  border-bottom: 1px solid var(--ln);
}
.podrow:last-child {
  border-bottom: none;
}
.empty {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  gap: 12px;
  padding: 60px 20px;
}
.spin {
  animation: spin 0.9s linear infinite;
}
@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}
.nospace {
  display: flex;
  gap: 18px;
  align-items: flex-start;
  background: var(--redS);
  border: 1px solid var(--red);
  border-radius: 12px;
  padding: 18px 20px;
  flex-wrap: wrap;
}
.nospace p {
  margin: 6px 0 0;
  font-size: 14px;
  line-height: 1.5;
  text-wrap: pretty;
}
.skip {
  border: 1px solid var(--ln);
  border-radius: 12px;
  background: var(--sf);
}
.skip-head {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 14px 18px;
  cursor: pointer;
  user-select: none;
  width: 100%;
  background: none;
  border: none;
  text-align: left;
}
.skip-body {
  border-top: 1px solid var(--ln);
  padding: 6px 18px 16px;
  display: flex;
  flex-direction: column;
}
.skip-row {
  display: flex;
  gap: 14px;
  padding: 9px 0;
  border-bottom: 1px solid var(--ln);
  font-size: 13px;
  align-items: baseline;
  flex-wrap: wrap;
}
</style>
