<template>
  <div class="bl vp-raw" :style="{ '--bl-status-h': `${statusH}px` }">
    <StatusBar @height="statusH = $event" />

    <div class="bl-page">
      <div v-if="!supported" class="bl-note red">
        <b>Web Serial isn't available.</b> This page talks to the Nucleo over Web Serial, which only Chrome and Edge
        have (on desktop). <a href="?demo">Try the demo</a> instead.
      </div>

      <ErrorList />

      <div v-if="device" class="bl-live-grid">
        <RemotePanel />
        <ScrollTuning />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
// The scroll tuning page: BenchLive's connection and remote, with the
// fast-scroll settings (#scrollcfg) instead of the charts.
// Shared styles first: the sections' scoped styles must come after them.
import "./live/bench-live.css";
import { onBeforeUnmount, onMounted, provide, ref } from "vue";
import { webSerialSupported } from "./web-serial.ts";
import { BENCH_LIVE, createBenchLive } from "./live/state.ts";
import StatusBar from "./live/StatusBar.vue";
import ErrorList from "./live/ErrorList.vue";
import RemotePanel from "./live/RemotePanel.vue";
import ScrollTuning from "./live/ScrollTuning.vue";

const ctx = createBenchLive();
provide(BENCH_LIVE, ctx);
const { supported, device, busy, restore, connect, disconnect, press } = ctx;

const statusH = ref(60);

onMounted(() => {
  restore();
  supported.value = webSerialSupported();
  if (new URLSearchParams(location.search).has("demo")) {
    supported.value = true;
    void connect(true);
  }
  window.addEventListener("keydown", onKey);
});
onBeforeUnmount(() => {
  window.removeEventListener("keydown", onKey);
  void disconnect();
});

// Arrow keys scroll, as on the live bench.
function onKey(e: KeyboardEvent) {
  if (!device.value || busy.value) return;
  const el = e.target as HTMLElement;
  if (el && ["INPUT", "SELECT", "TEXTAREA", "BUTTON"].includes(el.tagName)) return;
  if (e.key !== "ArrowUp" && e.key !== "ArrowDown") return;
  e.preventDefault();
  const n = e.shiftKey ? 24 : 1;
  void press("scroll", String(e.key === "ArrowUp" ? n : -n));
}
</script>

<style scoped>
/* Tokens, base and shared bits: live/bench-live.css. */
.bl-page {
  display: flex;
  flex-direction: column;
  gap: 20px;
  max-width: 1400px;
  margin: 0 auto;
  padding: 24px 32px 56px;
  box-sizing: border-box;
}
.bl-live-grid {
  display: grid;
  grid-template-columns: 340px minmax(0, 1fr);
  gap: 20px;
  align-items: start;
}
@container (max-width: 999px) {
  .bl-live-grid {
    grid-template-columns: minmax(0, 1fr);
  }
  /* RemotePanel's root. */
  .bl-remote {
    position: static !important;
  }
}
@container (max-width: 640px) {
  .bl-page {
    padding-left: 16px;
    padding-right: 16px;
  }
}
</style>
