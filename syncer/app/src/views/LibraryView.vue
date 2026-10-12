<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from 'vue';
import AlbumCard from '../components/AlbumCard.vue';
import Cover from '../components/Cover.vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import type { CardAlbum } from '../api';
import { albumTime, bytes, day, gb, num, plural, shortDay, trackTime } from '../format';
import { bringBack, card, loadLibrary, removeSelected, store } from '../store';

type Sort = 'artist' | 'album' | 'added';
const sort = ref<Sort>('artist');
const search = ref('');
const needsTags = ref(false);
const detail = ref<CardAlbum | null>(null);
const confirming = ref(false);
const removing = ref(false);
const cancelBtn = ref<HTMLButtonElement | null>(null);

onMounted(() => {
  if (!store.library) void loadLibrary();
});
watch(
  () => card.value?.path,
  () => {
    store.library = null;
    detail.value = null;
    void loadLibrary();
  },
);

const lib = computed(() => store.library);
const albums = computed(() => lib.value?.albums ?? []);
const tagged = computed(() => albums.value.filter((a) => a.tagsGuessed));
const fold = (s: string) => s.normalize('NFD').replace(/\p{M}/gu, '').toLowerCase();
const visible = computed(() => {
  let a = needsTags.value ? tagged.value : albums.value;
  const q = fold(search.value.trim());
  if (q) a = a.filter((x) => fold(x.title).includes(q) || fold(x.artist).includes(q));
  const by = {
    artist: (x: CardAlbum, y: CardAlbum) => x.artist.localeCompare(y.artist) || x.title.localeCompare(y.title),
    album: (x: CardAlbum, y: CardAlbum) => x.title.localeCompare(y.title),
    added: (x: CardAlbum, y: CardAlbum) => y.addedAt - x.addedAt,
  }[sort.value];
  return [...a].sort(by);
});

const selected = computed(() => albums.value.filter((a) => store.selection.has(a.id)));
const selTracks = computed(() => selected.value.reduce((n, a) => n + a.tracks.length, 0));
const selBytes = computed(() => selected.value.reduce((n, a) => n + a.bytes, 0));

const sub = computed(() => {
  if (store.librarySelect)
    return `${plural(selected.value.length, 'album')} selected · ${plural(selTracks.value, 'track')} · ${bytes(selBytes.value)}`;
  const l = lib.value;
  if (!l) return '';
  const used = l.capacity ? `${gb(l.bytes)} of ${gb(l.capacity)} used` : gb(l.bytes);
  return `${num(l.tracks)} tracks · ${plural(albums.value.length, 'album')} · ${used}`;
});

function toggle(a: CardAlbum) {
  const s = new Set(store.selection);
  if (s.has(a.id)) s.delete(a.id);
  else s.add(a.id);
  store.selection = s;
}
function selectArtist(artist: string) {
  const s = new Set(store.selection);
  const theirs = albums.value.filter((a) => a.artist === artist);
  const all = theirs.every((a) => s.has(a.id));
  for (const a of theirs) all ? s.delete(a.id) : s.add(a.id);
  store.selection = s;
}
function open(a: CardAlbum) {
  if (store.librarySelect) toggle(a);
  else detail.value = a;
}
function startSelect(a?: CardAlbum) {
  store.libraryTab = 'albums';
  store.librarySelect = true;
  store.selection = new Set(a ? [a.id] : []);
  detail.value = null;
}
function cancelSelect() {
  store.librarySelect = false;
  store.selection = new Set();
}
async function askConfirm() {
  confirming.value = true;
  await nextTick();
  cancelBtn.value?.focus();
}
async function confirmRemove() {
  removing.value = true;
  await removeSelected();
  removing.value = false;
  confirming.value = false;
  store.librarySelect = false;
}
const detailMeta = computed(() => {
  const a = detail.value;
  return a ? [plural(a.tracks.length, 'track'), albumTime(a.duration), bytes(a.bytes), a.format].join(' · ') : '';
});
const trackNo = (t: CardAlbum['tracks'][number], a: CardAlbum) => {
  const multi = a.tracks.some((x) => x.disc > 1);
  const n = String(t.no).padStart(2, '0');
  return multi ? `${t.disc}-${n}` : n;
};
</script>

