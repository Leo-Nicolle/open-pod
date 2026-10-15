<script setup lang="ts">
import { computed, ref } from 'vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import { api } from '../api';
import { about, day, gb, num, plural } from '../format';
import {
  addExclude,
  addLibrary,
  canCheck,
  card,
  check,
  chooseCardFolder,
  choosePodcasts,
  readCard,
  refreshCards,
  removeExclude,
  removeLibrary,
  stopPodcasts,
  store,
  useCard,
} from '../store';

const emit = defineEmits<{ rename: [] }>();
const adv = ref(false);

const kind = computed(() => (store.read.running ? 'reading' : (card.value?.state.kind ?? 'missing')));
const usedPct = computed(() => {
  const c = card.value;
  return c && c.capacity ? ((c.capacity - c.free) / c.capacity) * 100 : 0;
});
const ready = computed(() => (card.value?.state.kind === 'ready' ? card.value.state : null));
const unread = computed(() => (card.value?.state.kind === 'unread' ? card.value.state : null));
const lastSynced = computed(() => {
  const l = ready.value?.lastSync;
  return l ? `Last synced from ${l.machine} on ${day(l.at)}.` : 'Not synced from any computer yet.';
});
const lockReason = computed(() => (card.value?.state.kind === 'locked' ? card.value.state.reason : ''));
const isLockSwitch = computed(() => /read-only|EROFS|access is denied/i.test(lockReason.value));
const libLine = (l: { tracks: number; albums: number; formats: string[]; exists: boolean }) =>
  l.exists
    ? [plural(l.tracks, 'track'), plural(l.albums, 'album'), l.formats.join(', ')].filter(Boolean).join(' · ')
    : 'Folder not found';
const read = computed(() => store.read.progress);
const pod = computed(() => store.settings?.podcasts ?? null);
const podLine = computed(() => {
  const p = pod.value;
  if (!p) return '';
  if (!p.exists) return 'Folder not found';
  return api.desktop ? [plural(p.episodes, 'episode'), plural(p.shows, 'folder')].join(' · ') : '';
});
</script>

