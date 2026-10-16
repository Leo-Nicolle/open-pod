<template>
  <section class="bl-panel bl-log">
    <button class="bl-log-head" :aria-expanded="logOpen" @click="logOpen = !logOpen">
      <span class="bl-muted bl-chevron">{{ logOpen ? "▾" : "▸" }}</span>
      <span class="bl-log-title">Serial log</span>
      <span class="bl-mono bl-small bl-muted">{{ shownLog.length }} lines</span>
    </button>
    <div v-if="logOpen" class="bl-log-body">
      <div class="bl-log-tools">
        <input v-model="logFilter" class="bl-mono" placeholder="Filter lines" aria-label="Filter the log" />
        <button class="bl-checkbox" role="checkbox" :aria-checked="hideMonitor" @click="hideMonitor = !hideMonitor">
          <span class="bl-check" :class="{ on: hideMonitor }" aria-hidden="true">{{ hideMonitor ? "✓" : "" }}</span>
          Hide once-a-second monitor lines
        </button>
      </div>
      <div ref="logEl" class="bl-log-lines" @scroll="onLogScroll">
        <div v-for="(l, i) in shownLog" :key="i" :class="l.kind">{{ l.text }}</div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue";
import { useBenchLive } from "./state.ts";

const { logLines, logFilter, hideMonitor, logOpen } = useBenchLive();

const LOG_SHOWN = 400;
const logEl = ref<HTMLDivElement | null>(null);
let logStick = true;

const shownLog = computed(() => {
  const f = logFilter.value.toLowerCase();
  return logLines.value
    .filter((l) => !(hideMonitor.value && l.kind === "mon") && (!f || l.text.toLowerCase().includes(f)))
    .slice(-LOG_SHOWN);
});
function onLogScroll() {
  const el = logEl.value;
  if (el) logStick = el.scrollTop + el.clientHeight >= el.scrollHeight - 8;
}
// Opening the tab scrolls to the end too.
watch(
  () => (logOpen.value ? shownLog.value : null),
  async () => {
    await nextTick();
    if (logEl.value && logStick) logEl.value.scrollTop = logEl.value.scrollHeight;
  },
  { immediate: true },
);
</script>

<style scoped>
/* Log */
.bl-log {
  overflow: hidden;
}
.bl-log-head {
  display: flex;
  align-items: center;
  gap: 12px;
  width: 100%;
  padding: 16px 22px;
  border: 0;
  background: none;
  color: var(--bl-text);
  text-align: left;
  cursor: pointer;
}
.bl-chevron {
  width: 12px;
}
.bl-log-title {
  font-size: 16px;
  font-weight: 600;
}
.bl-log-body {
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 14px 22px 18px;
  border-top: 1px solid var(--bl-line);
}
.bl-log-tools {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 16px;
}
.bl-log-tools input {
  width: 260px;
  max-width: 100%;
  height: 34px !important;
  font-size: 12px !important;
}
.bl-log-lines {
  max-height: 360px;
  overflow: auto;
  padding: 12px 14px;
  border-radius: 8px;
  background: var(--bl-bg);
  font-family: var(--bl-mono);
  font-size: 12px;
  line-height: 1.6;
  white-space: pre;
}
.bl-log-lines .mon {
  color: var(--bl-muted);
}
.bl-log-lines .cmd {
  color: var(--bl-brand);
}
.bl-log-lines .evt {
  color: var(--bl-green);
}
.bl-log-lines .bad {
  color: var(--bl-red);
}
</style>
