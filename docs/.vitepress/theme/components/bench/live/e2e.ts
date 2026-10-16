// The e2e tests' state: kept in the page, so a run carries on (and its results
// stay) while other tabs are shown.
import { reactive, shallowRef, type Ref, type ShallowRef } from "vue";
import type { Device } from "@bench/device.ts";
import { TESTS, runE2E, type E2EResult, type E2ETest } from "@bench/e2e/index.ts";
import type { LogKind } from "./state.ts";

export type TestState = { kind: "running" | "passed" | "failed"; result?: E2EResult };

type Deps = {
  device: ShallowRef<Device | null>;
  testing: Ref<boolean>;
  busy: Readonly<Ref<boolean>>;
  log: (t: number, mark: string, line: string, kind: LogKind) => void;
  failMessage: (message: string, cause?: string) => void;
};

export function createE2E({ device, testing, busy, log, failMessage }: Deps) {
  const testState = reactive<Record<string, TestState>>({});
  const testAbort = shallowRef<AbortController | null>(null);

  async function runTests(tests: E2ETest[] = TESTS) {
    const d = device.value;
    if (!d || busy.value) return;
    testing.value = true;
    for (const t of tests) delete testState[t.name];
    testAbort.value = new AbortController();
    log(d.now(), " ", `run  e2e ${tests.length === TESTS.length ? "all" : tests.map((t) => t.name).join(", ")}`, "evt");
    try {
      await runE2E(d, tests, {
        signal: testAbort.value.signal,
        onProgress: (p) => {
          if (p.kind === "start") testState[p.name] = { kind: "running" };
          else {
            testState[p.result.name] = { kind: p.result.ok ? "passed" : "failed", result: p.result };
            log(d.now(), " ", `run  ${p.result.ok ? "passed" : "failed"} ${p.result.name}`, "evt");
          }
        },
      });
    } catch (e) {
      failMessage(e instanceof Error ? e.message : String(e), "e2e");
    } finally {
      testing.value = false;
      testAbort.value = null;
    }
  }

  return { testState, testAbort, runTests };
}