<template>
  <ScreenHeader title="On the card" :sub="sub">
    <template v-if="store.librarySelect">
      <button class="btn" @click="cancelSelect">Cancel</button>
      <button class="btn danger" :disabled="!selected.length" @click="askConfirm">Remove from card</button>
    </template>
    <button v-else-if="store.libraryTab === 'albums' && !detail && albums.length" class="btn" @click="startSelect()">
      Select
    </button>
  </ScreenHeader>

  <div class="body">
    <div v-if="!lib" class="mu" style="padding: 40px 0; text-align: center">
      {{ card?.state.kind === 'ready' ? 'Loading…' : 'No OpenPod card. Plug one in, or sync to set this one up.' }}
    </div>

    <template v-else-if="detail">
      <button class="link" style="margin-top: -6px; align-self: flex-start; font-size: 14px" @click="detail = null">
        ← On the card
      </button>
      <div style="display: flex; gap: 28px; flex-wrap: wrap; align-items: flex-start">
        <Cover :path="detail.cover" style="width: 220px; flex: none" />
        <div style="flex: 1; min-width: 280px; display: flex; flex-direction: column; gap: 14px">
          <div>
            <div style="font-size: 28px; font-weight: 600; letter-spacing: -0.02em">{{ detail.title }}</div>
            <div class="mu" style="font-size: 17px; margin-top: 2px">{{ detail.artist }}</div>
          </div>
          <div class="mono mu" style="font-size: 13px">{{ detailMeta }}</div>
          <div class="kv">
            <span class="mu">Came from</span>
            <span v-if="detail.machines.length"
              >from
              <template v-for="(m, i) in detail.machines" :key="m"
                ><span class="mono">{{ m }}</span><template v-if="i < detail.machines.length - 1">, </template></template
              ></span
            >
            <span v-else>already on the card</span>
            <span class="mu">Added</span><span>{{ day(detail.addedAt, true) }}</span>
            <span class="mu">Converted</span
            ><span>{{ detail.conversions.length ? detail.conversions.join('; ') : 'No, copied as it is' }}</span>
            <span class="mu">Tags</span><span>{{ detail.tagsGuessed ? 'Guessed from folders' : 'Read from the files' }}</span>
          </div>
          <div><button class="btn sm danger-text" @click="startSelect(detail!)">Remove from card…</button></div>
        </div>
      </div>
      <div style="display: flex; flex-direction: column; border-top: 1px solid var(--ln)">
        <div v-for="t in detail.tracks" :key="t.path + t.no" class="track">
          <span class="mono mu" style="font-size: 13px">{{ trackNo(t, detail) }}</span>
          <span class="ellipsis">{{ t.title }}</span>
          <span class="mono" style="font-size: 13px; text-align: right">{{ trackTime(t.duration) }}</span>
          <span class="mono mu" style="font-size: 12px; text-align: right">{{ t.format }}</span>
        </div>
      </div>
    </template>

    <template v-else>
      <div v-if="!store.librarySelect" class="tabs">
        <button :class="{ on: store.libraryTab === 'albums' }" @click="store.libraryTab = 'albums'">
          Albums <span class="mono mu">{{ num(albums.length) }}</span>
        </button>
        <button :class="{ on: store.libraryTab === 'removed' }" @click="store.libraryTab = 'removed'">
          Removed <span class="mono mu">{{ num(lib.removed.length) }}</span>
        </button>
      </div>

      <template v-if="store.libraryTab === 'albums' || store.librarySelect">
        <div class="toolbar">
          <label class="search">
            <span class="ring" style="width: 10px; height: 10px; border: 1.5px solid var(--mu)"></span>
            <input v-model="search" placeholder="Search albums and artists" />
          </label>
          <div class="seg">
            <button :class="{ on: sort === 'artist' }" @click="sort = 'artist'">Artist</button>
            <button :class="{ on: sort === 'album' }" @click="sort = 'album'">Album</button>
            <button :class="{ on: sort === 'added' }" @click="sort = 'added'">Date added</button>
          </div>
          <button v-if="tagged.length" class="tagchip" :class="{ on: needsTags }" @click="needsTags = !needsTags">
            <span style="width: 8px; height: 8px; border: 1px dashed currentColor; border-radius: 2px"></span>Needs tags:
            {{ plural(tagged.length, 'album') }}
          </button>
        </div>
        <div v-if="needsTags" class="tagnote">
          <b>These albums were copied, but their tags are missing or incomplete.</b> Names were guessed from the folder
          layout (Artist / Album / 01 Title), or show as "Unknown". Tags are fixed in your music library, not here: edit
          them with a tag editor, then check what's new again. The card is updated with the correct names.
        </div>
        <div class="album-grid">
          <AlbumCard
            v-for="a in visible"
            :key="a.id"
            :title="a.title"
            :artist="a.artist"
            :count="plural(a.tracks.length, 'track')"
            :cover="a.cover"
            :converted="a.conversions.length > 0"
            :tags-guessed="a.tagsGuessed"
            :selectable="store.librarySelect"
            :selected="store.selection.has(a.id)"
            @open="open(a)"
            @artist="selectArtist(a.artist)"
          />
        </div>
        <div v-if="!visible.length" class="mu" style="font-size: 14px">
          {{ albums.length ? 'No album matches.' : 'The card has no music yet.' }}
        </div>
      </template>

      <template v-else>
        <p class="mu" style="margin: 0; font-size: 14px; line-height: 1.5; max-width: 640px; text-wrap: pretty">
          Albums here are skipped by every sync, from any computer. Bring one back and the next sync copies it again.
        </p>
        <div style="display: flex; flex-direction: column; border-top: 1px solid var(--ln)">
          <div v-for="r in lib.removed" :key="r.id" class="removed">
            <Cover class="thumb" :path="`tone:${r.title.length}`" />
            <div style="flex: 1; min-width: 200px">
              <div style="font-weight: 600; font-size: 14px">{{ r.title }}</div>
              <div class="mu" style="font-size: 13px">{{ r.artist }} · {{ plural(r.tracks, 'track') }}</div>
            </div>
            <span class="badge removed">removed</span>
            <span class="mono mu" style="font-size: 13px; min-width: 190px">{{ shortDay(r.removedAt) }} · from {{ r.machine }}</span>
            <button class="btn xxs" @click="bringBack(r.id)">Bring back</button>
          </div>
          <div v-if="!lib.removed.length" class="mu" style="padding: 14px 4px; font-size: 14px">Nothing removed.</div>
        </div>
      </template>
    </template>
  </div>

  <div v-if="confirming" class="scrim" @keydown.esc="confirming = false">
    <div class="dialog" role="dialog" aria-modal="true" aria-labelledby="rm-title">
      <div id="rm-title" style="font-size: 19px; font-weight: 600">
        Remove {{ plural(selected.length, 'album') }} from the card?
      </div>
      <div class="list">
        <div v-for="a in selected" :key="a.id">
          <span class="ellipsis">{{ a.title }} · {{ a.artist }}</span
          ><span class="mono mu">{{ a.tracks.length }}</span>
        </div>
      </div>
      <div style="font-size: 14px; line-height: 1.5">
        Deletes <b class="mono" style="font-weight: 500">{{ plural(selTracks, 'track') }} ({{ bytes(selBytes) }})</b> from
        the card. Syncs from this or other computers won't add them back.
      </div>
      <div class="mu" style="font-size: 13px; line-height: 1.5">
        Your music library isn't touched. You can bring them back later from Removed.
      </div>
      <div style="display: flex; justify-content: flex-end; gap: 10px; margin-top: 6px">
        <button ref="cancelBtn" class="btn cancel" :disabled="removing" @click="confirming = false">Cancel</button>
        <button class="btn danger" :disabled="removing" @click="confirmRemove">
          {{ removing ? 'Removing…' : 'Remove from card' }}
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.toolbar {
  display: flex;
  gap: 10px;
  align-items: center;
  flex-wrap: wrap;
}
.search {
  flex: 1 1 200px;
  min-width: 160px;
  max-width: 320px;
  height: 36px;
  border: 1px solid var(--ln2);
  border-radius: 8px;
  display: flex;
  align-items: center;
  padding: 0 12px;
  gap: 8px;
  background: var(--sf);
}
.search input {
  flex: 1;
  min-width: 0;
  border: none;
  outline: none;
  background: transparent;
  font-size: 14px;
}
.search input::placeholder {
  color: var(--mu);
}
.seg {
  display: flex;
  flex: none;
  border: 1px solid var(--ln2);
  border-radius: 8px;
  overflow: hidden;
  height: 36px;
}
.seg button {
  display: flex;
  align-items: center;
  white-space: nowrap;
  padding: 0 12px;
  font-size: 13px;
  color: var(--mu);
  background: none;
  border: none;
  cursor: pointer;
}
.seg button + button {
  border-left: 1px solid var(--ln2);
}
.seg button.on {
  background: var(--sf2);
  color: var(--tx);
  font-weight: 500;
}
.tagchip {
  height: 36px;
  display: flex;
  white-space: nowrap;
  flex: none;
  align-items: center;
  gap: 8px;
  padding: 0 12px;
  border-radius: 8px;
  font-size: 13px;
  cursor: pointer;
  border: 1px solid var(--ln2);
  background: transparent;
  color: var(--mu);
}
.tagchip.on {
  border-color: var(--tx);
  background: var(--sf2);
  color: var(--tx);
  font-weight: 600;
}
.tagnote {
  background: var(--sf);
  border: 1px dashed var(--ln2);
  border-radius: 12px;
  padding: 16px 20px;
  font-size: 14px;
  line-height: 1.55;
  text-wrap: pretty;
}
.kv {
  display: grid;
  grid-template-columns: max-content 1fr;
  gap: 8px 18px;
  font-size: 14px;
  border-top: 1px solid var(--ln);
  padding-top: 14px;
}
.track {
  display: grid;
  grid-template-columns: 36px minmax(0, 1fr) 64px 170px;
  gap: 12px;
  align-items: center;
  padding: 9px 4px;
  border-bottom: 1px solid var(--ln);
  font-size: 14px;
}
.removed {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 12px 4px;
  border-bottom: 1px solid var(--ln);
  flex-wrap: wrap;
}
.scrim {
  position: absolute;
  inset: 0;
  background: var(--scrim);
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
  z-index: 10;
}
.dialog {
  background: var(--sf);
  border: 1px solid var(--ln);
  border-radius: 14px;
  box-shadow: var(--shadow);
  padding: 26px;
  display: flex;
  flex-direction: column;
  gap: 14px;
  width: 100%;
  max-width: 460px;
}
.list {
  display: flex;
  flex-direction: column;
  gap: 2px;
  font-size: 14px;
  background: var(--sf2);
  border-radius: 8px;
  padding: 10px 14px;
  max-height: 180px;
  overflow: auto;
}
.list > div {
  display: flex;
  justify-content: space-between;
  gap: 12px;
}
</style>
