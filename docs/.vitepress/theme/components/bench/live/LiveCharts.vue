<template>
  <section class="bl-charts">
    <div class="bl-charts-head">
      <h2>Live charts</h2>
      <div class="bl-chips">
        <button
          v-for="c in CHIPS"
          :key="c.key"
          class="bl-chip"
          :class="{ on: shown[c.key] }"
          :aria-pressed="shown[c.key]"
          @click="shown[c.key] = !shown[c.key]"
        >
          {{ c.label }}
        </button>
      </div>
    </div>
    <LoopBar v-if="shown.breakdown" :samples="recentProf" :unit="units.breakdown">
      <template #units>
        <Seg v-model="units.breakdown" :options="BREAKDOWN_UNITS" label="Loop breakdown unit" size="sm" />
      </template>
    </LoopBar>
    <EventTimeline v-if="shown.events" :events="wheelEvents" :now="now" />
    <div v-if="nCharts" class="bl-chart-grid" :class="{ single: nCharts === 1 }">
      <LiveChart
        v-if="shown.runway"
        title="Buffer runway"
        sub="Seconds of decoded audio ahead of playback"
        :readout="lastAudio ? lastAudio.runwayS.toFixed(1) : '—'"
        readout-unit="s"
        :legend="[{ name: 'Runway', color: 'var(--bl-brand)', value: lastAudio ? `${lastAudio.runwayS.toFixed(2)} s` : '—' }]"
        :series="[{ name: 'runway', color: 'var(--bl-brand)', points: runway }]"
        :bands="underrunBands"
        :now="now"
        :window-s="windowS"
        :height="chartH"
        :format="(v) => `${v.toFixed(1)} s`"
        :tick="(v) => fmtN(v, 's')"
        empty="Play a track to see the buffer."
      />
      <LiveChart
        v-if="shown.cpu"
        title="CPU time"
        sub="Share of each second. SD is counted inside audio."
        :readout="cpuReadout"
        :readout-unit="units.cpu === 'ms' ? 'ms per s' : '% of 1 s'"
        :legend="cpuLegend"
        :series="cpuSeries"
        :scale="cpuScale"
        :now="now"
        :window-s="windowS"
        :height="chartH"
        :format="(v) => `${fmtN(v, cpuUnit)} ${cpuUnit}`"
        :tick="(v) => fmtN(v, cpuUnit)"
      >
        <template #units>
          <Seg v-model="units.cpu" :options="CPU_UNITS" label="CPU time unit" size="xs" />
        </template>
      </LiveChart>
      <LiveChart
        v-if="shown.loop"
        title="Longest main loop"
        sub="Worst iteration in each second"
        :readout="fmtN(lastOf(maxLoop) === null ? null : lastOf(maxLoop)! * loopUnit.k, loopUnit.u)"
        :readout-unit="loopUnit.u"
        :legend="[{ name: 'Worst in window', color: 'var(--bl-text)', value: worstLoop }]"
        :series="[{ name: 'loop', color: 'var(--bl-text)', points: maxLoop }]"
        :scale="loopUnit.k"
        :threshold="{ v: FRAME_MS * loopUnit.k, label: `frame ${fmtN(FRAME_MS * loopUnit.k, loopUnit.u)} ${loopUnit.u}` }"
        :now="now"
        :window-s="windowS"
        :height="chartH"
        :format="(v) => `${fmtN(v, loopUnit.u)} ${loopUnit.u}`"
        :tick="(v) => fmtN(v, loopUnit.u)"
      >
        <template #units>
          <Seg v-model="units.loop" :options="TIME_UNITS" label="Loop time unit" size="xs" />
        </template>
      </LiveChart>
      <LiveChart
        v-if="shown.input"
        title="Input → settled screen"
        sub="One dot per command"
        mode="dots"
        :readout="fmtN(lastOf(latency) === null ? null : lastOf(latency)! * inputUnit.k, inputUnit.u)"
        :readout-unit="inputUnit.u"
        :legend="inputLegend"
        :series="[{ name: 'latency', color: 'var(--bl-brand)', points: latency }]"
        :scale="inputUnit.k"
        :y-floor="SLOW_INPUT_MS * inputUnit.k"
        :bad-above="SLOW_INPUT_MS * inputUnit.k"
        :now="now"
        :window-s="windowS"
        :height="chartH"
        :format="(v) => `${fmtN(v, inputUnit.u)} ${inputUnit.u}`"
        :tick="(v) => fmtN(v, inputUnit.u)"
        empty="Press a wheel button to measure input latency."
      >
        <template #units>
          <Seg v-model="units.input" :options="TIME_UNITS" label="Input latency unit" size="xs" />
        </template>
      </LiveChart>
      <LiveChart
        v-if="shown.stream"
        title="Stream rate"
        sub="KB/s delivered to the decoder"
        :readout="lastAudio ? String(Math.round(lastAudio.rateKBs)) : '—'"
        readout-unit="KB/s"
        :legend="[{ name: 'Rate', color: 'var(--bl-sgreen)', value: lastAudio ? `${Math.round(lastAudio.rateKBs)} KB/s` : '—' }]"
        :series="[{ name: 'rate', color: 'var(--bl-sgreen)', points: rate }]"
        :now="now"
        :window-s="windowS"
        :height="chartH"
        :format="(v) => `${Math.round(v)} KB/s`"
        :tick="(v) => fmtN(v)"
        empty="Play a track to see the stream rate."
      />
    </div>
    <p v-if="shown.runway && bandsInWindow" class="bl-underrun-note">
      <span class="bl-band-swatch" />Red bands mark seconds where the buffer ran dry and playback glitched.
    </p>
  </section>
