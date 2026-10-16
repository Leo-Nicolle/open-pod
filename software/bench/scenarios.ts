// The bench scenarios: each drives the UI through the bench console the way
// a user would (wheel clicks and scrolls) and returns what it measured.
import { BenchError, Device, type Fields } from "./device.ts";
import { BACKGROUND_SAMPLE, HEAVIEST_SAMPLE, SAMPLES } from "./sample-list.ts";

/** Route_t::RouteType (src/state/router.h). */
export const Route = { ROOT: 0, SEARCH: 1, GENRES: 2, ARTISTS: 3, ALBUMS: 4, TRACKS: 5, NOW_PLAYING: 6 } as const;

export const SAMPLES_ALBUM = "Openpod Bench Samples";

export type Dist = { n: number; p50: number; p95: number; max: number; mean: number };

/** Distribution of `values` (microseconds stay microseconds). */
export function dist(values: number[]): Dist {
  if (!values.length) return { n: 0, p50: 0, p95: 0, max: 0, mean: 0 };
  const s = [...values].sort((a, b) => a - b);
  const q = (p: number) => s[Math.min(s.length - 1, Math.floor(p * s.length))];
  return {
    n: s.length,
    p50: q(0.5),
    p95: q(0.95),
    max: s[s.length - 1],
    mean: Math.round(s.reduce((a, b) => a + b, 0) / s.length),
  };
}

/** What the audio monitor and profiler saw between two device times. */
export function audioWindow(d: Device, from: number, to: number) {
  const audio = d.audio.filter((a) => a.t >= from && a.t <= to);
  const prof = d.prof.filter((p) => p.t >= from && p.t <= to);
  const share = (cat: string) => {
    const busy = prof.reduce((a, p) => a + (p.busyMs[cat] ?? 0), 0);
    const total = prof.reduce((a, p) => a + p.windowMs, 0);
    return total ? Math.round((1000 * busy) / total) / 10 : 0; // % of CPU time
  };
  // The loop's top-level steps: null from firmware that doesn't time them.
  const timesLoop = prof.some((p) => p.busyMs.loop !== undefined);
  const step = (cat: string) => (timesLoop ? share(cat) : null);
  return {
    seconds: audio.length,
    underruns: audio.reduce((a, s) => a + s.underruns, 0),
    minRunwayS: audio.length ? Math.min(...audio.map((s) => s.minRunwayS)) : null,
    meanRateKBs: audio.length ? Math.round(audio.reduce((a, s) => a + s.rateKBs, 0) / audio.length) : null,
    maxStallMs: audio.length ? Math.max(...audio.map((s) => s.stallMs)) : null,
    maxTickGapUs: audio.length ? Math.max(...audio.map((s) => s.tickUs)) : null,
    maxLoopMs: prof.length ? Math.max(...prof.map((p) => p.maxLoopMs)) : null,
    cpuPercent: {
      isr: share("isr"),
      sd: share("sd"),
      psramWrite: share("psW"),
      psramRead: share("psR"),
      ui: share("ui"),
      audio: share("audio"),
      wheel: step("wheel"),
      input: step("input"),
      bench: step("bench"),
      bt: step("bt"),
      misc: step("misc"),
      loop: step("loop"),
    },
  };
}

// --- Navigation helpers -------------------------------------------------

export async function goRoot(d: Device) {
  for (let i = 0; i < 12; i++) {
    const s = await d.state();
    if (+s.route === Route.ROOT) return;
    await d.click("menu"); // Top: back one level (or out of seek-mode)
  }
  throw new BenchError("could not get back to the root menu");
}

/** Moves the highlight to the row labelled `label`, scrolling down from the top. */
export async function selectLabel(d: Device, label: string, maxRows = 400): Promise<Fields> {
  let s = await d.state();
  if (s.item === label) return s;
  // To the top first (scrolling up clamps at row 0).
  for (let guard = 0; +s.sel > 0 && guard < 100; guard++) {
    await d.scroll(Math.min(+s.sel, 24));
    s = await d.state();
  }
  for (let i = 0; i < maxRows; i++) {
    if (s.item === label) return s;
    const before = s.sel;
    await d.scroll(-1);
    s = await d.state();
    if (s.sel === before) break; // end of the list
  }
  if (s.item === label) return s;
  throw new BenchError(`no row "${label}" here (header "${s.header}")`);
}

export async function openRootItem(d: Device, label: string): Promise<number> {
  await goRoot(d);
  await selectLabel(d, label);
  return d.click("select");
}

/** Root > Search, typed query "openpod bench", into the samples album. */
async function openSamplesAlbum(d: Device) {
  await openRootItem(d, "Search");
  await d.send("type", "openpod bench samples");
  await d.click("play"); // Bottom: into the results
  await selectLabel(d, SAMPLES_ALBUM, 20);
  await d.click("select");
  const s = await d.state();
  if (+s.route !== Route.TRACKS) throw new BenchError(`expected the samples' track list, got route ${s.route}`);
}

