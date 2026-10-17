<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import { api, type AlbumKey } from '../api';
import { num, when } from '../format';
import { choosePodcasts, installOnCard, loadReport, saveSettings, stopPodcasts, store } from '../store';

const props = defineProps<{ focusName?: number }>();
const name = ref(store.settings?.machine ?? '');
const nameInput = ref<HTMLInputElement | null>(null);
watch(
  () => store.settings?.machine,
  (m) => (name.value = m ?? ''),
);
watch(
  () => props.focusName,
  () => nameInput.value?.focus(),
);
onMounted(() => {
  void loadReport();
  if (props.focusName) nameInput.value?.focus();
});

const r = computed(() => store.report);
const s = computed(() => store.settings);
function saveName() {
  const v = name.value.trim();
  if (v && v !== s.value?.machine) void saveSettings({ machine: v });
}
const folders = computed(() => {
  const st = s.value;
  if (!st) return '';
  const libs = st.libraries.map((l) => l.path).join(', ') || 'none yet';
  return st.exclude.length ? `${libs} · ${st.exclude.length} left out` : libs;
});
const podcasts = computed(() => {
  const p = s.value?.podcasts;
  if (!p) return 'Not synced';
  if (!p.exists) return `${p.path} · not found`;
  return api.desktop ? `${p.path} · ${num(p.episodes)} episodes in ${num(p.shows)} folders` : p.path;
});
const pr = computed(() => r.value?.podcasts);
const setKey = (k: AlbumKey) => void saveSettings({ albumKey: k });
const skipped = computed(() => [...(r.value?.skipped ?? []), ...(r.value?.failed ?? [])]);
</script>

