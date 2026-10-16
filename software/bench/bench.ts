// Runs the bench scenarios on a connected Device and builds the report.
// Shared by the CLI (cli.ts) and the docs' live page, so both produce the
// same reports.
import type { Device, Fields } from "./device.ts";
import type { Report } from "./metrics.ts";
import {
  ensureBackground,
  fastScrollRealScenario,
  navigationScenario,
  playbackScenario,
  scrollScenario,
  searchScenario,
  startBackground,
  startupScenario,
  stopBackground,
  stressScenario,
} from "./scenarios.ts";
import { BACKGROUND_SAMPLE } from "./sample-list.ts";

/** `ownAudio`: the scenario plays its own tracks, so the background sample doesn't run during it. */
export const SCENARIOS: Record<
  string,
  { run: (d: Device) => Promise<unknown>; label: string; desc: string; minutes: number; ownAudio?: boolean }
> = {
  scroll: { run: scrollScenario, label: "Scroll the track list", desc: "Row by row, then 24-row jumps", minutes: 0.5 },
  fastScrollReal: { run: fastScrollRealScenario, label: "Fast scroll, real finger", desc: "Full-turn flicks with the virtual finger", minutes: 0.5 },
  navigation: { run: navigationScenario, label: "Open and leave lists", desc: "Into the main menu's lists and back", minutes: 1 },
  search: { run: searchScenario, label: "Search queries", desc: "Typed queries, then their results scrolled", minutes: 0.5 },
  startup: { run: startupScenario, label: "Start tracks under load", desc: "Each sample started while the UI is hammered", minutes: 3, ownAudio: true },
  playback: { run: playbackScenario, label: "Play every sample", desc: "Each sample played from an idle UI", minutes: 3.5, ownAudio: true },
  stress: { run: stressScenario, label: "Scroll while playing", desc: "The heaviest sample, library scrolled hard", minutes: 0.7, ownAudio: true },
};

export type Progress =
  | { kind: "start"; scenario: string; index: number; total: number }
  | { kind: "done"; scenario: string; seconds: number }
  | { kind: "failed"; scenario: string; message: string };

export type RunOptions = {
  scenarios?: string[];
  /** The `#boot` line's fields, when the host saw the board boot. Asked with `#boot` otherwise. */
  boot?: Fields;
  host?: string;
  onProgress?: (p: Progress) => void;
  /** Checked between scenarios. */
  signal?: AbortSignal;
  /** Loop BACKGROUND_SAMPLE while the browsing scenarios run (default true). */
  background?: boolean;
};

export async function runBench(d: Device, opts: RunOptions = {}): Promise<Report> {
  const names = opts.scenarios ?? Object.keys(SCENARIOS);
  for (const n of names) if (!SCENARIOS[n]) throw new Error(`unknown scenario ${n} (${Object.keys(SCENARIOS).join(", ")})`);

  const boot = opts.boot ?? (await d.send("boot"));
  const dataset = await d.send("dataset");
  const json = dataset._raw.match(/^\{.*?\}/)?.[0];
  const report: Report = {
    rev: boot.rev,
    date: new Date().toISOString(),
    host: opts.host ?? "",
    dataset: { ...(json ? JSON.parse(json) : {}), tracksLoaded: +dataset.tracks_loaded, albumsLoaded: +dataset.albums_loaded },
    boot: { ms: +boot.ms, lookup_ms: +boot.lookup_ms, search_index_ms: +boot.search_index_ms },
    scenarios: {},
    errors: [],
  };
  const bg: NonNullable<Report["background"]> = { enabled: opts.background ?? true, sample: null, restarts: 0 };
  report.background = bg;
  if (bg.enabled) {
    bg.sample = BACKGROUND_SAMPLE;
    try {
      bg.startMs = await startBackground(d);
    } catch (e) {
      report.errors.push({ scenario: "background", message: e instanceof Error ? e.message : String(e) });
      bg.enabled = false;
    }
  }

  for (const [i, name] of names.entries()) {
    if (opts.signal?.aborted) {
      report.errors.push({ scenario: name, message: "stopped" });
      continue;
    }
    const t0 = Date.now();
    opts.onProgress?.({ kind: "start", scenario: name, index: i, total: names.length });
    try {
      if (bg.enabled && !SCENARIOS[name].ownAudio && (await ensureBackground(d))) bg.restarts++;
      report.scenarios[name] = await SCENARIOS[name].run(d);
      opts.onProgress?.({ kind: "done", scenario: name, seconds: (Date.now() - t0) / 1000 });
    } catch (e) {
      const message = e instanceof Error ? e.message : String(e);
      report.errors.push({ scenario: name, message });
      opts.onProgress?.({ kind: "failed", scenario: name, message });
    }
  }
  if (bg.enabled) await stopBackground(d).catch(() => {});
  report.scenarios.stats = await d.send("stats").catch(() => ({}));
  return report;
}