/** Waits until the playing position reaches `seconds` (or times out). */
async function waitPosition(d: Device, seconds: number, timeoutMs: number): Promise<number | null> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const s = await d.state();
    if (s.playing === "1" && +s.pos >= seconds) return Date.now() - start;
    await d.sleep(100);
  }
  return null;
}

// --- Scenarios ----------------------------------------------------------

export async function scrollScenario(d: Device) {
  await openRootItem(d, "Tracks");
  const from = d.now();
  const single: number[] = [];
  for (let i = 0; i < 200; i++) single.push(await d.scroll(-1));
  const fast: number[] = [];
  for (let i = 0; i < 20; i++) fast.push(await d.scroll(-24));
  for (let i = 0; i < 20; i++) fast.push(await d.scroll(24));
  // Back up through the first rows one click at a time.
  for (let i = 0; i < 50; i++) single.push(await d.scroll(1));
  const end = await d.state();
  return {
    tracks: +end.total,
    singleTickUs: dist(single),
    fastBurstUs: dist(fast),
    ...audioWindow(d, from, d.now()),
  };
}

/**
 * Fast scrolls with the virtual finger (a full turn in 400 ms), so the clicks
 * go through the wheel's lift-off hold and the fast-scroll window like a real
 * flick, unlike scrollScenario's single 24-click events. Timed from the end
 * of each turn to a settled UI.
 */
export async function fastScrollRealScenario(d: Device) {
  await openRootItem(d, "Tracks");
  const from = d.now();
  const settle: number[] = [];
  let events = 0;
  let overlay = 0;
  for (let i = 0; i < 20; i++) {
    const r = await d.spin(i % 2 ? 360 : -360, 400);
    settle.push(r.us);
    events += r.events;
    if ((await d.state()).fast === "1") overlay++;
  }
  return {
    turns: settle.length,
    settleUs: dist(settle),
    /** Turns after which the fast-scroll overlay was showing. */
    overlayTurns: overlay,
    eventsPerTurn: Math.round((10 * events) / settle.length) / 10,
    ...audioWindow(d, from, d.now()),
  };
}

export async function navigationScenario(d: Device) {
  const enter: number[] = [];
  const back: number[] = [];
  const from = d.now();
  enter.push(await openRootItem(d, "Artists"));
  for (let round = 0; round < 6; round++) {
    // A different artist each round, deeper into the list.
    await d.scroll(-(7 + round * 53));
    enter.push(await d.click("select")); // artist -> albums
    enter.push(await d.click("select")); // album -> tracks
    back.push(await d.click("menu"));
    back.push(await d.click("menu"));
  }
  enter.push(await openRootItem(d, "Albums"));
  for (let round = 0; round < 6; round++) {
    await d.scroll(-(11 + round * 97));
    enter.push(await d.click("select"));
    back.push(await d.click("menu"));
  }
  enter.push(await openRootItem(d, "Genres"));
  for (let round = 0; round < 4; round++) {
    await d.scroll(-(3 + round * 5));
    enter.push(await d.click("select")); // genre -> artists
    enter.push(await d.click("select")); // artist -> albums
    back.push(await d.click("menu"));
    back.push(await d.click("menu"));
  }
  return { enterUs: dist(enter), backUs: dist(back), ...audioWindow(d, from, d.now()) };
}

const QUERIES = ["a", "the", "lo", "kalo", "night river", "zz", "openpod bench", "e"];

export async function searchScenario(d: Device) {
  await openRootItem(d, "Search");
  const from = d.now();
  const queries = [];
  for (const q of QUERIES) {
    await d.send("clear");
    const r = await d.send("type", q);
    queries.push({ query: q, totalUs: +r.us, maxCharUs: +r.max_us, hits: +r.hits });
  }
  // Browse the results of a broad query.
  await d.send("clear");
  await d.send("type", "a");
  await d.click("play");
  const browse: number[] = [];
  for (let i = 0; i < 30; i++) browse.push(await d.scroll(-1));
  await d.click("menu"); // back to typing
  await d.send("clear");
  return {
    queries,
    worstCharUs: Math.max(...queries.map((q) => q.maxCharUs)),
    browseUs: dist(browse),
    ...audioWindow(d, from, d.now()),
  };
}

/**
 * Starts the background sample on repeat and goes back to the main menu, so
 * the next scenario browses while it plays. Resolves with how long the track
 * took to start, in ms.
 */
export async function startBackground(d: Device): Promise<number> {
  await d.send("repeat", "1");
  await openSamplesAlbum(d);
  await selectLabel(d, BACKGROUND_SAMPLE, 20);
  await d.click("select");
  const startMs = await waitPosition(d, 1, 10000);
  if (startMs === null) throw new BenchError(`the background sample "${BACKGROUND_SAMPLE}" didn't start`);
  await goRoot(d);
  return startMs;
}

/** Restarts the background sample if it isn't playing (a scenario played or paused something else). */
export async function ensureBackground(d: Device): Promise<boolean> {
  const s = await d.state();
  if (s.playing === "1" && s.repeat === "1" && s.now === BACKGROUND_SAMPLE) return false;
  await startBackground(d);
  return true;
}

