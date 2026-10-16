<template>
  <section class="bl-runner">
    <div class="bl-section-head">
      <h2>Run the bench</h2>
      <p class="bl-muted">Same scenarios as the CLI, <code class="bl-cmd">npm run bench -- run</code>.</p>
    </div>
    <div class="bl-note amber">
      <b>Hands off the wheel.</b> Scenarios drive the device themselves, so the remote is locked until the run
      ends. Stopping is safe: the current scenario finishes and its results are kept.
    </div>
    <div v-if="running && !current" class="bl-progress">
      <div class="bl-progress-row"><span>Preparing the run…</span></div>
      <div class="bl-progress-track"><div style="width: 0" /></div>
    </div>
    <div v-if="running && current" class="bl-progress">
      <div class="bl-progress-row">
        <span>Running <b>{{ SCENARIOS[current.name].label }}</b></span>
        <span class="bl-mono bl-muted">{{ current.index + 1 }} of {{ current.total }}</span>
      </div>
      <div class="bl-progress-track"><div :style="{ width: `${runPct}%` }" /></div>
    </div>
    <div class="bl-scenarios">
      <button
        v-for="(s, name) in SCENARIOS"
        :key="name"
        class="bl-sc"
        :class="{ running: progress[name]?.kind === 'running' }"
        role="checkbox"
        :aria-checked="chosen.includes(name)"
        :disabled="busy"
        @click="toggleScenario(name)"
      >
        <span class="bl-check" :class="{ on: chosen.includes(name) }" aria-hidden="true">{{ chosen.includes(name) ? "✓" : "" }}</span>
        <span class="bl-sc-text">
          <span class="bl-sc-name">{{ s.label }}</span>
          <span class="bl-sc-desc">{{ s.desc }}</span>
        </span>
        <span class="bl-sc-status" :class="progress[name]?.kind" :title="progress[name]?.message">{{ scenarioStatus(name) }}</span>
        <span v-if="progress[name]?.kind === 'running'" class="bl-sc-bar" :style="{ width: `${currentPct * 100}%` }" />
      </button>
    </div>
    <div class="bl-run-foot">
      <button class="bl-checkbox" role="checkbox" :aria-checked="background" :disabled="busy" :title="BACKGROUND_SAMPLE" @click="background = !background">
        <span class="bl-check" :class="{ on: background }" aria-hidden="true">{{ background ? "✓" : "" }}</span>
        Loop a sample track in the background
      </button>
      <span class="bl-spacer" />
      <button v-if="running" class="bl-primary quiet" :disabled="stopping" @click="stopRun">
        {{ stopping ? "Stopping after this scenario…" : "Stop after this scenario" }}
      </button>
      <button v-else class="bl-primary" :disabled="!chosen.length || testing" @click="runScenarios">
        {{
          !chosen.length
            ? "Select a scenario"
            : chosen.length === Object.keys(SCENARIOS).length
              ? "Run everything"
              : `Run ${chosen.length} scenario${chosen.length > 1 ? "s" : ""}`
        }}
      </button>
    </div>
  </section>
</template>

<script setup lang="ts">
import { SCENARIOS } from "@bench/bench.ts";
import { BACKGROUND_SAMPLE } from "@bench/sample-list.ts";
import { duration } from "../format.ts";
import { useBenchLive } from "./state.ts";

const { running, testing, busy, chosen, background, progress, stopping, current, currentPct, runPct, stopRun, runScenarios } = useBenchLive();

function toggleScenario(name: string) {
  chosen.value = chosen.value.includes(name) ? chosen.value.filter((n) => n !== name) : [...chosen.value, name];
}

function scenarioStatus(name: string) {
  const p = progress[name];
  if (!p) {
    const m = SCENARIOS[name].minutes;
    return `~${m < 1 ? `${Math.round(m * 60)} s` : `${m} min`}`;
  }
  if (p.kind === "queued") return "queued";
  if (p.kind === "running") return "running…";
  if (p.kind === "done") return `✓ ${duration(p.seconds ?? 0)}`;
  return `✕ ${p.message}`;
}
</script>

<style scoped>
.bl-progress {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.bl-progress-row {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
}
.bl-progress-row b {
  font-weight: 600;
}
.bl-progress-track {
  height: 10px;
  border-radius: 5px;
  background: var(--bl-surface2);
  overflow: hidden;
}
.bl-progress-track div {
  height: 100%;
  border-radius: 5px;
  background: var(--bl-grad);
  transition: width 0.9s linear;
}
.bl-scenarios {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
  gap: 10px;
}
.bl-sc {
  position: relative;
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 12px;
  align-items: start;
  padding: 14px 14px 16px;
  overflow: hidden;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-surface);
  color: var(--bl-text);
  font-size: 14px;
  text-align: left;
  cursor: pointer;
}
.bl-sc:hover:not(:disabled) {
  background: var(--bl-surface2);
}
.bl-sc:disabled {
  cursor: default;
}
.bl-sc.running {
  border-color: var(--bl-brand);
}
.bl-sc .bl-check {
  margin-top: 1px;
}
.bl-sc-text {
  display: flex;
  flex-direction: column;
  min-width: 0;
}
.bl-sc-name {
  font-weight: 600;
}
.bl-sc-desc {
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-sc-status {
  max-width: 130px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-sc-status.running {
  color: var(--bl-brand);
}
.bl-sc-status.done {
  color: var(--bl-green);
}
.bl-sc-status.failed {
  color: var(--bl-red);
}
.bl-sc-bar {
  position: absolute;
  left: 0;
  bottom: 0;
  height: 3px;
  background: var(--bl-grad);
  transition: width 0.9s linear;
}
</style>
