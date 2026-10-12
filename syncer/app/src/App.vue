<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import Sidebar from './components/Sidebar.vue';
import TopBar from './components/TopBar.vue';
import { api, unsupportedBrowser } from './api';
import { check, store } from './store';
import GrantView from './views/GrantView.vue';
import UnsupportedView from './views/UnsupportedView.vue';
import LibraryView from './views/LibraryView.vue';
import SettingsView from './views/SettingsView.vue';
import SetupView from './views/SetupView.vue';
import SyncView from './views/SyncView.vue';
import WhatsNewView from './views/WhatsNewView.vue';

/** `embed`: inside the docs page (top bar without the brand, bordered). */
const props = defineProps<{ embed?: boolean }>();
const width = ref(window.innerWidth);
const onResize = () => (width.value = window.innerWidth);
onMounted(() => window.addEventListener('resize', onResize));
onBeforeUnmount(() => window.removeEventListener('resize', onResize));

const layout = computed(() => (props.embed ? 'embed' : width.value >= 1100 ? 'wide' : 'narrow'));
const focusName = ref(0);
/** Web: the music folder and card still need choosing (or permission);
 * null until the browser has answered. */
const needsAccess = ref<boolean | null>(api.desktop ? false : null);
onMounted(async () => {
  const a = await api.access();
  needsAccess.value = !!a && (!a.music.length || !a.card || a.music.some((m) => !m.granted) || !a.card.granted);
});
function accessGiven() {
  needsAccess.value = false;
  void check();
}
function rename() {
  store.screen = 'settings';
  focusName.value++;
}
</script>

<template>
  <div class="app" :class="layout">
    <Sidebar v-if="layout === 'wide' && !unsupportedBrowser" @rename="rename" />
    <div class="main">
      <TopBar v-if="layout !== 'wide' && !unsupportedBrowser" :brand="layout === 'narrow'" />
      <UnsupportedView v-if="unsupportedBrowser" />
      <div v-else-if="needsAccess === null" class="body"></div>
      <GrantView v-else-if="needsAccess" @done="accessGiven" />
      <SetupView v-else-if="store.screen === 'setup'" @rename="rename" />
      <WhatsNewView v-else-if="store.screen === 'whatsnew'" />
      <SyncView v-else-if="store.screen === 'syncing'" />
      <LibraryView v-else-if="store.screen === 'library'" />
      <SettingsView v-else :focus-name="focusName" />
      <div v-if="store.toast" class="toast" role="status">{{ store.toast }}</div>
    </div>
  </div>
</template>

<style scoped>
.app {
  width: 100%;
  height: 100%;
  display: flex;
  background: var(--bg);
  color: var(--tx);
  overflow: hidden;
  position: relative;
}
.app.narrow,
.app.embed {
  flex-direction: column;
}
.app.embed {
  border: 1px solid var(--ln);
  border-radius: 12px;
}
.main {
  flex: 1;
  min-width: 0;
  min-height: 0;
  display: flex;
  flex-direction: column;
  position: relative;
}
.toast {
  position: absolute;
  left: 50%;
  bottom: 24px;
  transform: translateX(-50%);
  max-width: min(560px, calc(100% - 48px));
  background: var(--tx);
  color: var(--bg);
  padding: 10px 16px;
  border-radius: 10px;
  font-size: 14px;
  box-shadow: var(--shadow);
  z-index: 20;
}
</style>
