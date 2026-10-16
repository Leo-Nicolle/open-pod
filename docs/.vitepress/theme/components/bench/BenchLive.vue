<template>
  <div class="bl vp-raw" :style="{ '--bl-status-h': `${statusH}px` }">
    <StatusBar @height="statusH = $event" />

    <div class="bl-page">
      <div v-if="!supported" class="bl-note red">
        <b>Web Serial isn't available.</b> This page talks to the Nucleo over Web Serial, which only Chrome and Edge
        have (on desktop). <a href="?demo">Try the demo</a> instead.
      </div>

      <ErrorList />

      <template v-if="device">
        <KpiRow />

        <!-- Tabs -->
        <div class="bl-tabs" role="tablist">
          <button
            v-for="t in tabs"
            :key="t.id"
            role="tab"
            :aria-selected="tab === t.id"
            :class="{ on: tab === t.id }"
            @click="tab = t.id"
          >
            {{ t.label }}<span v-if="t.count" class="bl-tab-count">{{ t.count }}</span>
          </button>
        </div>

        <div v-if="tab === 'live'" class="bl-live-grid">
          <RemotePanel />
          <LiveCharts />
        </div>

        <div v-else-if="tab === 'run'" class="bl-stack">
          <ScenarioRunner />
          <ResultsPanel v-if="report" :report="report" />
          <E2ETests />
        </div>

        <SerialLog v-else />
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
// Shared styles first: the sections' scoped styles must come after them.
import "./live/bench-live.css";
import { computed, onBeforeUnmount, onMounted, provide, ref } from "vue";
import { webSerialSupported } from "./web-serial.ts";
import { BENCH_LIVE, createBenchLive } from "./live/state.ts";
import StatusBar from "./live/StatusBar.vue";
import ErrorList from "./live/ErrorList.vue";
import KpiRow from "./live/KpiRow.vue";
import RemotePanel from "./live/RemotePanel.vue";
import LiveCharts from "./live/LiveCharts.vue";
import ScenarioRunner from "./live/ScenarioRunner.vue";
import ResultsPanel from "./live/ResultsPanel.vue";
import E2ETests from "./live/E2ETests.vue";
import SerialLog from "./live/SerialLog.vue";

const ctx = createBenchLive();
provide(BENCH_LIVE, ctx);
const { supported, device, tab, busy, running, testing, current, report, logLines, restore, connect, disconnect, press, flash } = ctx;

// The status bar's height (it wraps on narrow screens), so the remote can stick under it.
const statusH = ref(60);

const tabs = computed(() => [
  { id: "live" as const, label: "Live", count: "" },
  {
    id: "run" as const,
    label: "Run the bench",
    count: running.value && current.value ? `${current.value.index + 1}/${current.value.total}` : testing.value ? "e2e" : "",
  },
  { id: "log" as const, label: "Serial log", count: String(logLines.value.length) },
]);

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

// Keyboard shortcuts for the remote, on every tab.
function onKey(e: KeyboardEvent) {
  if (!device.value || busy.value) return;
  const el = e.target as HTMLElement;
  if (el && (el.tagName === "INPUT" || el.tagName === "SELECT" || el.tagName === "TEXTAREA" || el.tagName === "BUTTON")) return;
  const keys: Record<string, [string, string]> = {
    ArrowUp: ["scroll", e.shiftKey ? "24" : "1"],
    ArrowDown: ["scroll", e.shiftKey ? "-24" : "-1"],
    ArrowLeft: ["click", "prev"],
    ArrowRight: ["click", "next"],
    Enter: ["click", "select"],
    Escape: ["click", "menu"],
    " ": ["click", "play"],
  };
  const k = keys[e.key];
  if (!k) return;
  e.preventDefault();
  if (k[0] === "click") flash(k[1]);
  void press(...k);
}
</script>

<style scoped>
/* Tokens, base and shared bits: live/bench-live.css. */

/* --- Page ------------------------------------------------------------------- */
.bl-page {
  display: flex;
  flex-direction: column;
  gap: 20px;
  max-width: 1720px;
  margin: 0 auto;
  padding: 24px 32px 56px;
  box-sizing: border-box;
}
.bl-stack {
  display: flex;
  flex-direction: column;
  gap: 28px;
}

/* Tabs */
.bl-tabs {
  display: flex;
  gap: 24px;
  border-bottom: 1px solid var(--bl-line);
  overflow-x: auto;
}
.bl-tabs button {
  display: flex;
  align-items: center;
  gap: 8px;
  height: 42px;
  padding: 0 2px;
  border: 0;
  background: transparent;
  color: var(--bl-muted);
  font-size: 14px;
  font-weight: 600;
  white-space: nowrap;
  cursor: pointer;
}
.bl-tabs button.on {
  color: var(--bl-text);
  box-shadow: inset 0 -2px 0 var(--bl-brand);
}
.bl-tab-count {
  font-family: var(--bl-mono);
  font-size: 11px;
  font-weight: 500;
  color: var(--bl-muted);
}

/* Live tab */
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
