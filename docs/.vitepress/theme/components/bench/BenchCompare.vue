<template>
  <div class="bc vp-raw">
    <div class="bc-slots">
      <section
        v-for="side in SIDES"
        :key="side"
        class="bc-slot"
        :class="{ over: dragOver === side, filled: !!slots[side] }"
        @dragover.prevent="dragOver = side"
        @dragleave="dragOver = null"
        @drop.prevent="onDrop(side, $event)"
      >
        <div class="bc-slot-head">
          <span class="bc-label">{{ side === "a" ? "Before (A)" : "After (B)" }}</span>
          <button v-if="slots[side]" class="bc-text-btn" @click="slots[side] = null">Clear</button>
        </div>
        <template v-if="slots[side]">
          <div class="bc-rev bl-mono">{{ slots[side]!.report.rev }}</div>
          <div class="bc-small bc-muted">
            {{ fullDate(slots[side]!.report.date) }} · {{ slots[side]!.name }}
          </div>
          <div class="bc-small bc-muted">{{ datasetLine(slots[side]!.report) }}</div>
        </template>
        <div v-else class="bc-small bc-muted">Drop a report (.json) here, or pick one below.</div>
        <div class="bc-slot-actions">
          <select v-if="sources.length" class="bc-select" :value="''" @change="pickSource(side, $event)">
            <option value="" disabled>{{ loadingSources ? "Loading…" : "Saved reports…" }}</option>
            <option v-for="s in sources" :key="s.url" :value="s.url">{{ s.label }}</option>
          </select>
          <label class="bc-ghost xs">
            Open file…
            <input type="file" accept=".json,application/json" hidden @change="onFile(side, $event)" />
          </label>
        </div>
      </section>
    </div>

    <p v-if="loadError" class="bc-note red">{{ loadError }}</p>

    <template v-if="A && B">
      <p v-for="w in warnings" :key="w" class="bc-note amber">{{ w }}</p>

      <div class="bc-toolbar">
        <div class="bc-badges">
          <span class="bc-badge red">{{ counts.worse }} worse</span>
          <span class="bc-badge green">{{ counts.better }} better</span>
          <span class="bc-badge muted">{{ counts.same }} same</span>
        </div>
        <span class="bc-spacer" />
        <button class="bc-ghost xs" title="Swap A and B" @click="swap">⇄ Swap A and B</button>
        <span class="bc-small bc-muted">Noise below</span>
        <Seg v-model="threshold" :options="THRESHOLDS" label="Noise threshold" size="sm" />
      </div>

      <h3>Headline numbers</h3>
      <div class="bc-table-wrap">
        <table>
          <thead>
            <tr>
              <th>Metric</th>
              <th class="r">A</th>
              <th class="r">B</th>
              <th class="r">Change</th>
            </tr>
          </thead>
          <tbody>
            <template v-for="g in headlineGroups" :key="g.group">
              <tr class="bc-group">
                <td colspan="4">{{ g.group }}</td>
              </tr>
              <tr v-for="m in g.metrics" :key="m.key">
                <td>{{ m.label }}</td>
                <td class="r bl-mono bc-muted">{{ fmtParts(sa[m.key], m.unit).join(" ") }}</td>
                <td class="r bl-mono">
                  {{ fmtParts(sb[m.key], m.unit)[0] }} <span class="bc-muted">{{ fmtParts(sb[m.key], m.unit)[1] }}</span>
                </td>
                <td class="r">
                  <span v-if="headlineChange(m)" class="bc-change" :class="headlineChange(m)!.kind">{{ headlineChange(m)!.text }}</span>
                  <span v-else class="bc-muted bc-small">only in {{ sa[m.key] === undefined ? "B" : "A" }}</span>
                </td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>

      <div class="bc-detail-head">
        <h3>Every number</h3>
        <label class="bc-check">
          <input v-model="onlyChanged" type="checkbox" />
          Only what changed
        </label>
      </div>
      <p class="bc-small bc-muted">
        Each scenario's raw figures, matched by sample, query or step. Rows without a clear better direction (counts, durations of the run) show as plain changes.
      </p>
      <details v-for="sec in detailSections" :key="sec.name" class="bc-section" :open="sec.open">
        <summary>
          <span class="bc-section-name">{{ sec.name }}</span>
          <span class="bc-badges">
            <span v-if="sec.counts.worse" class="bc-badge red">{{ sec.counts.worse }} worse</span>
            <span v-if="sec.counts.better" class="bc-badge green">{{ sec.counts.better }} better</span>
            <span v-if="sec.counts.missing" class="bc-badge amber">{{ sec.counts.missing }} only in one</span>
            <span v-if="!sec.counts.worse && !sec.counts.better && !sec.counts.missing" class="bc-badge muted">no change</span>
          </span>
        </summary>
        <div class="bc-table-wrap">
          <table>
            <thead>
              <tr>
                <th>Figure</th>
                <th class="r">A</th>
                <th class="r">B</th>
                <th class="r">Change</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in sec.rows" :key="row.path">
                <td class="bl-mono bc-path">
                  <span v-for="(p, i) in row.label" :key="i"><span v-if="i" class="bc-muted"> › </span>{{ p }}</span>
                </td>
                <td class="r bl-mono bc-muted">{{ fmtLeaf(row.a, row.path) }}</td>
                <td class="r bl-mono">{{ fmtLeaf(row.b, row.path) }}</td>
                <td class="r">
                  <span v-if="row.change" class="bc-change" :class="row.change.kind">{{ row.change.text }}</span>
                  <span v-else class="bc-muted bc-small">only in {{ row.a === undefined ? "B" : "A" }}</span>
                </td>
              </tr>
              <tr v-if="!sec.rows.length">
                <td colspan="4" class="bc-muted">Nothing changed beyond {{ Math.round(threshold * 100) }}%.</td>
              </tr>
              <tr v-if="sec.hidden">
                <td colspan="4" class="bc-muted bc-small">
                  {{ sec.hidden }} figure{{ sec.hidden > 1 ? "s" : "" }} in only one report, not shown:
                  <span class="bl-mono">{{ sec.hiddenNames.join(", ") }}</span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </details>

      <div v-for="e in runErrors" :key="e.side + e.scenario" class="bc-note red">
        <b>{{ e.side }}: {{ e.scenario }} {{ e.message === "stopped" ? "didn't run" : "failed" }}.</b> {{ e.message === "stopped" ? "" : e.message }}
      </div>
    </template>
    <p v-else class="bc-empty bc-muted">Load two reports to compare them.</p>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from "vue";