</template>

<script setup lang="ts">
import { computed } from "vue";
import LiveChart, { type LegendItem, type Point } from "../LiveChart.vue";
import LoopBar from "../LoopBar.vue";
import EventTimeline from "../EventTimeline.vue";
import Seg from "../Seg.vue";
import { FRAME_MS, fmtN } from "../format.ts";
import { BREAKDOWN_UNITS, CHIPS, CPU_UNITS, TIME_UNITS, timeUnit } from "./chart-prefs.ts";
import { CPU, useBenchLive } from "./state.ts";

const { shown, units, now, windowS, runway, rate, maxLoop, latency, cpu, underrunBands, lastAudio, recentProf, wheelEvents } = useBenchLive();

/** Inputs slower than this are drawn red. */
const SLOW_INPUT_MS = 100;

const nCharts = computed(() => (["runway", "cpu", "loop", "input", "stream"] as const).filter((k) => shown[k]).length);
const chartH = computed(() => (nCharts.value <= 1 ? 380 : nCharts.value <= 2 ? 300 : nCharts.value <= 4 ? 230 : 190));

const inWindow = (a: Point[]) => a.filter((p) => p.t >= now.value - windowS.value * 1000);
const lastOf = (a: Point[]) => (a.length ? a[a.length - 1].v : null);

const loopUnit = computed(() => timeUnit(units.loop));
const inputUnit = computed(() => timeUnit(units.input));

const worstLoop = computed(() => {
  const v = inWindow(maxLoop.value);
  return v.length ? `${fmtN(Math.max(...v.map((p) => p.v)) * loopUnit.value.k, loopUnit.value.u)} ${loopUnit.value.u}` : "—";
});

const inputLegend = computed<LegendItem[]>(() => {
  const v = inWindow(latency.value).map((p) => p.v);
  if (!v.length) return [];
  const s = [...v].sort((a, b) => a - b);
  const { k, u } = inputUnit.value;
  const at = (q: number) => fmtN(s[Math.min(s.length - 1, Math.floor(s.length * q))] * k, u);
  return [
    { name: "p50", color: "var(--bl-brand)", value: at(0.5), dot: true },
    { name: "p95", color: "var(--bl-brand)", value: at(0.95), dot: true },
    { name: `> ${SLOW_INPUT_MS} ms`, color: "var(--bl-red)", value: String(v.filter((x) => x > SLOW_INPUT_MS).length), dot: true },
  ];
});

const cpuScale = computed(() => (units.cpu === "ms" ? 10 : 1));
const cpuUnit = computed(() => (units.cpu === "ms" ? "ms" : "%"));
const cpuSeries = computed(() =>
  CPU.map((c) => ({
    name: c.label,
    color: c.color,
    points: cpu[c.name],
    area: "area" in c,
    dash: "dash" in c ? c.dash : undefined,
    width: "width" in c ? c.width : 2,
  })),
);
const cpuLegend = computed<LegendItem[]>(() =>
  CPU.map((c) => {
    const v = lastOf(cpu[c.name]);
    return { name: c.label, color: c.swatch, value: v === null ? "—" : fmtN(v * cpuScale.value, cpuUnit.value) };
  }),
);
const cpuReadout = computed(() => {
  const parts = ["audio", "UI", "feeder"].map((n) => lastOf(cpu[n]));
  return parts.some((v) => v === null) ? "—" : fmtN(parts.reduce((a, v) => a! + v!, 0)! * cpuScale.value, cpuUnit.value);
});

const bandsInWindow = computed(() => underrunBands.value.some(([, b]) => b >= now.value - windowS.value * 1000));
</script>

<style scoped>
/* Charts */
.bl-charts {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
}
.bl-charts-head {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 12px 20px;
}
.bl-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.bl-chip {
  height: 28px;
  padding: 0 12px;
  border: 1px solid var(--bl-line2);
  border-radius: 999px;
  background: transparent;
  color: var(--bl-muted);
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
}
.bl-chip.on {
  border-color: var(--bl-text);
  background: var(--bl-text);
  color: var(--bl-bg);
}
.bl-chart-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 420px), 1fr));
  gap: 16px;
}
.bl-chart-grid.single {
  grid-template-columns: minmax(0, 1fr);
}
.bl-underrun-note {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-band-swatch {
  width: 14px;
  height: 12px;
  border-radius: 2px;
  background: var(--bl-band);
}
</style>
