<template>
  <section class="bl-runner">
    <div class="bl-section-head">
      <h2>Run the e2e tests</h2>
      <p class="bl-muted">
        The tests in <code class="bl-cmd">software/bench/e2e/</code>, as <code class="bl-cmd">npm run bench -- e2e</code>
        runs them: gestures played on the board through the wheel's sensor layer, then checks on what the UI did.
      </p>
    </div>
    <div class="bl-run-foot">
      <span v-if="testSummary" class="bl-mono bl-small bl-muted">{{ testSummary }}</span>
      <span class="bl-spacer" />
      <button v-if="testing" class="bl-primary quiet" @click="testAbort?.abort()">Stop after this test</button>
      <button v-else class="bl-primary" :disabled="running" @click="runTests()">Run all {{ TESTS.length }} tests</button>
    </div>
    <div class="bl-test-groups">
      <div v-for="(group, g) in testGroups" :key="g" class="bl-test-group">
        <div class="bl-label">{{ g }}</div>
        <div v-for="test in group" :key="test.name" class="bl-test" :class="testState[test.name]?.kind">
          <span class="bl-test-icon" aria-hidden="true">{{ testIcon(test.name) }}</span>
          <span class="bl-test-name">{{ test.name }}</span>
          <span class="bl-mono bl-small bl-muted">{{ testMeta(test.name) }}</span>
          <button class="bl-ghost xs" :disabled="busy" @click="runTests([test])">Run</button>
          <div v-if="testState[test.name]?.kind === 'failed'" class="bl-test-error">
            <div v-if="testState[test.name].result?.step">while: {{ testState[test.name].result?.step }}</div>
            <div>{{ testState[test.name].result?.error }}</div>
            <div class="bl-muted">events: <code class="bl-mono">{{ sequence(testState[test.name].result?.events ?? []) || "none" }}</code></div>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed } from "vue";
import { TESTS, sequence, type E2ETest } from "@bench/e2e/index.ts";
import { useBenchLive } from "./state.ts";

const { running, testing, busy, testState, testAbort, runTests } = useBenchLive();

const testGroups = computed(() => {
  const out: Record<string, E2ETest[]> = {};
  for (const t of TESTS) (out[t.group] ??= []).push(t);
  return out;
});
const testSummary = computed(() => {
  const done = Object.values(testState).filter((s) => s.kind !== "running");
  if (!done.length) return "";
  const failed = done.filter((s) => s.kind === "failed").length;
  return `${done.length - failed} passed · ${failed} failed`;
});
const testIcon = (name: string) => ({ running: "…", passed: "✓", failed: "✕" })[testState[name]?.kind ?? ""] ?? "○";
const testMeta = (name: string) => {
  const s = testState[name];
  if (!s) return "";
  if (s.kind === "running") return "running…";
  return `${((s.result?.ms ?? 0) / 1000).toFixed(1)} s`;
};
</script>

<style scoped>
/* E2E tests */
.bl-test-groups {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
  gap: 16px 20px;
}
.bl-test-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.bl-test {
  display: grid;
  grid-template-columns: 18px minmax(0, 1fr) auto auto;
  align-items: center;
  gap: 4px 10px;
  padding: 8px 10px 8px 12px;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-surface);
}
.bl-test.running {
  border-color: var(--bl-brand);
}
.bl-test.failed {
  border-color: var(--bl-red);
}
.bl-test-icon {
  font-weight: 700;
  text-align: center;
  color: var(--bl-muted);
}
.bl-test.running .bl-test-icon {
  color: var(--bl-brand);
}
.bl-test.passed .bl-test-icon {
  color: var(--bl-green);
}
.bl-test.failed .bl-test-icon {
  color: var(--bl-red);
}
.bl-test-name {
  overflow: hidden;
  text-overflow: ellipsis;
}
.bl-test-error {
  grid-column: 2 / -1;
  font-size: 12px;
  color: var(--bl-red);
  overflow-wrap: anywhere;
}
.bl-test-error code {
  font-size: 11px;
}
</style>