export async function stopBackground(d: Device) {
  await d.send("repeat", "0");
  await pause(d);
}

export async function playbackScenario(d: Device, playSeconds = 15) {
  await d.send("repeat", "0"); // next/prev and track ends behave as usual
  await openSamplesAlbum(d);
  const results = [];
  for (const sample of SAMPLES) {
    await selectLabel(d, sample.name, 20);
    const row = await d.state();
    const startUs = await d.click("select");
    const from = d.now();
    const firstSecondMs = await waitPosition(d, 1, 10000);
    await d.sleep(playSeconds * 1000);
    const now = await d.state();

    const seekUs = +(await d.send("seek", "25")).us;
    const seekSettleMs = await waitPosition(d, 26, 5000);
    await d.sleep(2000);
    const playEnd = d.now();

    const nextUs = await d.click("next");
    await d.sleep(1500);
    const prevUs = await d.click("prev"); // restarts the next track
    await d.click("prev"); // back to this one
    await d.sleep(500);
    await d.click("play"); // pause before leaving
    await d.click("menu"); // back to the samples' list

    results.push({
      sample: row.item || now.now,
      durationS: +now.dur,
      uiStartUs: startUs,
      firstSecondMs,
      seekUs,
      seekSettleMs,
      nextUs,
      prevUs,
      ...audioWindow(d, from, playEnd),
    });
  }
  return results;
}

/**
 * The riskiest moment: a track has just started, so its buffer is empty, and
 * the UI is busy at the same time. Each sample is started, then the UI is
 * hammered straight away (screen transitions, broad searches, fast scrolling
 * through the library) while the first seconds of audio are recorded.
 * Compare with the playback scenario, which starts the same tracks idle.
 */
export async function startupScenario(d: Device, watchSeconds = 10) {
  await d.send("repeat", "0");
  const results = [];
  for (const sample of SAMPLES) {
    await openSamplesAlbum(d);
    await selectLabel(d, sample.name, 20);
    const from = d.now();
    const load: { what: string; us: number }[] = [];
    const step = async (what: string, run: () => Promise<number>) => load.push({ what, us: await run() });

    await step("start + slide to Now Playing", () => d.click("select"));
    await step("back to the album", () => d.click("menu"));
    await step("back to the results", () => d.click("menu"));
    await step("back to typing", () => d.click("menu"));
    for (const q of ["the", "a"]) {
      await d.send("clear");
      await step(`search "${q}"`, async () => +(await d.send("type", q)).us);
    }
    await step("leave search", () => d.click("menu"));
    await selectLabel(d, "Tracks");
    await step("open Tracks", () => d.click("select"));
    for (let i = 0; i < 4; i++) await step("fast scroll", () => d.scroll(-24));
    for (let i = 0; i < 2; i++) await step("fast scroll back", () => d.scroll(24));
    const loadMs = d.now() - from;

    // Let the rest of the watch window play out, then read it.
    const until = from + watchSeconds * 1000;
    if (d.now() < until) await d.sleep(until - d.now() + 1200);
    const s = await d.state();
    const trace = d.audio
      .filter((a) => a.t >= from && a.t <= until + 1200)
      .map((a) => ({ s: Math.round((a.t - from) / 100) / 10, runwayS: a.runwayS, underruns: a.underruns }));
    results.push({
      sample: sample.name,
      stillPlaying: s.playing === "1",
      loadMs,
      load,
      trace,
      ...audioWindow(d, from, until + 1200),
    });
    await pause(d);
  }
  return results;
}

/** Pauses whatever plays, via Now Playing. */
async function pause(d: Device) {
  const s = await d.state();
  if (s.playing !== "1") return;
  await goRoot(d);
  await selectLabel(d, "Now Playing");
  await d.click("select");
  await d.click("play");
  await goRoot(d);
}

/** The heaviest sample plays while the library is scrolled hard. */
export async function stressScenario(d: Device, seconds = 25) {
  await openSamplesAlbum(d);
  await selectLabel(d, HEAVIEST_SAMPLE, 20);
  await d.click("select");
  if ((await waitPosition(d, 1, 10000)) === null) throw new BenchError("heaviest sample didn't start");
  await openRootItem(d, "Tracks");
  const from = d.now();
  const ticks: number[] = [];
  let dir = -1;
  while (d.now() - from < seconds * 1000) {
    for (let i = 0; i < 10; i++) ticks.push(await d.scroll(dir * 24));
    for (let i = 0; i < 20; i++) ticks.push(await d.scroll(dir));
    dir = -dir;
  }
  const to = d.now();
  const s = await d.state();
  // Pause it again.
  await goRoot(d);
  await selectLabel(d, "Now Playing");
  await d.click("select");
  await d.click("play");
  return { stillPlaying: s.playing === "1", scrollUs: dist(ticks), ...audioWindow(d, from, to) };
}