import { withBase } from "vitepress";
import { METRICS, summarize, type Metric, type Report } from "@bench/metrics.ts";
import Seg from "./Seg.vue";
import { changeOf, fmtParts, type Change } from "./format.ts";

type Side = "a" | "b";
type Loaded = { name: string; report: Report };

const SIDES: Side[] = ["a", "b"];
const THRESHOLDS = [
  { value: 0.05, label: "5%" },
  { value: 0.1, label: "10%" },
  { value: 0.25, label: "25%" },
];

const slots = reactive<Record<Side, Loaded | null>>({ a: null, b: null });
const A = computed(() => slots.a?.report ?? null);
const B = computed(() => slots.b?.report ?? null);
const threshold = ref(0.1);
const onlyChanged = ref(true);
const dragOver = ref<Side | null>(null);
const loadError = ref("");

// Reports to pick from: the repo's benchmarks/ folder, and the ones published to the docs.
const REPO_REPORTS = import.meta.glob("../../../../../benchmarks/*.json", { query: "?url", import: "default" }) as Record<
  string,
  () => Promise<string>
>;
const sources = ref<{ label: string; url: string }[]>([]);
const loadingSources = ref(true);

onMounted(async () => {
  const list: { label: string; url: string }[] = [];
  for (const [path, load] of Object.entries(REPO_REPORTS)) {
    list.push({ label: `benchmarks/${path.split("/").pop()}`, url: await load() });
  }
  try {
    const index = await (await fetch(withBase("/bench/index.json"))).json();
    for (const r of [...(index.runs ?? [])].reverse()) list.push({ label: `published · ${r.rev}`, url: withBase(`/bench/${r.file}`) });
  } catch {
    // Nothing published.
  }
  list.sort((x, y) => y.label.localeCompare(x.label));
  sources.value = list;
  loadingSources.value = false;
  // Newest saved report as A, so a downloaded one only has to be dropped on B.
  if (list.length && !slots.a) await loadUrl("a", list[0].url, list[0].label);
});

