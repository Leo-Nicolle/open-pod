// Which live charts are shown, and their units (kept between visits).
import { reactive, watch } from "vue";
import { FRAME_MS } from "../format.ts";

export const CHIPS = [
  { key: "breakdown", label: "Loop breakdown" },
  { key: "events", label: "Wheel events" },
  { key: "runway", label: "Buffer runway" },
  { key: "cpu", label: "CPU time" },
  { key: "loop", label: "Longest loop" },
  { key: "input", label: "Input → settled" },
  { key: "stream", label: "Stream rate" },
] as const;
export type ChartKey = (typeof CHIPS)[number]["key"];

export const CPU_UNITS = [
  { value: "pct" as const, label: "% of 1 s" },
  { value: "ms" as const, label: "ms per s" },
];
export const TIME_UNITS = [
  { value: "ms" as const, label: "ms" },
  { value: "us" as const, label: "µs" },
  { value: "frame" as const, label: "% of frame" },
];
export const BREAKDOWN_UNITS = [
  { value: "pct" as const, label: "% of loop" },
  { value: "us" as const, label: "µs per loop" },
  { value: "frame" as const, label: "% of frame" },
];
const STORE = { charts: "bench-live.charts", units: "bench-live.units" };

export const timeUnit = (u: "ms" | "us" | "frame") =>
  u === "us" ? { k: 1000, u: "µs" } : u === "frame" ? { k: 100 / FRAME_MS, u: "% frame" } : { k: 1, u: "ms" };

export function createChartPrefs() {
  const shown = reactive<Record<ChartKey, boolean>>({
    breakdown: true,
    events: true,
    runway: true,
    cpu: true,
    loop: true,
    input: true,
    stream: true,
  });
  const units = reactive({
    cpu: "pct" as "pct" | "ms",
    loop: "ms" as "ms" | "us" | "frame",
    input: "ms" as "ms" | "us" | "frame",
    breakdown: "pct" as "pct" | "us" | "frame",
  });

  function restore() {
    try {
      Object.assign(shown, JSON.parse(localStorage.getItem(STORE.charts) ?? "{}"));
      Object.assign(units, JSON.parse(localStorage.getItem(STORE.units) ?? "{}"));
    } catch {
      // Storage blocked or garbled: the defaults do.
    }
  }
  function save(key: string, value: object) {
    try {
      localStorage.setItem(key, JSON.stringify(value));
    } catch {
      // Not kept: fine.
    }
  }
  watch(shown, (v) => save(STORE.charts, v));
  watch(units, (v) => save(STORE.units, v));

  return { shown, units, restore };
}
