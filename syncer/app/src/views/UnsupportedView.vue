<script setup lang="ts">
const RELEASES = 'https://github.com/Leo-Nicolle/open-pod/releases';
const ua = navigator.userAgent;
const os = /Mac/.test(ua) ? 'macOS' : /Win/.test(ua) ? 'Windows' : 'Linux';
const others = ['macOS', 'Windows', 'Linux'].filter((o) => o !== os);
</script>

<template>
  <div class="body">
    <div class="wrap">
      <div class="ring" style="width: 44px; height: 44px; border: 8px solid var(--pri)"></div>
      <div style="font-size: 24px; font-weight: 600; letter-spacing: -0.01em">This needs Chrome or Edge</div>
      <p class="mu" style="margin: 0; font-size: 15px; line-height: 1.55; text-wrap: pretty">
        The web version writes music straight onto your SD card, and only Chrome and Edge let a page do that. Open this
        page in one of them, or download the desktop app. It works the same and can also convert AAC, Ogg and Opus
        files.
      </p>
      <div style="display: flex; gap: 10px; flex-wrap: wrap; margin-top: 6px">
        <a class="btn primary" :href="RELEASES" style="display: inline-flex; align-items: center">Download for {{ os }}</a>
        <a v-for="o in others" :key="o" class="btn" :href="RELEASES" style="display: inline-flex; align-items: center">{{ o }}</a>
      </div>
      <div class="mono mu" style="font-size: 12px"><a :href="RELEASES">all downloads</a></div>
    </div>
  </div>
</template>

<style scoped>
.wrap {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  justify-content: center;
  gap: 14px;
  padding: 30px 12px;
  max-width: 520px;
}
</style>
