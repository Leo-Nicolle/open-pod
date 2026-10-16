// The scenario runner's state: kept in the page, so a run carries on (and its
// results stay) while other tabs are shown.
import { computed, reactive, ref, shallowRef, type Ref, type ShallowRef } from "vue";
import { withBase } from "vitepress";
import type { Device } from "@bench/device.ts";
import { SCENARIOS, runBench } from "@bench/bench.ts";
import type { Report } from "@bench/metrics.ts";
import type { LogKind } from "./state.ts";

export type ScenarioProgress = { kind: "queued" | "running" | "done" | "failed"; seconds?: number; message?: string };
export type Reference = { rev: string; date?: string; summary: Record<string, number> };

type Deps = {
  device: ShallowRef<Device | null>;
  running: Ref<boolean>;
  tab: Ref<"live" | "run" | "log">;
  wallNow: Ref<number>;
  log: (t: number, mark: string, line: string, kind: LogKind) => void;
  failMessage: (message: string, cause?: string) => void;
};

export function createRunner({ device, running, tab, wallNow, log, failMessage }: Deps) {
  const chosen = ref<string[]>(Object.keys(SCENARIOS));
  const background = ref(true);
  const progress = reactive<Record<string, ScenarioProgress>>({});
  const abort = shallowRef<AbortController | null>(null);
  const stopping = ref(false);
  const current = ref<{ name: string; index: number; total: number; startedAt: number } | null>(null);
  const report = shallowRef<Report | null>(null);
  const reportJson = ref("");
  const reference = ref<Reference | null>(null);

  /** The running scenario's progress, from its usual duration (it never claims to be done). */
  const currentPct = computed(() => {
    const c = current.value;
    if (!c) return 0;
    return Math.min(0.95, (wallNow.value - c.startedAt) / (SCENARIOS[c.name].minutes * 60_000));
  });
  const runPct = computed(() => (current.value ? (100 * (current.value.index + currentPct.value)) / current.value.total : 0));

  function stopRun() {
    stopping.value = true;
    abort.value?.abort();
  }

  async function runScenarios() {
    const d = device.value;
    if (!d) return;
    const names = Object.keys(SCENARIOS).filter((n) => chosen.value.includes(n));
    running.value = true;
    stopping.value = false;
    report.value = null;
    tab.value = "run";
    for (const k of Object.keys(progress)) delete progress[k];
    for (const n of names) progress[n] = { kind: "queued" };
    abort.value = new AbortController();
    log(d.now(), " ", `run  ${names.join(", ")}`, "evt");
    try {
      const r = await runBench(d, {
        scenarios: names,
        host: "browser",
        background: background.value,
        signal: abort.value.signal,
        onProgress: (p) => {
          if (p.kind === "start") {
            progress[p.scenario] = { kind: "running" };
            current.value = { name: p.scenario, index: p.index, total: p.total, startedAt: Date.now() };
          } else if (p.kind === "done") progress[p.scenario] = { kind: "done", seconds: p.seconds };
          else progress[p.scenario] = { kind: "failed", message: p.message };
          log(d.now(), " ", `run  ${p.kind} ${p.scenario}${p.kind === "failed" ? `: ${p.message}` : ""}`, "evt");
        },
      });
      reportJson.value = JSON.stringify({ ...r, log: d.log }, null, 2);
      report.value = r;
      await loadReference(r.rev);
    } catch (e) {
      failMessage(e instanceof Error ? e.message : String(e), "bench run");
    } finally {
      // Stopped before their turn: back to idle.
      for (const n of names) if (progress[n]?.kind === "queued") delete progress[n];
      running.value = false;
      stopping.value = false;
      current.value = null;
      abort.value = null;
    }
  }

  /** The latest published report other than this revision, to compare with. */
  async function loadReference(rev: string) {
    try {
      const index = await (await fetch(withBase("/bench/index.json"))).json();
      const runs = (index.runs ?? []).filter((r: any) => r.rev !== rev);
      reference.value = runs.length ? runs[runs.length - 1] : null;
    } catch {
      reference.value = null;
    }
  }

  return { chosen, background, progress, stopping, current, report, reportJson, reference, currentPct, runPct, stopRun, runScenarios };
}
