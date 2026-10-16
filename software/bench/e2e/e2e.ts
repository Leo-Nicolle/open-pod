// End-to-end tests: each drives the pod through the bench console - mostly
// with the virtual finger (Device.spin/hold/gesture), which goes through the
// wheel's sensor layer like a real finger - and checks what the UI did.
// Shared by the CLI (`npm run bench -- e2e`) and the docs' live page, so no
// Node imports here.
import type { Device, Fields, WheelEvent } from "../device.ts";
import { goRoot, openRootItem, selectLabel } from "../scenarios.ts";

export class E2EFailure extends Error {}

export type TestContext = {
  d: Device;
  /** A fresh `#state`. */
  state(): Promise<Fields>;
  /** Wheel events the UI handled since the test started (finger positions left out). */
  events(): WheelEvent[];
  /** Same, since `mark()` was called. */
  eventsSince(mark: number): WheelEvent[];
  /** A point to count events from (see eventsSince). */
  mark(): number;
  /** Polls the state until `pred` holds; fails after `timeoutMs`. */
  waitFor(what: string, pred: (s: Fields) => boolean, timeoutMs?: number): Promise<Fields>;
  /** Notes what the test is doing, shown with a failure. */
  step(what: string): void;
  sleep(ms: number): Promise<void>;
  goRoot(): Promise<void>;
  openRootItem(label: string): Promise<void>;
  selectLabel(label: string): Promise<Fields>;
};

export type E2ETest = {
  name: string;
  group: string;
  run: (t: TestContext) => Promise<void>;
};

export type E2EResult = {
  name: string;
  group: string;
  ok: boolean;
  ms: number;
  /** The step the test was on, and why it failed. */
  step?: string;
  error?: string;
  /** Wheel events handled during the test (finger positions left out). */
  events: WheelEvent[];
};

export type E2EProgress = { kind: "start"; name: string } | { kind: "done"; result: E2EResult };

// --- Assertions -----------------------------------------------------------

export function check(cond: unknown, message: string): asserts cond {
  if (!cond) throw new E2EFailure(message);
}

export function equal<T>(actual: T, expected: T, what: string) {
  if (actual !== expected) throw new E2EFailure(`${what}: expected ${String(expected)}, got ${String(actual)}`);
}

/** Event types in order, e.g. "down up click" (scrolls as "scroll-2"). */
export function sequence(events: WheelEvent[]): string {
  return events.map((e) => (e.type === "scroll" ? `scroll${e.value}` : e.button ? `${e.type}:${e.button}` : e.type)).join(" ");
}

/** Sum of the scroll clicks in `events`. */
export function scrolled(events: WheelEvent[]): number {
  return events.filter((e) => e.type === "scroll").reduce((a, e) => a + (e.value ?? 0), 0);
}

// --- Runner ---------------------------------------------------------------

export async function runE2E(
  d: Device,
  tests: E2ETest[],
  opts: { onProgress?: (p: E2EProgress) => void; signal?: AbortSignal } = {},
): Promise<E2EResult[]> {
  const results: E2EResult[] = [];
  for (const test of tests) {
    if (opts.signal?.aborted) break;
    opts.onProgress?.({ kind: "start", name: test.name });
    const first = d.events.length;
    const t0 = Date.now();
    let step = "";
    const own = (from: number) => d.events.slice(from).filter((e) => e.type !== "finger");
    const t: TestContext = {
      d,
      state: () => d.state(),
      events: () => own(first),
      eventsSince: (mark) => own(mark),
      mark: () => d.events.length,
      async waitFor(what, pred, timeoutMs = 3000) {
        const start = Date.now();
        let s = await d.state();
        while (!pred(s)) {
          if (Date.now() - start > timeoutMs) throw new E2EFailure(`${what} (not within ${timeoutMs} ms)`);
          await d.sleep(50);
          s = await d.state();
        }
        return s;
      },
      step: (what) => void (step = what),
      sleep: (ms) => d.sleep(ms),
      goRoot: () => goRoot(d),
      openRootItem: async (label) => void (await openRootItem(d, label)),
      selectLabel: (label) => selectLabel(d, label),
    };
    const result: E2EResult = { name: test.name, group: test.group, ok: true, ms: 0, events: [] };
    try {
      await test.run(t);
    } catch (e) {
      result.ok = false;
      result.step = step || undefined;
      result.error = e instanceof Error ? e.message : String(e);
    }
    result.ms = Date.now() - t0;
    result.events = own(first);
    results.push(result);
    opts.onProgress?.({ kind: "done", result });
  }
  return results;
}