function accept(side: Side, name: string, text: string) {
  try {
    const r = JSON.parse(text);
    if (!r || typeof r !== "object" || !r.scenarios || !r.rev) throw new Error("not a bench report (no rev or scenarios)");
    r.boot ??= {};
    slots[side] = { name, report: r };
    loadError.value = "";
  } catch (e) {
    loadError.value = `${name}: ${e instanceof Error ? e.message : String(e)}`;
  }
}

async function loadUrl(side: Side, url: string, label: string) {
  try {
    accept(side, label, await (await fetch(url)).text());
  } catch (e) {
    loadError.value = `${label}: ${e instanceof Error ? e.message : String(e)}`;
  }
}

function pickSource(side: Side, e: Event) {
  const el = e.target as HTMLSelectElement;
  const s = sources.value.find((x) => x.url === el.value);
  el.value = "";
  if (s) loadUrl(side, s.url, s.label);
}

async function onFile(side: Side, e: Event) {
  const el = e.target as HTMLInputElement;
  const f = el.files?.[0];
  el.value = "";
  if (f) accept(side, f.name, await f.text());
}

async function onDrop(side: Side, e: DragEvent) {
  dragOver.value = null;
  const f = e.dataTransfer?.files?.[0];
  if (f) accept(side, f.name, await f.text());
}

function swap() {
  [slots.a, slots.b] = [slots.b, slots.a];
}

// Header lines

const fullDate = (d: string) => {
  const t = new Date(d);
  return Number.isNaN(+t) ? d : t.toLocaleString(undefined, { dateStyle: "medium", timeStyle: "short" });
};
function datasetLine(r: Report) {
  const d = r.dataset as Record<string, unknown>;
  const parts = [d.tracks && `${Number(d.tracks).toLocaleString()} tracks`, d.seed !== undefined && `seed ${d.seed}`, r.background?.enabled ? `background: ${r.background.sample}` : "no background"];
  return parts.filter(Boolean).join(" · ");
}

const warnings = computed(() => {
  const a = A.value!;
  const b = B.value!;
  const w: string[] = [];
  const da = a.dataset as Record<string, unknown>;
  const db = b.dataset as Record<string, unknown>;
  if (da.tracks !== db.tracks || da.seed !== db.seed)
    w.push(`Different libraries (A: ${da.tracks} tracks, seed ${da.seed}; B: ${db.tracks} tracks, seed ${db.seed}): browsing and search numbers aren't comparable.`);
  if (!!a.background?.enabled !== !!b.background?.enabled)
    w.push("Only one of the runs had the background sample playing while browsing: browsing numbers aren't comparable.");
  return w;
});

const runErrors = computed(() => [
  ...(A.value?.errors ?? []).map((e) => ({ side: "A", ...e })),
  ...(B.value?.errors ?? []).map((e) => ({ side: "B", ...e })),
]);

// Headline metrics

const sa = computed(() => summarize(A.value!));
const sb = computed(() => summarize(B.value!));
const headline = computed(() => METRICS.filter((m) => sa.value[m.key] !== undefined || sb.value[m.key] !== undefined));
const headlineGroups = computed(() => {
  const groups: { group: string; metrics: Metric[] }[] = [];
  for (const m of headline.value) {
    let g = groups.find((x) => x.group === m.group);
    if (!g) groups.push((g = { group: m.group, metrics: [] }));
    g.metrics.push(m);
  }
  return groups;
});
const headlineChange = (m: Metric) => changeOf(sa.value[m.key], sb.value[m.key], m.better, m.unit === "count", threshold.value);
const counts = computed(() => {
  const c = { worse: 0, better: 0, same: 0 };
  for (const m of headline.value) {
    const k = headlineChange(m)?.kind;
    if (k === "worse" || k === "better" || k === "same") c[k]++;
  }
  return c;
});