<template>
  <ScreenHeader title="Report &amp; settings" sub="What happened in the last sync, and how this computer syncs." />
  <div class="body">
    <div class="cols">
      <section class="panel">
        <div style="display: flex; justify-content: space-between; align-items: baseline">
          <div class="label">Last sync</div>
          <span v-if="r" class="mono mu" style="font-size: 12px">{{ when(r.at) }} · {{ r.machine }}</span>
        </div>
        <template v-if="r">
          <div class="group">
            <div style="font-weight: 600">
              Converted for the player <span class="mono" style="color: var(--amb); font-weight: 500">{{ num(r.converted) }}</span>
            </div>
            <div v-for="[k, n] in r.conversions" :key="k" class="line">
              <span>{{ k }}</span><span class="mono mu">{{ num(n) }}</span>
            </div>
            <div v-if="!r.conversions.length" class="mu" style="font-size: 13px">Everything was copied as it is.</div>
          </div>
          <div class="group sep">
            <div style="font-weight: 600">
              Skipped <span class="mono mu" style="font-weight: 500">{{ num(skipped.length) }}</span>
            </div>
            <div v-for="k in skipped" :key="k.path" style="font-size: 13px; line-height: 1.5; overflow-wrap: anywhere">
              <span class="mono">{{ k.path }}</span> <span class="mu">· {{ k.reason }}</span>
            </div>
            <div v-if="!skipped.length" class="mu" style="font-size: 13px">Nothing.</div>
          </div>
          <div v-if="pr" class="group sep">
            <div style="font-weight: 600">Podcasts</div>
            <div class="line"><span>Copied</span><span class="mono mu">{{ num(pr.copied) }}</span></div>
            <div class="line"><span>Moved on the card</span><span class="mono mu">{{ num(pr.moved) }}</span></div>
            <div class="line"><span>Removed (gone from the folder)</span><span class="mono mu">{{ num(pr.deleted) }}</span></div>
            <div class="mu" style="font-size: 13px">
              {{ num(pr.episodes) }} episodes on the card, {{ num(pr.listened) }} started or played.
            </div>
            <div v-for="k in [...pr.failed, ...pr.skipped]" :key="k.path" style="font-size: 13px; line-height: 1.5; overflow-wrap: anywhere">
              <span class="mono">{{ k.path }}</span> <span class="mu">· {{ k.reason }}</span>
            </div>
          </div>
          <div class="group sep">
            <div style="font-weight: 600">
              Tags guessed from folders <span class="mono mu" style="font-weight: 500">{{ num(r.tagsGuessed.length) }}</span>
            </div>
            <div class="mono" style="font-size: 13px; line-height: 1.7; overflow-wrap: anywhere">
              <div v-for="t in r.tagsGuessed" :key="t">{{ t }}</div>
            </div>
            <div class="mu" style="font-size: 13px; line-height: 1.5">
              {{ r.tagsGuessed.length ? 'Fix the tags in your music library, then sync again.' : 'Every track had its own tags.' }}
            </div>
          </div>
        </template>
        <div v-else class="mu" style="font-size: 14px">No sync has written to this card yet.</div>
      </section>

      <div style="display: flex; flex-direction: column; gap: 20px">
        <section class="panel" style="gap: 18px">
          <div class="label">Settings</div>
          <label style="display: flex; flex-direction: column; gap: 6px">
            <span style="font-weight: 600; font-size: 14px">This computer's name</span>
            <input
              ref="nameInput"
              v-model="name"
              class="input mono"
              style="max-width: 260px; font-size: 14px"
              @blur="saveName"
              @keydown.enter="($event.target as HTMLInputElement).blur()"
            />
            <span class="mu" style="font-size: 13px">Shown on albums as "from {{ name || s?.hostname }}".</span>
          </label>
          <div class="sep" style="display: flex; justify-content: space-between; align-items: center; gap: 12px; padding-top: 16px">
            <div style="min-width: 0">
              <div style="font-weight: 600; font-size: 14px">Music folders</div>
              <div class="mono mu ellipsis" style="font-size: 13px">{{ folders }}</div>
            </div>
            <button class="link" style="font-size: 14px; white-space: nowrap" @click="store.screen = 'setup'">Change…</button>
          </div>
          <div class="sep" style="display: flex; justify-content: space-between; align-items: center; gap: 12px; padding-top: 16px">
            <div style="min-width: 0">
              <div style="font-weight: 600; font-size: 14px">Podcasts folder</div>
              <div class="mono mu ellipsis" style="font-size: 13px">{{ podcasts }}</div>
              <div class="mu" style="font-size: 13px; line-height: 1.5; margin-top: 3px">
                Copied to the card's Podcasts folder as it is laid out (publisher, show, series). Episodes you delete
                here are deleted from the card too; what you listened to is kept.
              </div>
            </div>
            <div style="display: flex; flex-direction: column; gap: 4px; align-items: flex-end">
              <button class="link" style="font-size: 14px; white-space: nowrap" @click="choosePodcasts">
                {{ s?.podcasts ? 'Change…' : 'Choose…' }}
              </button>
              <button v-if="s?.podcasts" class="link" style="font-size: 13px; white-space: nowrap" @click="stopPodcasts">
                Stop syncing
              </button>
            </div>
          </div>
          <div class="sep" style="display: flex; flex-direction: column; gap: 8px; padding-top: 16px">
            <div style="font-weight: 600; font-size: 14px">
              Album grouping <span class="mu" style="font-weight: 400; font-size: 13px">· advanced</span>
            </div>
            <button class="radio" @click="setKey('artist-album')">
              <span class="ring" :class="{ on: s?.albumKey !== 'name' }"></span>Album artist + album
              <span class="mu">(recommended)</span>
            </button>
            <button class="radio" @click="setKey('name')">
              <span class="ring" :class="{ on: s?.albumKey === 'name' }"></span>Album name only
            </button>
          </div>
          <div v-if="api.desktop" class="sep" style="display: flex; flex-direction: column; gap: 6px; padding-top: 16px">
            <div style="display: flex; gap: 10px; align-items: center">
              <span style="font-weight: 600; font-size: 14px">ffmpeg</span>
              <span v-if="s?.ffmpeg" class="badge new">found · {{ s.ffmpeg }}</span>
              <span v-else class="badge tags">not found</span>
            </div>
            <div class="mu" style="font-size: 13px; line-height: 1.5">
              Lets the app convert AAC, Ogg and Opus files to MP3. Without it, those files are skipped.
            </div>
          </div>
        </section>
        <section v-if="api.desktop" class="install">
          <div style="flex: 1; min-width: 240px">
            <div style="font-weight: 600">Put the app on the card</div>
            <div class="mu" style="font-size: 13px; line-height: 1.5; margin-top: 3px">
              Copies opentone Sync onto the card, so the card carries its own sync tool to any computer.
            </div>
          </div>
          <button class="btn sm" @click="installOnCard">Put the app on the card</button>
        </section>
      </div>
    </div>
  </div>
</template>

<style scoped>
.cols {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(360px, 1fr));
  gap: 20px;
  align-items: start;
}
.group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.sep {
  border-top: 1px solid var(--ln);
  padding-top: 14px;
}
.line {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
  gap: 12px;
}
.radio {
  display: flex;
  gap: 10px;
  align-items: center;
  font-size: 14px;
  background: none;
  border: none;
  padding: 0;
  cursor: pointer;
  text-align: left;
}
.radio .ring {
  width: 16px;
  height: 16px;
  border: 1.5px solid var(--ln2);
}
.radio .ring.on {
  border: 5px solid var(--pri);
}
.install {
  background: var(--sf2);
  border-radius: 12px;
  padding: 20px 22px;
  display: flex;
  gap: 16px;
  align-items: center;
  flex-wrap: wrap;
}
</style>
