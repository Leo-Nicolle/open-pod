<template>
  <section class="bl-panel bl-results">
    <div class="bl-results-main">
      <div class="bl-section-head">
        <div class="bl-results-title">
          <h2>Results</h2>
          <span class="bl-mono bl-small bl-muted">
            {{ report.rev }} · {{ hhmm(report.date) }}
            <template v-if="reference"> vs published {{ reference.rev }}<template v-if="reference.date"> · {{ shortDate(reference.date) }}</template></template>
            <template v-else> · nothing published to compare with</template>
          </span>
        </div>
        <div v-if="reference" class="bl-badges">
          <span class="bl-badge red">{{ counts.worse }} worse</span>
          <span class="bl-badge green">{{ counts.better }} better</span>
          <span class="bl-badge muted">{{ counts.same }} same (±10%)</span>
        </div>
      </div>
      <div class="bl-table-wrap">
        <table>
          <thead>
            <tr>
              <th>Metric</th>
              <th class="r">This run</th>
              <th v-if="reference" class="r">Published</th>
              <th v-if="reference" class="r">Change</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="m in shownMetrics" :key="m.key">
              <td>
                <div>{{ m.label }}</div>
                <div class="bl-small bl-muted">{{ m.group }}</div>
              </td>
              <td class="r bl-mono">
                {{ fmtParts(summary[m.key], m.unit)[0] }} <span class="bl-muted">{{ fmtParts(summary[m.key], m.unit)[1] }}</span>
              </td>
              <td v-if="reference" class="r bl-mono bl-muted">{{ fmtParts(reference.summary[m.key], m.unit).join(" ") }}</td>
              <td v-if="reference" class="r">
                <span v-if="change(m)" class="bl-change" :class="change(m)!.kind">{{ change(m)!.text }}</span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
      <div v-for="e in report.errors" :key="e.scenario" class="bl-note red">
        <div class="bl-note-title">
          {{ e.scenario === "background" ? "Background track" : (SCENARIOS[e.scenario]?.label ?? e.scenario) }} {{ e.message === "stopped" ? "didn't run" : "failed" }}
        </div>
        <div>{{ e.message === "stopped" ? "The run was stopped before it started." : e.message }}</div>
      </div>
    </div>
    <aside class="bl-publish">
      <div class="bl-label">Publish this run</div>
      <div class="bl-step">
        <span class="bl-step-n">1</span>
        <div class="bl-step-body">
          <div>Download the report</div>
          <button class="bl-primary wide" @click="download">Download report</button>
          <div class="bl-mono bl-xs bl-muted">{{ fileName }} · {{ reportKB }} KB</div>
        </div>
      </div>
      <div class="bl-step">
        <span class="bl-step-n">2</span>
        <div class="bl-step-body">
          <div>Publish it from the repo</div>
          <div class="bl-code">
            <span>{{ publishCmd }}</span>
            <button @click="copyPublish">{{ copied ? "Copied" : "Copy" }}</button>
          </div>
          <div class="bl-small bl-muted">Run it in <code class="bl-mono">software/</code>. It becomes the new baseline for the next comparison.</div>
        </div>
      </div>
    </aside>
  </section>
</template>

<script setup lang="ts">
import { computed, ref } from "vue";
import { SCENARIOS } from "@bench/bench.ts";
import { METRICS, revFile, summarize, type Metric, type Report } from "@bench/metrics.ts";
import { changeOf, fmtParts } from "../format.ts";
import { useBenchLive } from "./state.ts";

const props = defineProps<{ report: Report }>();
const { reportJson, reference } = useBenchLive();

const summary = computed(() => summarize(props.report));
const shownMetrics = computed(() => METRICS.filter((m) => summary.value[m.key] !== undefined));
const fileName = computed(() => `${revFile(props.report.rev)}.json`);
const reportKB = computed(() => Math.max(1, Math.round(new Blob([reportJson.value]).size / 1024)));
const publishCmd = computed(() => `npm run bench -- import ~/Downloads/${fileName.value}`);

function change(m: Metric) {
  return changeOf(reference.value?.summary[m.key], summary.value[m.key], m.better, m.unit === "count");
}
const counts = computed(() => {
  const c = { worse: 0, better: 0, same: 0 };
  for (const m of shownMetrics.value) {
    const k = change(m)?.kind as keyof typeof c | undefined;
    if (k) c[k]++;
  }
  return c;
});

function download() {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([reportJson.value], { type: "application/json" }));
  a.download = fileName.value;
  a.click();
  URL.revokeObjectURL(a.href);
}

const copied = ref(false);
async function copyPublish() {
  try {
    await navigator.clipboard.writeText(publishCmd.value);
    copied.value = true;
    setTimeout(() => (copied.value = false), 1400);
  } catch {
    // No clipboard access: the command stays selectable.
  }
}

const hhmm = (iso: string) => new Date(iso).toTimeString().slice(0, 5);
const shortDate = (iso: string) => new Date(iso).toLocaleDateString("en-US", { month: "short", day: "numeric" });
</script>

<style scoped>
/* Results */
.bl-results {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 320px;
  gap: 24px;
  padding: 24px;
}
@container (max-width: 1099px) {
  .bl-results {
    grid-template-columns: minmax(0, 1fr);
  }
}
.bl-results-main {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
}
.bl-results-title {
  display: flex;
  align-items: baseline;
  flex-wrap: wrap;
  gap: 12px;
}
.bl-badges {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.bl-table-wrap {
  overflow-x: auto;
}
.bl-table-wrap table {
  display: table;
  width: 100%;
  margin: 0;
  min-width: 520px;
  border-collapse: collapse;
  font-size: 13px;
}
.bl-table-wrap tr {
  border: 0;
  background: none;
}
.bl-table-wrap th {
  padding: 0 12px 8px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
  font-size: 12px;
  font-weight: 500;
  text-align: left;
  color: var(--bl-muted);
}
.bl-table-wrap td {
  padding: 10px 12px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
}
.bl-table-wrap th:first-child,
.bl-table-wrap td:first-child {
  padding-left: 0;
}
.bl-table-wrap th:last-child,
.bl-table-wrap td:last-child {
  padding-right: 0;
}
.bl-table-wrap .r {
  text-align: right;
  white-space: nowrap;
}
.bl-change {
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
.bl-change.worse {
  background: var(--bl-redsoft);
  color: var(--bl-red);
  font-weight: 600;
}
.bl-change.better {
  background: var(--bl-greensoft);
  color: var(--bl-green);
}
.bl-publish {
  display: flex;
  flex-direction: column;
  gap: 18px;
  align-self: start;
  padding: 20px;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-bg);
}
.bl-step {
  display: grid;
  grid-template-columns: 24px minmax(0, 1fr);
  gap: 10px;
}
.bl-step-n {
  display: grid;
  place-items: center;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-step-body {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-width: 0;
}
.bl-code {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border-radius: 8px;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-code span {
  flex: 1;
  min-width: 0;
  word-break: break-all;
}
.bl-code button {
  height: 26px;
  padding: 0 8px;
  border: 1px solid var(--bl-line2);
  border-radius: 6px;
  background: transparent;
  color: var(--bl-text);
  font-size: 11px;
  cursor: pointer;
}
</style>