// Every number: flatten both reports' scenarios, match leaves by path.

type Leaf = { path: string; label: string[]; value: number };
/** Array items are matched by these instead of their position. */
const ITEM_KEYS = ["sample", "query", "what"];
/** Time series and raw dumps: not figures to compare. */
const SKIP = new Set(["trace", "_raw", "log"]);

function flatten(v: unknown, path: string[], out: Map<string, Leaf>) {
  if (typeof v === "number" && Number.isFinite(v)) {
    out.set(path.join("."), { path: path.join("."), label: path, value: v });
  } else if (typeof v === "boolean") {
    out.set(path.join("."), { path: path.join("."), label: path, value: v ? 1 : 0 });
  } else if (Array.isArray(v)) {
    v.forEach((item, i) => {
      const k = item && typeof item === "object" ? ITEM_KEYS.find((k) => typeof item[k] === "string") : undefined;
      flatten(item, [...path, k ? String(item[k]) : `#${i + 1}`], out);
    });
  } else if (v && typeof v === "object") {
    for (const [k, x] of Object.entries(v)) if (!SKIP.has(k) && !ITEM_KEYS.includes(k)) flatten(x, [...path, k], out);
  }
}

function leaves(r: Report) {
  const out = new Map<string, Leaf>();
  flatten({ boot: r.boot, background: r.background ? { startMs: r.background.startMs, restarts: r.background.restarts } : undefined }, [], out);
  flatten(r.scenarios, [], out);
  return out;
}

/** Which way is better, guessed from the field name. null: neither. */
function direction(path: string): "lower" | "higher" | null {
  const last = path.split(".").pop()!;
  if (/^(minRunwayS|stillPlaying|free_ram)$/.test(last)) return "higher";
  if (/^(n|seconds|durationS|hits|turns|eventsPerTurn|overlayTurns|meanRateKBs|uptime_ms|tracks)$/.test(last)) return null;
  if (path.includes(".cpuPercent.")) return "lower";
  if (/(Us|Ms|_ms|^ms|^us|underruns|restarts)$/.test(last) || /^(p50|p95|max|mean)$/.test(last)) return "lower";
  return null;
}
const isCount = (path: string) => /(underruns|restarts)$/.test(path);

/** A raw figure, unit from its name. */
function fmtLeaf(v: number | undefined, path: string) {
  if (v === undefined) return "–";
  const parts = path.split(".");
  const last = parts[parts.length - 1];
  const unitOf = /^(p50|p95|max|mean)$/.test(last) ? parts[parts.length - 2] : last;
  if (path.includes(".cpuPercent.")) return `${v} %`;
  if (/Us$|^us$/.test(unitOf)) return fmtParts(v, "us").join(" ");
  if (/(Ms|_ms)$|^ms$/.test(unitOf)) return `${Math.round(v * 100) / 100} ms`;
  if (/S$/.test(unitOf) && unitOf !== "underruns") return `${v} s`;
  if (/KBs$/.test(unitOf)) return `${v} KB/s`;
  return Math.round(v * 100) / 100 + "";
}

type Row = { path: string; label: string[]; a?: number; b?: number; change: Change | null };