<template>
  <ScreenHeader
    title="Card &amp; music"
    sub="Plug in the card and choose your music and podcasts folders. Nothing is written until you confirm."
  >
    <button class="btn primary" :disabled="!canCheck" @click="check">Check what's new</button>
  </ScreenHeader>
  <div class="body">
    <div class="cols">
      <section class="panel">
        <div class="label">The card</div>

        <template v-if="kind === 'missing'">
          <div class="dashed">
            <div style="font-size: 19px; font-weight: 600">No card found</div>
            <p class="mu">
              Plug the player's SD card into this computer. It shows up here by itself. If it doesn't, choose the
              card's folder yourself.
            </p>
            <button class="btn sm" style="margin-top: 6px" @click="chooseCardFolder">Choose folder…</button>
          </div>
          <div class="watch">
            <span class="ring" style="width: 7px; height: 7px; border: 1.5px solid var(--mu)"></span>Watching for cards…
          </div>
        </template>

        <template v-else-if="kind === 'locked'">
          <div class="ident">
            <div class="sd" style="border-color: var(--red)"></div>
            <div>
              <div class="cname">{{ card!.name }}</div>
              <div class="mono mu sm">{{ card!.path }} · {{ gb(card!.capacity) }} · read-only</div>
            </div>
          </div>
          <div class="note red">
            <div style="font-weight: 600; color: var(--red)">This card can't be written to</div>
            <p v-if="isLockSwitch">
              The card is locked. Slide the small lock switch on the side of the SD card (or its adapter) up, away from
              "Lock", then plug it in again.
            </p>
            <p v-else>{{ lockReason }}. Check that the card is mounted with write access for your user.</p>
          </div>
          <div style="display: flex; gap: 10px">
            <button class="btn sm" @click="refreshCards">Try again</button>
            <button class="link" style="padding: 0 12px" @click="chooseCardFolder">Choose another folder…</button>
          </div>
        </template>

        <template v-else-if="kind === 'empty'">
          <div class="ident">
            <div class="sd"></div>
            <div>
              <div class="cname">{{ card!.name }}</div>
              <div class="mono mu sm">{{ card!.path }} · {{ gb(card!.capacity) }} · {{ gb(card!.free) }} free</div>
            </div>
          </div>
          <div class="note green">
            <div style="font-weight: 600">A new card</div>
            <p>
              This card hasn't been used with OpenPod yet. The first sync sets it up and copies your music. Nothing
              else on the card is touched.
            </p>
          </div>
        </template>

        <template v-else-if="kind === 'unread'">
          <div class="ident">
            <div class="sd" style="border-color: var(--amb)"></div>
            <div>
              <div class="cname">{{ card!.name }}</div>
              <div class="mono mu sm">
                {{ card!.path }} · {{ gb(card!.capacity) }} · {{ num(unread!.files) }} files ·
                {{ gb(unread!.bytes) }} used
              </div>
            </div>
          </div>
          <div class="note amber">
            <div style="font-weight: 600">This card already has music</div>
            <p>
              To avoid copying anything twice, the app reads the whole card once to learn what's on it. This takes
              about a minute. Nothing on the card is changed. After this, checks take under a second.
            </p>
          </div>
          <div v-if="store.read.error" class="note red">
            <p>{{ store.read.error }}</p>
          </div>
          <div><button class="btn primary sm" @click="readCard">Read the card</button></div>
        </template>

        <template v-else-if="kind === 'reading'">
          <div class="ident">
            <div class="sd"></div>
            <div>
              <div class="cname">{{ card?.name }}</div>
              <div class="mono mu sm">
                {{ card?.path }} · {{ gb(card?.capacity ?? 0) }}<template v-if="read"> · {{ num(read.files) }} files</template>
              </div>
            </div>
          </div>
          <div style="display: flex; flex-direction: column; gap: 10px">
            <div style="display: flex; justify-content: space-between; align-items: baseline">
              <span style="font-weight: 600">Reading the card, once</span>
              <span v-if="read" class="mono mu sm">{{ num(read.filesDone) }} / {{ num(read.files) }} files</span>
            </div>
            <div class="bar sm"><div :style="{ width: (read?.fraction ?? 0) * 100 + '%' }"></div></div>
            <div class="mono mu sm" style="display: flex; justify-content: flex-end">
              {{ read && read.filesDone === read.files && read.files > 0 ? 'Saving…' : read?.eta != null ? about(read.eta) + ' left' : 'Starting…' }}
            </div>
          </div>
          <p class="mu sm sep">Nothing on the card is changed. You only wait for this the first time.</p>
        </template>

        <template v-else>
          <div class="ident">
            <div class="sd"></div>
            <div>
              <div class="cname">{{ card!.name }}</div>
              <div class="mono mu sm">{{ card!.path }} · SD card</div>
            </div>
          </div>
          <div v-if="card!.capacity" class="bar usage8"><div :style="{ width: usedPct + '%' }"></div></div>
          <div class="stats">
            <template v-if="card!.capacity">
              <div><div class="k">Capacity</div><div class="v mono">{{ gb(card!.capacity) }}</div></div>
              <div><div class="k">Free</div><div class="v mono">{{ gb(card!.free) }}</div></div>
            </template>
            <div><div class="k">Tracks</div><div class="v mono">{{ num(ready!.tracks) }}</div></div>
            <div><div class="k">Albums</div><div class="v mono">{{ num(ready!.albums) }}</div></div>
          </div>
          <div class="sep mu" style="display: flex; align-items: center; gap: 8px; font-size: 13px">
            <span class="dot" style="background: var(--grn)"></span>Ready. {{ lastSynced }}
          </div>
        </template>

        <div v-if="store.cards?.others.length" class="others">
          <div class="mu sm">Other cards plugged in</div>
          <div v-for="o in store.cards.others" :key="o.path" class="other">
            <span class="mono sm ellipsis" style="flex: 1">{{ o.name }} · {{ o.path }}</span>
            <button class="link sm" @click="useCard(o.path)">Use this card</button>
          </div>
        </div>
      </section>

      <div style="display: flex; flex-direction: column; gap: 20px">
        <section class="panel" style="gap: 14px">
          <div class="label">Music folders on this computer</div>
          <div v-if="store.settings">
            <div v-for="l in store.settings.libraries" :key="l.path" class="lib">
              <div style="flex: 1; min-width: 0">
                <div class="mono ellipsis" style="font-size: 14px">{{ l.path }}</div>
                <div class="mu sm" style="margin-top: 2px" :style="{ color: l.exists ? '' : 'var(--red)' }">
                  {{ libLine(l) }}
                </div>
              </div>
              <button class="quiet" @click="removeLibrary(l.path)">Remove</button>
            </div>
            <p v-if="!store.settings.libraries.length" class="mu" style="margin: 0 0 4px; font-size: 14px">
              Add the folder your music lives in. You can add several.
            </p>
          </div>
          <div><button class="btn xs" @click="addLibrary">Add folder…</button></div>
          <div class="sep" style="display: flex; flex-direction: column; gap: 10px; padding-top: 12px">
            <button class="disclosure" @click="adv = !adv">
              <span class="arrow">{{ adv ? '▾' : '▸' }}</span>Advanced: folders to leave out ({{
                store.settings?.exclude.length ?? 0
              }})
            </button>
            <template v-if="adv && store.settings">
              <div v-for="e in store.settings.exclude" :key="e" class="excl">
                <div class="mono" style="flex: 1; font-size: 13px">{{ e }}</div>
                <button class="quiet" style="height: auto; padding: 0" @click="removeExclude(e)">Remove</button>
              </div>
              <div><button class="link sm" @click="addExclude">Leave out a folder…</button></div>
            </template>
          </div>
        </section>

        <section class="panel" style="gap: 14px">
          <div class="label">Podcasts folder on this computer</div>
          <div v-if="pod" class="lib">
            <div style="flex: 1; min-width: 0">
              <div class="mono ellipsis" style="font-size: 14px">{{ pod.path }}</div>
              <div v-if="podLine" class="mu sm" style="margin-top: 2px" :style="{ color: pod.exists ? '' : 'var(--red)' }">
                {{ podLine }}
              </div>
            </div>
            <button class="quiet" @click="stopPodcasts">Stop syncing</button>
          </div>
          <p class="mu sm" style="margin: 0">
            Copied to the card's Podcasts folder as it is laid out (publisher, show, series). Episodes you delete here
            are deleted from the card too; what you listened to is kept.
          </p>
          <div>
            <button class="btn xs" @click="choosePodcasts">{{ pod ? 'Change folder…' : 'Choose folder…' }}</button>
          </div>
        </section>
      </div>
    </div>
    <div class="mu" style="font-size: 13px" v-if="store.settings">
      This computer is called <span class="mono" style="color: var(--tx)">{{ store.settings.machine }}</span>. Albums
      synced from here are marked "from {{ store.settings.machine }}".
      <button class="link" @click="emit('rename')">Rename</button>
    </div>
  </div>
