// Shared numbers and formatting for the live bench page.

/** The UI's frame budget: AnimationManager::FRAME_TIME_US (src/rendering/animation_manager.h). */
export const FRAME_MS = 16.667;

/** A number for a chart axis or readout, with as many decimals as its size deserves. */
export function fmtN(v: number | null | undefined, unit = ""): string {
  if (v == null || Number.isNaN(v)) return "—";
  if (unit === "µs") return Math.round(v).toLocaleString("en-US");
  if (unit === "s") return v.toFixed(1);
  if (v >= 100) return Math.round(v).toString();
  if (v >= 10) return v.toFixed(unit === "ms" ? 1 : 0);
  return v.toFixed(1);
}

/** m:ss */
export function clock(s: number) {
  if (!Number.isFinite(s)) return "–:––";
  return `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;
}

/** A metric value and its unit, apart (the unit is drawn muted). */
export function fmtParts(v: number | undefined, unit: string): [string, string] {
  if (v === undefined) return ["–", ""];
  if (unit === "us") return v >= 1000 ? [(v / 1000).toFixed(1), "ms"] : [String(Math.round(v)), "µs"];
  if (unit === "count") return [String(v), ""];
  return [String(Math.round(v * 100) / 100), unit];
}

/** 45 s, 1 m 52 s */
export function duration(s: number) {
  s = Math.round(s);
  return s < 60 ? `${s} s` : `${Math.floor(s / 60)} m ${String(s % 60).padStart(2, "0")} s`;
}