const detailSections = computed(() => {
  const la = leaves(A.value!);
  const lb = leaves(B.value!);
  const paths = [...new Set([...la.keys(), ...lb.keys()])];
  const bySection = new Map<string, Row[]>();
  for (const p of paths) {
    const a = la.get(p)?.value;
    const b = lb.get(p)?.value;
    const label = (la.get(p) ?? lb.get(p))!.label;
    const change = changeOf(a, b, direction(p), isCount(p), threshold.value);
    const rows = bySection.get(label[0]) ?? [];
    rows.push({ path: p, label: label.slice(1), a, b, change });
    bySection.set(label[0], rows);
  }
  return [...bySection].map(([name, all]) => {
    const c = { worse: 0, better: 0, missing: 0 };
    for (const r of all) {
      if (!r.change) c.missing++;
      else if (r.change.kind === "worse") c.worse++;
      else if (r.change.kind === "better") c.better++;
    }
    // A scenario only one report ran: all its rows are "only in", show them anyway.
    const oneSided = c.missing === all.length;
    const rows = onlyChanged.value && !oneSided ? all.filter((r) => r.change && r.change.kind !== "same") : all;
    const hidden = onlyChanged.value && !oneSided ? all.filter((r) => !r.change) : [];
    const hiddenNames = [...new Set(hidden.map((r) => r.label[r.label.length - 1]))];
    return { name, rows, counts: c, open: c.worse > 0, hidden: hidden.length, hiddenNames };
  });
});
</script>

<style scoped>
.bc {
  --bl-surface: var(--vp-c-bg-soft);
  --bl-surface2: #ebebef;
  --bl-line: var(--vp-c-divider);
  --bl-line2: #c2c2c4;
  --bl-text: var(--vp-c-text-1);
  --bl-muted: var(--vp-c-text-2);
  --bl-brand: #972d6f;
  --bl-brandsoft: rgba(151, 45, 111, 0.11);
  --bl-green: #3d7419;
  --bl-greensoft: rgba(96, 165, 52, 0.14);
  --bl-amber: #9a5b00;
  --bl-ambersoft: rgba(154, 91, 0, 0.11);
  --bl-red: #a9392f;
  --bl-redsoft: rgba(169, 57, 47, 0.11);
  --bl-mono: "IBM Plex Mono", var(--vp-font-family-mono);

  display: flex;
  flex-direction: column;
  gap: 14px;
  margin-top: 24px;
  font-size: 14px;
  line-height: 1.45;
  font-variant-numeric: tabular-nums;
  color: var(--bl-text);
}
:root.dark .bc {
  --bl-surface2: #32363f;
  --bl-line2: #3c3f44;
  --bl-brand: #cb5297;
  --bl-brandsoft: rgba(203, 82, 151, 0.16);
  --bl-green: #96dd42;
  --bl-greensoft: rgba(150, 221, 66, 0.13);
  --bl-amber: #f0a83a;
  --bl-ambersoft: rgba(240, 168, 58, 0.13);
  --bl-red: #e8857b;
  --bl-redsoft: rgba(232, 133, 123, 0.14);
}
.bc h3 {
  margin: 12px 0 0;
  font-size: 16px;
  font-weight: 600;
}
.bl-mono {
  font-family: var(--bl-mono);
}
.bc-muted {
  color: var(--bl-muted);
}
.bc-small {
  font-size: 12px;
}
.bc-spacer {
  flex: 1;
}

/* Report slots */
.bc-slots {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}
.bc-slot {
  display: flex;
  flex-direction: column;
  gap: 4px;
  min-width: 0;
  padding: 14px 16px;
  border: 1px dashed var(--bl-line2);
  border-radius: 12px;
  background: var(--bl-surface);
}
.bc-slot.filled {
  border-style: solid;
  border-color: var(--bl-line);
}
.bc-slot.over {
  border: 1px dashed var(--bl-brand);
  background: var(--bl-brandsoft);
}
.bc-slot-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.bc-label {
  font-size: 12px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--bl-muted);
}
.bc-rev {
  font-size: 15px;
  font-weight: 600;
  overflow-wrap: anywhere;
}
.bc-slot-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-top: 8px;
}
.bc-select {
  min-width: 0;
  max-width: 100%;
  height: 26px;
  padding: 0 8px;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  background: var(--vp-c-bg);
  color: var(--bl-text);
  font-size: 12px;
}
@media (max-width: 640px) {
  .bc-slots {
    grid-template-columns: 1fr;
  }
}