</template>

<style scoped>
.cols {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
  gap: 20px;
  align-items: start;
}
.dashed {
  border: 1.5px dashed var(--ln2);
  border-radius: 10px;
  padding: 30px 24px;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 10px;
}
.dashed p {
  margin: 0;
  font-size: 14px;
  line-height: 1.5;
  text-wrap: pretty;
}
.watch {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: var(--mu);
}
.ident {
  display: flex;
  gap: 16px;
  align-items: center;
}
.sd {
  width: 40px;
  height: 52px;
  border: 2px solid var(--ln2);
  border-radius: 4px 12px 4px 4px;
  flex: none;
}
.cname {
  font-size: 21px;
  font-weight: 600;
}
.sm {
  font-size: 13px;
}
.stats {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 12px;
}
.stats .k {
  font-size: 12px;
  color: var(--mu);
}
.stats .v {
  font-size: 20px;
  font-weight: 500;
}
.sep {
  border-top: 1px solid var(--ln);
  padding-top: 14px;
  margin: 0;
  line-height: 1.5;
}
.lib {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 0;
  border-bottom: 1px solid var(--ln);
}
.excl {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 8px 12px;
  background: var(--sf2);
  border-radius: 8px;
}
.others {
  border-top: 1px solid var(--ln);
  padding-top: 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.other {
  display: flex;
  gap: 10px;
  align-items: center;
}
</style>
