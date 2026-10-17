<template>
  <header ref="statusEl" class="bl-status">
    <div class="bl-brand">
      <img :src="withBase('/opentone_logo.svg')" alt="" />
      <span><b>opentone</b> <span class="bl-muted">BenchLive</span></span>
    </div>
    <span class="bl-divider" />
    <template v-if="!supported">
      <span class="bl-live"><i />Unavailable in this browser</span>
    </template>
    <template v-else-if="!device">
      <button class="bl-primary" :disabled="connecting" @click="connect()">
        {{ connecting ? "Connecting…" : "Connect to the Nucleo" }}
      </button>
      <span class="bl-small bl-muted">Flash the bench firmware first: <code class="bl-mono">pio run -e bench -t upload</code></span>
    </template>
    <template v-else>
      <span class="bl-live" :class="{ on: alive }">
        <i /><b>{{ alive ? "Live" : "No data for 3 s" }}</b>
        <span class="bl-mono bl-muted">{{ portLabel }}</span>
      </span>
      <span v-if="info.rev" class="bl-small bl-muted">firmware <span class="bl-mono bl-text">{{ info.rev }}</span></span>
    </template>
    <span class="bl-spacer" />
    <div v-if="device" class="bl-window">
      <span class="bl-small bl-muted">Window</span>
      <Seg v-model="windowS" :options="WINDOWS" label="Time window" size="md" />
    </div>
    <div class="bl-actions">
      <button class="bl-ghost" @click="isDark = !isDark">{{ isDark ? "Light mode" : "Dark mode" }}</button>
      <template v-if="device">
        <button class="bl-ghost" @click="clearCharts">Clear</button>
        <button class="bl-ghost" @click="disconnect">Disconnect</button>
      </template>
    </div>
  </header>
</template>

<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from "vue";
import { useData, withBase } from "vitepress";
import Seg from "../Seg.vue";
import { useBenchLive } from "./state.ts";

const emit = defineEmits<{ height: [px: number] }>();

const { isDark } = useData();
const { supported, device, connecting, connect, disconnect, alive, portLabel, info, windowS, clearCharts } = useBenchLive();

const WINDOWS = [
  { value: 30, label: "30 s" },
  { value: 60, label: "1 min" },
  { value: 300, label: "5 min" },
];

// The bar wraps on narrow screens: its height lets the remote stick under it.
const statusEl = ref<HTMLElement | null>(null);
let statusObserver: ResizeObserver | null = null;
onMounted(() => {
  statusObserver = new ResizeObserver(([e]) => emit("height", e.borderBoxSize[0].blockSize));
  if (statusEl.value) statusObserver.observe(statusEl.value);
});
onBeforeUnmount(() => statusObserver?.disconnect());
</script>

<style scoped>
/* --- Status bar ---------------------------------------------------------- */
.bl-status {
  position: sticky;
  top: var(--vp-nav-height, 64px);
  z-index: 10;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px 18px;
  padding: 12px 32px;
  background: var(--bl-chrome);
  border-top: 1px solid var(--bl-line);
  border-bottom: 1px solid var(--bl-line);
}
.bl-brand {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 15px;
}
.bl-brand img {
  width: 22px;
  height: 22px;
  border-radius: 6px;
}
.bl-brand b {
  font-weight: 700;
}
.bl-divider {
  width: 1px;
  height: 22px;
  background: var(--bl-line);
}
.bl-live {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
}
.bl-live b {
  font-weight: 600;
}
.bl-live .bl-mono {
  font-size: 12px;
}
.bl-live i {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--bl-muted);
}
.bl-live.on i {
  background: var(--bl-green);
}
.bl-window,
.bl-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

@container (max-width: 640px) {
  .bl-status {
    padding-left: 16px;
    padding-right: 16px;
  }
}
</style>
