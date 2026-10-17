<script setup lang="ts">
// Tools → opentone Sync: the app (syncer/app, built into public/sync-app by
// its `npm run build:web`) fills the whole page under the nav bar, with
// its own sidebar, following the site's dark/light toggle.
import { computed } from "vue";
import { useData, withBase } from "vitepress";

const { isDark } = useData();
const src = computed(() => withBase(`/sync-app/index.html?theme=${isDark.value ? "dark" : "light"}`));
</script>

<template>
  <ClientOnly>
    <iframe class="SyncLayout" :src="src" title="opentone Sync" />
  </ClientOnly>
</template>

<style scoped>
.SyncLayout {
  display: block;
  width: 100%;
  height: calc(100vh - var(--vp-nav-height));
  border: none;
}
</style>
