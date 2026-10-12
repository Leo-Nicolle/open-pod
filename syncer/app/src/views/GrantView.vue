<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import ScreenHeader from '../components/ScreenHeader.vue';
import { api, type Access } from '../api';
import { refreshCards, loadSettings, store } from '../store';

const emit = defineEmits<{ done: [] }>();
const access = ref<Access | null>(null);
onMounted(async () => (access.value = await api.access()));

const musicOk = computed(() => !!access.value?.music.length && access.value.music.every((m) => m.granted));
const cardOk = computed(() => !!access.value?.card?.granted);
/** Folders chosen on an earlier visit whose permission lapsed. */
const lapsed = computed(
  () => !!access.value && (access.value.music.some((m) => !m.granted) || (!!access.value.card && !access.value.card.granted)),
);

async function after() {
  access.value = await api.access();
  await loadSettings();
  await refreshCards();
  if (musicOk.value && cardOk.value) emit('done');
}
async function addMusic() {
  const name = await api.pickFolder('Choose your music folder');
  if (name && store.settings) await api.saveSettings({ libraries: [...store.settings.libraries.map((l) => l.path), name] });
  await after();
}
async function chooseCard() {
  await api.pickFolder("Choose the SD card's top folder", undefined, true);
  await after();
}
async function allow() {
  access.value = await api.grantAccess();
  await after();
}
</script>

<template>
  <ScreenHeader
    title="Give access to your folders"
    sub="The browser needs your permission to read your music and write to the card."
  >
    <button v-if="lapsed" class="btn primary" @click="allow">Allow again</button>
    <button v-else class="btn primary" :disabled="!(musicOk && cardOk)" @click="emit('done')">Check what's new</button>
  </ScreenHeader>
  <div class="body">
    <div class="steps">
      <div class="step">
        <span class="num" :class="{ ok: musicOk }">1</span>
        <div style="flex: 1; min-width: 200px">
          <div style="font-weight: 600">Music folder</div>
          <div v-if="access?.music.length" class="mu" style="font-size: 13px; margin-top: 2px">
            <template v-for="(m, i) in access.music" :key="m.name">
              <template v-if="i">, </template>
              {{ m.granted ? 'Access given:' : 'Needs permission again:' }}
              <span class="mono" style="color: var(--tx)">{{ m.name }}</span>
            </template>
          </div>
          <div v-else class="mu" style="font-size: 13px; margin-top: 2px">The folder your music lives in. You can add several.</div>
        </div>
        <button v-if="access?.music.length" class="link sm" @click="addMusic">Add another folder</button>
        <button v-else class="btn primary sm" @click="addMusic">Choose the music folder…</button>
      </div>
      <div class="step" style="border-bottom: none">
        <span class="num" :class="{ ok: cardOk }">2</span>
        <div style="flex: 1; min-width: 200px">
          <div style="font-weight: 600">The card</div>
          <div class="mu" style="font-size: 13px; margin-top: 2px">
            <template v-if="access?.card">
              {{ access.card.granted ? 'Access given:' : 'Needs permission again:' }}
              <span class="mono" style="color: var(--tx)">{{ access.card.name }}</span>
            </template>
            <template v-else>
              Choose the SD card's top folder. Your browser will ask to allow changes; that's needed to copy music onto
              it.
            </template>
          </div>
        </div>
        <button
          :class="access?.card ? 'link sm' : 'btn primary sm'"
          :disabled="!access?.music.length && !access?.card"
          @click="chooseCard"
        >
          {{ access?.card ? 'Choose another card' : 'Choose the card…' }}
        </button>
      </div>
    </div>
    <p class="mu" style="margin: 0; font-size: 13px; line-height: 1.55; text-wrap: pretty">
      Everything runs in this tab. Your music is never uploaded. The browser may ask again on your next visit; your
      choices are remembered, you just confirm them.
    </p>
  </div>
</template>

<style scoped>
.steps {
  display: flex;
  flex-direction: column;
  border: 1px solid var(--ln);
  border-radius: 12px;
  background: var(--sf);
}
.step {
  display: flex;
  gap: 16px;
  align-items: center;
  padding: 20px 22px;
  border-bottom: 1px solid var(--ln);
  flex-wrap: wrap;
}
.num {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  border: 1.5px solid var(--ln2);
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: 'IBM Plex Mono', monospace;
  font-size: 13px;
  flex: none;
}
.num.ok {
  border: none;
  background: var(--grnS);
  color: var(--grn);
}
.sm {
  font-size: 13px;
}
</style>
