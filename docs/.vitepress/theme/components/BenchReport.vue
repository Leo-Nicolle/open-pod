<template>
  <div class="bench viz-root">
    <p v-if="error" class="bench-empty">{{ error }}</p>
    <p v-else-if="!index" class="bench-empty">Loading reports…</p>
    <p v-else-if="!index.runs.length" class="bench-empty">
      No bench report published yet. Run <code>npm run bench -- run</code> in <code>software/</code>.
    </p>
    <template v-else>
      <div class="bench-bar">
        <label>
          Revision
          <select v-model="selectedRev">
            <option v-for="r in [...index.runs].reverse()" :key="r.rev" :value="r.rev">
              {{ r.rev }} · {{ shortDate(r.date) }}
            </option>
          </select>
        </label>
        <span v-if="selected" class="bench-meta">
          {{ datasetLine(selected.dataset) }}
        </span>
      </div>

      <h3>Headline numbers</h3>
      <p class="bench-note" v-if="previous">
        Compared with the previous report, <code>{{ previous.rev }}</code>. Changes under 10% are noise.
      </p>
      <div class="bench-table-wrap">
        <table class="bench-table">
          <thead>
            <tr>
              <th>Metric</th>
              <th class="num">{{ selected?.rev }}</th>
              <th class="num" v-if="previous">Previous</th>
              <th v-if="previous">Change</th>
            </tr>
          </thead>
          <tbody>
            <template v-for="group in groups" :key="group">
              <tr class="bench-group">
                <td :colspan="previous ? 4 : 2">{{ group }}</td>
              </tr>
              <tr v-for="m in metricsIn(group)" :key="m.key">
                <td>{{ m.label }}</td>
                <td class="num">{{ fmt(selected?.summary[m.key], m.unit) }}</td>
                <td class="num" v-if="previous">{{ fmt(previous.summary[m.key], m.unit) }}</td>
                <td v-if="previous">
                  <span v-if="change(m)" :class="['bench-change', change(m)!.kind]">
                    {{ change(m)!.icon }} {{ change(m)!.text }}
                  </span>
                </td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>

      <template v-if="index.runs.length > 1">
        <h3>Across versions</h3>
        <div class="bench-grid">
          <figure v-for="m in index.metrics" :key="m.key" class="bench-trend">
            <figcaption>
              {{ m.label }} <span class="bench-unit">{{ unitLabel(m) }}, {{ m.better }} is better</span>
            </figcaption>
            <svg
              :viewBox="`0 0 ${W} ${H}`"
              role="img"
              :aria-label="`${m.label} per revision`"
              @mouseleave="hover = null"
            >
              <template v-for="t in trend(m)" :key="'g' + t.y">
                <line v-if="t.grid" :x1="PAD_L" :x2="W - PAD_R" :y1="t.y" :y2="t.y" class="grid" />
                <text v-if="t.grid" :x="PAD_L - 6" :y="t.y + 4" class="tick">{{ t.label }}</text>
              </template>
              <path :d="path(m)" class="line" />
              <g v-for="(p, i) in points(m)" :key="i">
                <circle :cx="p.x" :cy="p.y" r="4" :class="['dot', { on: hover?.key === m.key && hover.i === i }]" />
                <rect
                  :x="p.x - hitW(m) / 2"
                  :y="0"
                  :width="hitW(m)"
                  :height="H"
                  class="hit"
                  @mouseenter="hover = { key: m.key, i, x: p.x, y: p.y, run: p.run, v: p.v }"
                  @click="selectedRev = p.run.rev"
                />
              </g>
              <g v-if="hover?.key === m.key" class="tip" :transform="`translate(${tipX(hover.x)}, 6)`">
                <rect width="128" height="38" rx="4" />
                <text x="8" y="16" class="tip-v">{{ fmt(hover.v, m.unit) }}</text>
                <text x="8" y="31" class="tip-r">{{ hover.run.rev }}</text>
              </g>
            </svg>
          </figure>
        </div>
      </template>

      <template v-if="detail?.scenarios?.playback?.length">
        <h3>Playback, per sample</h3>
        <div class="bench-table-wrap">
          <table class="bench-table">
            <thead>
              <tr>
                <th>Sample</th>
                <th class="num">Stream</th>
                <th class="num">Start</th>
                <th class="num">Underruns</th>
                <th class="num">Min runway</th>
                <th class="num">Feeder CPU</th>
                <th class="num">SD CPU</th>
                <th class="num">Longest loop</th>
                <th class="num">Seek</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="p in detail.scenarios.playback" :key="p.sample">
                <td>{{ p.sample }}</td>
                <td class="num">{{ p.meanRateKBs ?? "-" }} KB/s</td>
                <td class="num">{{ p.firstSecondMs ?? "-" }} ms</td>
                <td class="num">
                  <span v-if="p.underruns" class="bench-change worse">▲ {{ p.underruns }}</span>
                  <span v-else>0</span>
                </td>
                <td class="num">{{ p.minRunwayS ?? "-" }} s</td>
                <td class="num">{{ p.cpuPercent?.isr ?? "-" }} %</td>
                <td class="num">{{ p.cpuPercent?.sd ?? "-" }} %</td>
                <td class="num">{{ p.maxLoopMs ?? "-" }} ms</td>
                <td class="num">{{ fmt(p.seekUs, "us") }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </template>

      <template v-if="detail?.scenarios?.startup?.length">
        <h3>Track start under load, per sample</h3>
        <p class="bench-note">
          Each sample starts while the UI is hammered for its first seconds (transitions, searches, fast scrolling):
          its buffer is still empty, the riskiest moment. "Idle start" is the same track started with nothing else going on.
        </p>
        <div class="bench-table-wrap">
          <table class="bench-table">
            <thead>
              <tr>
                <th>Sample</th>
                <th class="num">Underruns</th>
                <th class="num">Min runway</th>
                <th class="num">Longest loop</th>
                <th class="num">Load ran for</th>
                <th class="num">Idle start: min runway</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="p in detail.scenarios.startup" :key="p.sample">
                <td>{{ p.sample }}</td>
                <td class="num">
                  <span v-if="p.underruns" class="bench-change worse">▲ {{ p.underruns }}</span>
                  <span v-else>0</span>
                </td>
                <td class="num">{{ p.minRunwayS ?? "-" }} s</td>
                <td class="num">{{ p.maxLoopMs ?? "-" }} ms</td>
                <td class="num">{{ (p.loadMs / 1000).toFixed(1) }} s</td>
                <td class="num">{{ idleRunway(p.sample) }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </template>

      <template v-if="detail?.scenarios?.search?.queries">
        <h3>Search, per query</h3>
        <div class="bench-table-wrap">
          <table class="bench-table">
            <thead>
              <tr>
                <th>Query</th>
                <th class="num">Hits</th>
                <th class="num">Typing it, total</th>
                <th class="num">Slowest keystroke</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="q in detail.scenarios.search.queries" :key="q.query">
                <td><code>{{ q.query }}</code></td>
                <td class="num">{{ q.hits }}</td>
                <td class="num">{{ fmt(q.totalUs, "us") }}</td>
                <td class="num">{{ fmt(q.maxCharUs, "us") }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </template>

      <p v-if="detail?.errors?.length" class="bench-errors">
        Failed in this run:
        <span v-for="e in detail.errors" :key="e.scenario"><b>{{ e.scenario }}</b>: {{ e.message }}. </span>
      </p>
      <p class="bench-note">
        <a :href="withBase(`/bench/${selectedRun?.file}`)" target="_blank">Raw report (JSON)</a>
      </p>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue";
import { withBase } from "vitepress";

type Metric = { key: string; label: string; unit: string; better: "lower" | "higher"; group: string };
type Run = { rev: string; date: string; file: string; dataset: Record<string, any>; summary: Record<string, number> };

const W = 320;
const H = 140;
const PAD_L = 44;
const PAD_R = 10;
const PAD_T = 12;
const PAD_B = 14;

const index = ref<{ metrics: Metric[]; runs: Run[] } | null>(null);
const error = ref("");
const selectedRev = ref("");
const detail = ref<any>(null);
const hover = ref<{ key: string; i: number; x: number; y: number; run: Run; v: number } | null>(null);

onMounted(async () => {
  try {
    const res = await fetch(withBase("/bench/index.json"));
    if (!res.ok) {
      index.value = { metrics: [], runs: [] };
      return;
    }
    index.value = await res.json();
    selectedRev.value = index.value!.runs.at(-1)?.rev ?? "";
  } catch (e) {
    error.value = `Couldn't load the bench reports (${e}).`;
  }
});

const selectedRun = computed(() => index.value?.runs.find((r) => r.rev === selectedRev.value));
const selected = selectedRun;
const previous = computed(() => {
  const runs = index.value?.runs ?? [];
  const i = runs.findIndex((r) => r.rev === selectedRev.value);
  return i > 0 ? runs[i - 1] : null;
});

watch(selectedRun, async (run) => {
  detail.value = null;
  if (!run) return;
  try {
    detail.value = await (await fetch(withBase(`/bench/${run.file}`))).json();
  } catch {
    detail.value = null;
  }
});

const groups = computed(() => [...new Set((index.value?.metrics ?? []).map((m) => m.group))]);
const metricsIn = (g: string) => (index.value?.metrics ?? []).filter((m) => m.group === g);

function fmt(v: number | undefined | null, unit: string): string {
  if (v === undefined || v === null || Number.isNaN(v)) return "–";
  if (unit === "us") return v >= 1000 ? `${(v / 1000).toFixed(1)} ms` : `${Math.round(v)} µs`;
  if (unit === "count") return String(v);
  return `${Math.round(v * 100) / 100} ${unit}`;
}

function unitLabel(m: Metric) {
  return m.unit === "us" ? "ms" : m.unit === "count" ? "count" : m.unit;
}

function change(m: Metric) {
  const a = previous.value?.summary[m.key];
  const b = selected.value?.summary[m.key];
  if (a === undefined || b === undefined) return null;
  const delta = b - a;
  if (delta === 0) return { kind: "same", icon: "", text: "same" };
  const rel = a === 0 ? Infinity : delta / Math.abs(a);
  const significant = Math.abs(rel) > 0.1 || (m.unit === "count" && a === 0);
  const worse = m.better === "lower" ? delta > 0 : delta < 0;
  const text = a === 0 ? `${delta > 0 ? "+" : ""}${delta}` : `${rel > 0 ? "+" : ""}${Math.round(rel * 100)}%`;
  if (!significant) return { kind: "same", icon: "", text };
  return worse ? { kind: "worse", icon: "▲", text: `${text} worse` } : { kind: "better", icon: "▼", text: `${text} better` };
}

// --- Trend charts: one small multiple per metric, revisions on x ---------

function runsWith(m: Metric) {
  return (index.value?.runs ?? []).filter((r) => r.summary[m.key] !== undefined);
}

function scaleY(m: Metric) {
  const vals = runsWith(m).map((r) => r.summary[m.key] / (m.unit === "us" ? 1000 : 1));
  const max = Math.max(...vals, 0) * 1.15 || 1;
  return { max, y: (v: number) => PAD_T + (H - PAD_T - PAD_B) * (1 - v / max) };
}

function points(m: Metric) {
  const runs = runsWith(m);
  const { y } = scaleY(m);
  const step = runs.length > 1 ? (W - PAD_L - PAD_R) / (runs.length - 1) : 0;
  return runs.map((run, i) => {
    const v = run.summary[m.key];
    return { x: PAD_L + (runs.length > 1 ? i * step : (W - PAD_L - PAD_R) / 2), y: y(v / (m.unit === "us" ? 1000 : 1)), run, v };
  });
}

function hitW(m: Metric) {
  const n = runsWith(m).length;
  return n > 1 ? (W - PAD_L - PAD_R) / (n - 1) : W;
}

function path(m: Metric) {
  return points(m)
    .map((p, i) => `${i ? "L" : "M"}${p.x.toFixed(1)},${p.y.toFixed(1)}`)
    .join("");
}

function trend(m: Metric) {
  const { max, y } = scaleY(m);
  const nice = niceStep(max / 3);
  const out = [];
  for (let v = 0; v <= max; v += nice) out.push({ grid: true, y: y(v), label: +v.toFixed(2) });
  return out;
}

function niceStep(raw: number) {
  const p = Math.pow(10, Math.floor(Math.log10(raw || 1)));
  const n = raw / p;
  return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * p;
}

function tipX(x: number) {
  return Math.min(Math.max(x - 64, 0), W - 128);
}

/** The same sample's lowest runway when started idle (playback scenario). */
function idleRunway(sample: string) {
  const p = detail.value?.scenarios?.playback?.find((x: any) => x.sample === sample);
  return p?.minRunwayS != null ? `${p.minRunwayS} s` : "–";
}

function shortDate(d: string) {
  return d ? d.slice(0, 10) : "";
}

function datasetLine(ds: Record<string, any>) {
  if (!ds) return "";
  const parts = [`${(ds.tracksLoaded ?? ds.tracks ?? "?").toLocaleString?.() ?? ds.tracks} tracks`];
  if (ds.albums) parts.push(`${ds.albums} albums`);
  if (ds.artists) parts.push(`${ds.artists} artists`);
  if (ds.seed !== undefined) parts.push(`seed ${ds.seed}`);
  if (ds.covers) parts.push("covers");
  return parts.join(" · ");
}
</script>

<style scoped>
.viz-root {
  --series-1: #2a78d6;
  --bench-good: #008300;
  --bench-bad: #c0392b;
}
/* VitePress puts .dark on <html> for both the OS setting and its toggle. */
:root.dark .viz-root {
  --series-1: #3987e5;
  --bench-good: #3fae4a;
  --bench-bad: #e66767;
}
.bench-empty,
.bench-note,
.bench-meta {
  color: var(--vp-c-text-2);
}
.bench-bar {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 16px;
  align-items: center;
  margin: 16px 0;
}
.bench-bar select {
  margin-left: 8px;
  padding: 4px 8px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  font-family: var(--vp-font-family-mono);
  font-size: 13px;
}
.bench-table-wrap {
  overflow-x: auto;
}
.bench-table {
  display: table;
  width: 100%;
  font-size: 14px;
}
.bench-table .num {
  text-align: right;
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
}
.bench-group td {
  font-weight: 600;
  color: var(--vp-c-text-2);
  background: var(--vp-c-bg-soft);
}
.bench-change {
  white-space: nowrap;
  color: var(--vp-c-text-2);
}
.bench-change.worse {
  color: var(--bench-bad);
  font-weight: 600;
}
.bench-change.better {
  color: var(--bench-good);
}
.bench-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: 16px;
}
.bench-trend {
  margin: 0;
  padding: 10px 12px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}
.bench-trend figcaption {
  font-size: 13px;
  font-weight: 600;
  color: var(--vp-c-text-1);
}
.bench-unit {
  font-weight: 400;
  color: var(--vp-c-text-3);
}
.bench-trend svg {
  width: 100%;
  height: auto;
  overflow: visible;
}
.grid {
  stroke: var(--vp-c-divider);
  stroke-width: 1;
}
.tick {
  fill: var(--vp-c-text-3);
  font-size: 10px;
  text-anchor: end;
  font-variant-numeric: tabular-nums;
}
.line {
  fill: none;
  stroke: var(--series-1);
  stroke-width: 2;
  stroke-linejoin: round;
}
.dot {
  fill: var(--series-1);
  stroke: var(--vp-c-bg-soft);
  stroke-width: 2;
}
.dot.on {
  r: 6;
}
.hit {
  fill: transparent;
  cursor: pointer;
}
.tip rect {
  fill: var(--vp-c-bg);
  stroke: var(--vp-c-divider);
}
.tip text {
  font-size: 11px;
}
.tip-v {
  fill: var(--vp-c-text-1);
  font-weight: 600;
}
.tip-r {
  fill: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
}
.tip {
  pointer-events: none;
}
.bench-errors {
  color: var(--bench-bad);
}
</style>
