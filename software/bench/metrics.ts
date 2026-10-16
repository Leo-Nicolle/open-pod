// Bench report shape, headline metrics and comparison. No Node imports: the
// docs pages (trends, live runner) use it too.
export type Report = {
  rev: string;
  date: string;
  host: string;
  dataset: Record<string, unknown>;
  boot: Record<string, number>;
  scenarios: Record<string, any>;
  /** The sample looping while the browsing scenarios ran. */
  background?: { enabled: boolean; sample: string | null; restarts: number; startMs?: number };
  errors: { scenario: string; message: string }[];
};

export type Metric = {
  key: string;
  label: string;
  unit: "ms" | "us" | "s" | "count" | "%";
  /** Which way is an improvement. */
  better: "lower" | "higher";
  group: string;
};

/** The headline numbers: what `compare` and the docs trends track. */
export const METRICS: Metric[] = [
  { key: "boot.ms", label: "Boot to ready", unit: "ms", better: "lower", group: "Boot" },
  { key: "boot.lookup_ms", label: "Library index load", unit: "ms", better: "lower", group: "Boot" },
  { key: "boot.search_index_ms", label: "Search index load", unit: "ms", better: "lower", group: "Boot" },
  { key: "scroll.single.p95", label: "Scroll one row (p95)", unit: "us", better: "lower", group: "Browsing" },
  { key: "scroll.fast.p95", label: "Fast scroll burst (p95)", unit: "us", better: "lower", group: "Browsing" },
  { key: "scroll.real_fast.p95", label: "Fast scroll, real finger (p95)", unit: "us", better: "lower", group: "Browsing" },
  { key: "nav.enter.p95", label: "Open a list (p95)", unit: "us", better: "lower", group: "Browsing" },
  { key: "nav.back.p95", label: "Go back (p95)", unit: "us", better: "lower", group: "Browsing" },
  { key: "search.worst_char", label: "Search, slowest keystroke", unit: "us", better: "lower", group: "Search" },
  { key: "search.browse.p95", label: "Scroll search results (p95)", unit: "us", better: "lower", group: "Search" },
  { key: "browse.underruns", label: "Underruns while browsing", unit: "count", better: "lower", group: "Browsing" },
  { key: "browse.min_runway", label: "Runway while browsing", unit: "s", better: "higher", group: "Browsing" },
  { key: "startup.underruns", label: "Underruns, track start under load", unit: "count", better: "lower", group: "Track start" },
  { key: "startup.min_runway", label: "Lowest runway, track start under load", unit: "s", better: "higher", group: "Track start" },
  { key: "startup.max_loop", label: "Longest main loop, track start under load", unit: "ms", better: "lower", group: "Track start" },
  { key: "playback.underruns", label: "Underruns, all samples", unit: "count", better: "lower", group: "Playback" },
  { key: "playback.min_runway", label: "Lowest buffer runway", unit: "s", better: "higher", group: "Playback" },
  { key: "playback.start.max", label: "Slowest track start", unit: "ms", better: "lower", group: "Playback" },
  { key: "playback.isr_max", label: "Feeder CPU, heaviest sample", unit: "%", better: "lower", group: "Playback" },
  { key: "stress.underruns", label: "Underruns, scrolling while playing", unit: "count", better: "lower", group: "Stress" },
  { key: "stress.min_runway", label: "Runway, scrolling while playing", unit: "s", better: "higher", group: "Stress" },
  { key: "stress.max_loop", label: "Longest main loop, stress", unit: "ms", better: "lower", group: "Stress" },
  { key: "bench.overhead_max", label: "Bench's own CPU, worst scenario", unit: "%", better: "lower", group: "Bench" },
];