/* Buttons */
.bc-ghost {
  display: inline-flex;
  align-items: center;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  background: transparent;
  color: var(--bl-text);
  font-weight: 500;
  white-space: nowrap;
  cursor: pointer;
}
.bc-ghost.xs {
  height: 26px;
  padding: 0 10px;
  font-size: 12px;
}
.bc-ghost:hover:not(:disabled) {
  background: var(--bl-surface2);
}
.bc-ghost:disabled {
  opacity: 0.5;
  cursor: default;
}
.bc-text-btn {
  border: 0;
  background: transparent;
  color: var(--bl-muted);
  font-size: 12px;
  cursor: pointer;
}
.bc-text-btn:hover {
  color: var(--bl-text);
}
.bc-check {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  cursor: pointer;
}

/* Notes and badges */
.bc-note {
  margin: 0;
  padding: 12px 14px;
  border-radius: 10px;
  font-size: 13px;
}
.bc-note.amber {
  background: var(--bl-ambersoft);
}
.bc-note.red {
  background: var(--bl-redsoft);
}
.bc-note.red b {
  color: var(--bl-red);
}
.bc-empty {
  padding: 24px 0;
  text-align: center;
}
.bc-toolbar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
}
.bc-badges {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.bc-badge {
  display: inline-flex;
  align-items: center;
  height: 20px;
  padding: 0 7px;
  border-radius: 4px;
  font-family: var(--bl-mono);
  font-size: 11px;
  font-weight: 500;
  white-space: nowrap;
}
.bc-badge.red {
  background: var(--bl-redsoft);
  color: var(--bl-red);
}
.bc-badge.green {
  background: var(--bl-greensoft);
  color: var(--bl-green);
}
.bc-badge.amber {
  background: var(--bl-ambersoft);
  color: var(--bl-amber);
}
.bc-badge.muted {
  background: var(--bl-surface2);
  color: var(--bl-muted);
}

/* Tables */
.bc-table-wrap {
  overflow-x: auto;
}
.bc-table-wrap table {
  display: table;
  width: 100%;
  margin: 0;
  min-width: 480px;
  border-collapse: collapse;
  font-size: 13px;
}
.bc .bc-table-wrap tr {
  border: 0;
  background: none;
}
.bc-table-wrap th {
  padding: 0 12px 8px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
  font-size: 12px;
  font-weight: 500;
  text-align: left;
  color: var(--bl-muted);
}
.bc-table-wrap td {
  padding: 8px 12px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
}
.bc-table-wrap th:first-child,
.bc-table-wrap td:first-child {
  padding-left: 0;
}
.bc-table-wrap th:last-child,
.bc-table-wrap td:last-child {
  padding-right: 0;
}
.bc-table-wrap .r {
  text-align: right;
  white-space: nowrap;
}
.bc-group td {
  padding-top: 16px;
  font-size: 12px;
  font-weight: 600;
  color: var(--bl-muted);
}
.bc-path {
  font-size: 12px;
  overflow-wrap: anywhere;
}
.bc-change {
  display: inline-flex;
  align-items: center;
  height: 22px;
  padding: 0 8px;
  border-radius: 4px;
  font-family: var(--bl-mono);
  font-size: 12px;
  font-weight: 500;
  color: var(--bl-muted);
}
.bc-change.worse {
  background: var(--bl-redsoft);
  color: var(--bl-red);
  font-weight: 600;
}
.bc-change.better {
  background: var(--bl-greensoft);
  color: var(--bl-green);
}
.bc-change.changed {
  background: var(--bl-surface2);
  color: var(--bl-text);
}

/* Per-scenario sections */
.bc-detail-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
}
.bc-section {
  border: 1px solid var(--bl-line);
  border-radius: 12px;
  background: var(--bl-surface);
}
.bc-section summary {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  cursor: pointer;
}
.bc-section-name {
  font-weight: 600;
  min-width: 110px;
}
.bc-section .bc-table-wrap {
  padding: 0 16px 12px;
}
</style>
