<template>
  <figure class="lb">
    <header class="lb-head">
      <div class="lb-titles">
        <div class="lb-title">Loop breakdown</div>
        <div class="lb-sub">Where main-loop time goes, averaged over the last {{ seconds }} s</div>
      </div>
      <slot name="units" />
    </header>
    <template v-if="parts">
      <div class="lb-bar" role="img" :aria-label="ariaLabel" @mouseleave="hot = null">
        <span
          v-for="p in parts.filter((p) => p.pct > 0)"
          :key="p.key"
          class="lb-seg"
          :class="{ dim: hot && hot !== p.key }"
          :style="{ flexGrow: p.pct, background: p.color }"
          @mouseenter="hot = p.key"
        />
      </div>
      <ul class="lb-legend">
        <li
          v-for="p in parts"
          :key="p.key"
          :class="{ dim: hot && hot !== p.key }"
          :title="p.help"
          @mouseenter="hot = p.key"
          @mouseleave="hot = null"
        >
          <i :style="{ background: p.color }" />
          <span>{{ p.name }}</span>
          <b>{{ value(p) }}</b>
        </li>
      </ul>
      <p class="lb-note">
        Feeder interrupts land inside these steps and are counted in them ({{ feederPct.toFixed(1) }} % in all).
        <b>bench</b> is the console, the bench's answers and the once-a-second monitor lines.
      </p>
    </template>
    <p v-else class="lb-note">
      {{ samples.length ? "This firmware doesn't time the loop's steps: flash the current bench firmware." : "Waiting for the profiler…" }}
    </p>
  </figure>
</template>

<script setup lang="ts">
import { computed, ref } from "vue";
import type { ProfSample } from "@bench/device.ts";
import { FRAME_MS } from "./format.ts";

const props = defineProps<{ samples: ProfSample[]; unit: "pct" | "us" | "frame" }>();
const hot = ref<string | null>(null);

// Fixed order and colors: ui and audio keep their CPU-chart colors.
const STEPS = [
  { key: "audio", name: "audio", color: "var(--bl-brand)", help: "player.loop(): SD and PSRAM reads, VS1053 feeding" },
  { key: "wheel", name: "wheel", color: "var(--bl-brand2)", help: "wheel.update(): MPR121 over I2C, holds the SPI1 lock" },
  { key: "ui", name: "UI", color: "var(--bl-sgreen)", help: "ui.update(): drawing, animations, screen logic" },
  { key: "input", name: "input", color: "var(--bl-text)", help: "wheel events, seek/scroll timeouts, idle check" },
  { key: "bench", name: "bench", color: "var(--bl-muted)", help: "console, bench answers, profiler and monitor lines" },
] as const;

const seconds = computed(() => props.samples.length);
const feederPct = computed(() => share("isr"));

function sum(key: string) {
  return props.samples.reduce((a, s) => a + (s.busyMs[key] ?? 0), 0);
}
function share(key: string) {
  const total = props.samples.reduce((a, s) => a + s.windowMs, 0);
  return total ? (100 * sum(key)) / total : 0;
}
/** Main-loop passes per second. */
const loopsPerS = computed(() => {
  const ms = props.samples.reduce((a, s) => a + s.windowMs, 0);
  return ms ? (1000 * props.samples.reduce((a, s) => a + s.loops, 0)) / ms : 0;
});

const parts = computed(() => {
  const n = props.samples.length;
  if (!n || props.samples.every((s) => s.busyMs.loop === undefined)) return null;
  const total = props.samples.reduce((a, s) => a + s.windowMs, 0);
  const perS = (ms: number) => (1000 * ms) / total;
  const steps = STEPS.map((s) => ({ ...s, ms: perS(sum(s.key)) }));
  const other = perS(sum("bt") + sum("misc"));
  const timed = steps.reduce((a, s) => a + s.ms, 0) + other;
  // Loop time no span covers, plus time between loop() calls (core, serial events).
  const untimed = Math.max(0, 1000 - timed);
  return [
    ...steps,
    { key: "other", name: "BT, misc", color: "var(--bl-line2)", help: "bluetooth.loop(), progress updates, telemetry", ms: other },
    { key: "untimed", name: "untimed", color: "var(--bl-surface2)", help: "inside loop() but outside every step, or between loop() calls", ms: untimed },
  ].map((p) => ({ ...p, pct: p.ms / 10 }));
});

/** ms per second → the chosen unit. */
function value(p: { ms: number; pct: number }) {
  if (props.unit === "pct") return `${p.pct.toFixed(1)}%`;
  const us = loopsPerS.value ? (1000 * p.ms) / loopsPerS.value : 0;
  if (props.unit === "us") return `${Math.round(us).toLocaleString("en-US")} µs`;
  return `${((us / (FRAME_MS * 1000)) * 100).toFixed(1)}%`;
}

const ariaLabel = computed(
  () => `Main loop: ${parts.value?.map((p) => `${p.name} ${p.pct.toFixed(1)} percent`).join(", ") ?? ""}`,
);
</script>

<style scoped>
.lb {
  display: flex;
  flex-direction: column;
  gap: 14px;
  margin: 0;
  padding: 18px 20px;
  border: 1px solid var(--bl-line);
  border-radius: 12px;
  background: var(--bl-surface);
  min-width: 0;
}
.lb-head {
  display: flex;
  align-items: flex-start;
  flex-wrap: wrap;
  gap: 12px;
}
.lb-titles {
  flex: 1;
  min-width: 200px;
}
.lb-title {
  font-size: 15px;
  font-weight: 600;
}
.lb-sub {
  margin-top: 2px;
  font-size: 12px;
  color: var(--bl-muted);
}
.lb-bar {
  display: flex;
  gap: 2px;
  height: 14px;
  border-radius: 7px;
  overflow: hidden;
}
.lb-seg {
  flex-basis: 0;
  min-width: 2px;
  transition: opacity 0.15s;
}
.lb-seg.dim,
.lb-legend li.dim {
  opacity: 0.35;
}
.lb-legend {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 22px;
  margin: 0;
  padding: 0;
  list-style: none;
}
.lb-legend li {
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 12px;
  cursor: default;
}
.lb-legend i {
  width: 10px;
  height: 10px;
  border-radius: 3px;
  flex: none;
}
.lb-legend span {
  color: var(--bl-muted);
}
.lb-legend b {
  font-family: var(--bl-mono);
  font-weight: 400;
}
.lb-note {
  margin: 0;
  font-size: 12px;
  color: var(--bl-muted);
}
.lb-note b {
  font-weight: 600;
  color: var(--bl-text);
}
</style>