/** Flattens a report into METRICS keys (missing ones are left out). */
export function summarize(r: Report): Record<string, number> {
  const s = r.scenarios;
  const out: Record<string, number> = {};
  const set = (k: string, v: unknown) => {
    if (typeof v === "number" && Number.isFinite(v)) out[k] = v;
  };
  set("boot.ms", r.boot.ms);
  set("boot.lookup_ms", r.boot.lookup_ms);
  set("boot.search_index_ms", r.boot.search_index_ms);
  set("scroll.single.p95", s.scroll?.singleTickUs?.p95);
  set("scroll.fast.p95", s.scroll?.fastBurstUs?.p95);
  set("scroll.real_fast.p95", s.fastScrollReal?.settleUs?.p95);
  set("nav.enter.p95", s.navigation?.enterUs?.p95);
  set("nav.back.p95", s.navigation?.backUs?.p95);
  set("search.worst_char", s.search?.worstCharUs);
  set("search.browse.p95", s.search?.browseUs?.p95);
  // The browsing scenarios, with the background sample playing.
  const browsing = [s.scroll, s.fastScrollReal, s.navigation, s.search].filter((x) => x && x.seconds > 0);
  if (r.background?.enabled && browsing.length) {
    set("browse.underruns", browsing.reduce((a, x) => a + (x.underruns ?? 0), 0));
    const runways = browsing.map((x) => x.minRunwayS).filter((v) => typeof v === "number");
    if (runways.length) set("browse.min_runway", Math.min(...runways));
  }
  const st: any[] = s.startup ?? [];
  if (st.length) {
    set("startup.underruns", st.reduce((a, x) => a + (x.underruns ?? 0), 0));
    const runways = st.map((x) => x.minRunwayS).filter((v) => typeof v === "number");
    if (runways.length) set("startup.min_runway", Math.min(...runways));
    const loops = st.map((x) => x.maxLoopMs).filter((v) => typeof v === "number");
    if (loops.length) set("startup.max_loop", Math.max(...loops));
  }
  const pb: any[] = s.playback ?? [];
  if (pb.length) {
    set("playback.underruns", pb.reduce((a, p) => a + (p.underruns ?? 0), 0));
    const runways = pb.map((p) => p.minRunwayS).filter((v) => typeof v === "number");
    if (runways.length) set("playback.min_runway", Math.min(...runways));
    const starts = pb.map((p) => p.firstSecondMs).filter((v) => typeof v === "number");
    if (starts.length) set("playback.start.max", Math.max(...starts));
    set("playback.isr_max", Math.max(...pb.map((p) => p.cpuPercent?.isr ?? 0)));
  }
  set("stress.underruns", s.stress?.underruns);
  set("stress.min_runway", s.stress?.minRunwayS);
  set("stress.max_loop", s.stress?.maxLoopMs);
  // What the console and monitor lines cost (firmware that times them only).
  const windows = [s.scroll, s.fastScrollReal, s.navigation, s.search, ...st, ...pb, s.stress];
  const bench = windows.map((x) => x?.cpuPercent?.bench).filter((v) => typeof v === "number");
  if (bench.length) set("bench.overhead_max", Math.max(...bench));
  return out;
}

/** File-name-safe revision ("v0.3-12-gabc123-dirty"). */
export function revFile(rev: string): string {
  return rev.replace(/[^\w.-]/g, "_");
}

export type IndexEntry = { rev: string; date: string; file: string; dataset: Record<string, unknown>; summary: Record<string, number> };

function fmt(v: number | undefined, unit: string): string {
  if (v === undefined) return "-";
  if (unit === "us" && v >= 1000) return `${(v / 1000).toFixed(1)} ms`;
  return `${Math.round(v * 100) / 100} ${unit === "count" ? "" : unit}`.trim();
}

/** A table of every metric, A vs B, flagging changes beyond `threshold`. */
export function compareText(a: Report, b: Report, threshold = 0.1): { text: string; regressions: number } {
  const sa = summarize(a);
  const sb = summarize(b);
  const rows: string[][] = [["metric", a.rev, b.rev, "change", ""]];
  let regressions = 0;
  for (const m of METRICS) {
    const va = sa[m.key];
    const vb = sb[m.key];
    let change = "";
    let flag = "";
    if (va !== undefined && vb !== undefined) {
      const delta = vb - va;
      const rel = va === 0 ? (vb === 0 ? 0 : Infinity) : delta / Math.abs(va);
      change = va === 0 ? (delta ? `+${delta}` : "0") : `${rel >= 0 ? "+" : ""}${(rel * 100).toFixed(0)}%`;
      const worse = m.better === "lower" ? delta > 0 : delta < 0;
      // Counts (underruns) regress on any increase from zero.
      const significant = Math.abs(rel) > threshold || (m.unit === "count" && va === 0 && vb > 0);
      if (significant) {
        flag = worse ? "REGRESSION" : "better";
        if (worse) regressions++;
      }
    }
    rows.push([m.label, fmt(va, m.unit), fmt(vb, m.unit), change, flag]);
  }
  const widths = rows[0].map((_, i) => Math.max(...rows.map((r) => r[i].length)));
  const text = rows.map((r) => r.map((c, i) => c.padEnd(widths[i])).join("  ")).join("\n");
  return { text, regressions };
}
