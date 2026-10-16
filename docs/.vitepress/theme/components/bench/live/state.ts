// The live page's shared state: the connection, the series parsed from the
// board's lines, errors, the serial log, and the remote's commands. Made once
// per <BenchLive> and handed to its sections with provide/inject.
import { computed, inject, reactive, ref, shallowRef, type InjectionKey } from "vue";
import type { ButtonName, Device, Fields, ProfSample, WheelEvent } from "@bench/device.ts";
import { parseAudio, parseEvent, parseFields, parseProf } from "@bench/device.ts";
import type { Point } from "../LiveChart.vue";
import { connectWebSerial } from "../web-serial.ts";
import { demoDevice } from "../demo-device.ts";
import { createChartPrefs } from "./chart-prefs.ts";
import { createRunner } from "./runner.ts";
import { createE2E } from "./e2e.ts";

const KEEP_MS = 10 * 60 * 1000;

// CPU categories: firmware key, name, and how each line is drawn.
export const CPU = [
  { key: "audio", name: "audio", label: "Audio", color: "var(--bl-brand)", swatch: "var(--bl-brand)", area: true },
  { key: "sd", name: "SD", label: "SD", color: "var(--bl-brand)", swatch: "var(--bl-brand2)", dash: "3 3", width: 1.5 },
  { key: "ui", name: "UI", label: "UI", color: "var(--bl-sgreen)", swatch: "var(--bl-sgreen)", width: 1.75 },
  { key: "isr", name: "feeder", label: "Feeder", color: "var(--bl-muted)", swatch: "var(--bl-muted)", width: 1.5 },
] as const;

const trim = <P extends { t: number }>(a: P[], t: number) => (a.length && a[0].t < t - KEEP_MS ? a.filter((p) => p.t >= t - KEEP_MS) : a);

// --- Errors ------------------------------------------------------------------

export type BenchErr = { id: number; title: string; body: string; details: string };

function createErrors() {
  const errors = ref<BenchErr[]>([]);
  let errorId = 0;

  /** Shows an error callout; the same title again replaces the older one. */
  function fail(title: string, body = "", cause = "", lastLine = "") {
    const details = [new Date().toTimeString().slice(0, 8), cause, lastLine && `last line “${lastLine}”`].filter(Boolean).join(" · ");
    errors.value = [...errors.value.filter((e) => e.title !== title), { id: ++errorId, title, body, details }];
  }
  /** A long message as a title (its first clause) and a body (the rest). */
  function failMessage(message: string, cause = "") {
    const m = /^(.+?)(?:[.:] |$)([\s\S]*)$/.exec(message.trim());
    fail(m ? m[1] : message, m?.[2] ?? "", cause);
  }
  const dismiss = (id: number) => (errors.value = errors.value.filter((e) => e.id !== id));

  return { errors, fail, failMessage, dismiss };
}

// --- Serial log ----------------------------------------------------------------

export type LogKind = "mon" | "cmd" | "ans" | "evt" | "line" | "bad";
export type LogLine = { text: string; kind: LogKind };
const LOG_KEEP = 2000;

/** The once-a-second chatter: profiler, audio monitor, state polls, finger positions. */
const isMonitor = (line: string) =>
  line.includes("[prof ") ||
  line.includes("runway=") ||
  /^[<>] #(ok )?(state|finger)\b/.test(line) ||
  line.startsWith("#ev finger");

function createLog() {
  const logLines = shallowRef<LogLine[]>([]);
  // The log tab's controls, kept while other tabs are shown.
  const logFilter = ref("");
  const hideMonitor = ref(true);
  const logOpen = ref(true);

  function log(t: number, mark: string, line: string, kind: LogKind) {
    const text = `${(t / 1000).toFixed(1).padStart(7)}  ${mark}  ${line}`;
    const next = [...logLines.value, { text, kind }];
    logLines.value = next.length > LOG_KEEP ? next.slice(-LOG_KEEP) : next;
  }

  return { logLines, logFilter, hideMonitor, logOpen, log };
}

// --- Live series (device ms). Plain arrays behind refs, replaced in batches. ---

function createSeries() {
  const runway = ref<Point[]>([]);
  const rate = ref<Point[]>([]);
  const maxLoop = ref<Point[]>([]);
  const latency = ref<Point[]>([]);
  const cpu = reactive<Record<string, Point[]>>({ feeder: [], SD: [], UI: [], audio: [] });
  const underrunBands = ref<[number, number][]>([]);
  const underrunTotal = ref(0);
  const lastAudio = shallowRef<ReturnType<typeof parseAudio>>(null);
  const recentProf = shallowRef<ProfSample[]>([]); // the last few seconds, for the loop breakdown
  const wheelEvents = shallowRef<WheelEvent[]>([]); // what the UI handled (no finger positions)
  const fingerAngle = ref(-1); // where the finger is on the wheel (real or virtual), -1 = none
  const held = ref(new Set<string>()); // buttons down, from the board's events

  function clearCharts() {
    runway.value = [];
    rate.value = [];
    maxLoop.value = [];
    recentProf.value = [];
    wheelEvents.value = [];
    latency.value = [];
    for (const c of CPU) cpu[c.name] = [];
    underrunBands.value = [];
    underrunTotal.value = 0;
  }

  return { runway, rate, maxLoop, latency, cpu, underrunBands, underrunTotal, lastAudio, recentProf, wheelEvents, fingerAngle, held, clearCharts };
}

// --- All of it ---------------------------------------------------------------------

export function createBenchLive() {
  const supported = ref(true);
  const demo = ref(false);
  const device = shallowRef<Device | null>(null);
  const portLabel = ref("");
  const connecting = ref(false);
  const now = ref(0);
  const wallNow = ref(Date.now());
  const lastLineAt = ref(0);
  const windowS = ref(60);
  const tab = ref<"live" | "run" | "log">("live");

  const st = ref<Fields>({});
  const info = reactive<Record<string, string>>({});
  // The remote's fields, kept while other tabs are shown.
  const query = ref("");
  const seekTo = ref(25);

  const running = ref(false);
  const testing = ref(false);
  const busy = computed(() => running.value || testing.value);
  const alive = computed(() => now.value - lastLineAt.value < 3000);

  const errs = createErrors();
  const { fail, failMessage } = errs;
  const logs = createLog();
  const { log } = logs;
  const series = createSeries();
  const prefs = createChartPrefs();

  function onLine(line: string, t: number) {
    const { runway, rate, maxLoop, latency, cpu, underrunBands, underrunTotal, lastAudio, recentProf, wheelEvents, fingerAngle, held } = series;
    lastLineAt.value = t;
    line = line.replace(/\x1b\[[0-9;]*m/g, "");
    const answer = /^#(ok|err) /.test(line);

    const a = parseAudio(line, t);
    log(t, answer ? "<" : " ", line, a && a.underruns > 0 ? "bad" : isMonitor(answer ? `< ${line}` : line) ? "mon" : answer ? "ans" : "line");
    if (a) {
      lastAudio.value = a;
      runway.value = trim([...runway.value, { t, v: a.runwayS }], t);
      rate.value = trim([...rate.value, { t, v: a.rateKBs }], t);
      if (a.underruns > 0) {
        underrunTotal.value += a.underruns;
        underrunBands.value = [...underrunBands.value, [t - 1000, t]];
      }
      return;
    }
    const ev = parseEvent(line, t);
    if (ev) {
      if (ev.type === "finger") fingerAngle.value = ev.angle ?? -1;
      else {
        wheelEvents.value = trim([...wheelEvents.value, ev], t);
        if (ev.type === "down" || ev.type === "up") {
          const next = new Set(held.value);
          if (ev.type === "down") next.add(ev.button!);
          else next.delete(ev.button!);
          held.value = next;
        }
      }
      return;
    }
    const p = parseProf(line, t);
    if (p) {
      maxLoop.value = trim([...maxLoop.value, { t, v: p.maxLoopMs }], t);
      recentProf.value = [...recentProf.value, p].slice(-5);
      // busyMs and windowMs are both ms: the ratio is the share of CPU time over the window.
      for (const c of CPU) cpu[c.name] = trim([...cpu[c.name], { t, v: (100 * (p.busyMs[c.key] ?? 0)) / p.windowMs }], t);
      return;
    }
    const m = /^#(ok|err|boot) ?(\S*)\s*(.*)$/.exec(line);
    if (!m) return;
    if (m[1] === "boot") {
      applyBoot(parseFields(line));
      return;
    }
    const f = parseFields(m[3]);
    if (m[1] === "ok" && m[2] === "state") {
      st.value = f;
      return;
    }
    if (m[1] === "ok" && m[2] === "boot") applyBoot(f);
    if (m[1] === "ok" && m[2] === "dataset") {
      info.tracks = f.tracks_loaded;
      info.albums = f.albums_loaded;
    }
    if (m[1] === "ok" && f.us && ["click", "long", "scroll", "type", "seek"].includes(m[2])) {
      latency.value = trim([...latency.value, { t, v: +f.us / 1000, label: m[2] }], t);
    }
  }

  function applyBoot(f: Fields) {
    info.rev = f.rev;
    info.bootMs = f.ms;
    info.lookupMs = f.lookup_ms;
    info.searchIndexMs = f.search_index_ms;
  }

  /** Logs every command sent, whoever sends it (the remote, the polling, the runner). */
  function logSends(d: Device) {
    const send = d.send.bind(d);
    d.send = (cmd: string, args = "", timeoutMs?: number) => {
      const line = `#${cmd}${args ? " " + args : ""}`;
      log(d.now(), ">", line, isMonitor(`> ${line}`) ? "mon" : "cmd");
      return send(cmd, args, timeoutMs);
    };
  }

  // --- Connection ------------------------------------------------------------------

  let unsubscribe: (() => void) | null = null;
  let ticker: ReturnType<typeof setInterval> | null = null;
  let poll: ReturnType<typeof setInterval> | null = null;

  async function connect(useDemo = demo.value) {
    errs.errors.value = [];
    connecting.value = true;
    demo.value = useDemo;
    try {
      const { device: d, label, stats } = useDemo
        ? { device: demoDevice(), label: "demo board (simulated)", stats: null }
        : await connectWebSerial((e) => {
            if (e instanceof Error && !(e instanceof DOMException)) failMessage(e.message, "port closed");
            else fail("Connection lost", "Reconnect when the board is back.", e ? String(e) : "port closed");
            void disconnect();
          });
      logSends(d);
      device.value = d;
      portLabel.value = label;
      unsubscribe = d.subscribe(onLine);
      ticker = setInterval(() => {
        now.value = d.now();
        wallNow.value = Date.now();
      }, 250);
      // The board may be running for a while already: ask what a fresh boot prints.
      // Asked twice: a board still booting (or old firmware) misses the first.
      await d
        .send("boot", "", 3000)
        .catch(() => d.send("boot", "", 5000))
        .catch(() => {
          const link = stats
            ? `received ${stats.bytesIn} bytes / ${stats.linesIn} lines, sent ${stats.bytesOut} bytes` +
              (stats.writeError ? `, write failed: ${stats.writeError}` : "")
            : "";
          if (stats && stats.bytesIn === 0) fail("Nothing received from the board", "Wrong port, or the browser can't read it.", link);
          else if (stats && stats.bytesOut === 0) fail("The board talks but the browser couldn't send to it", "", link, stats.lastLine);
          else
            fail(
              "The board doesn't answer bench commands",
              "Is the bench firmware flashed? `pio run -e bench -t upload`",
              link,
              stats?.lastLine ?? "",
            );
        });
      await d.send("dataset", "", 3000).catch(() => {});
      startPolling();
    } catch (e) {
      if (e instanceof DOMException && e.name === "NotFoundError") {
        // Picker closed: nothing chosen, or nothing listed.
        fail(
          "No port chosen",
          "If the Nucleo wasn't listed, check its USB cable. With Chromium from snap, run `sudo snap connect chromium:raw-usb` then restart Chromium.",
        );
      } else failMessage(e instanceof Error ? e.message : String(e), "connect");
    } finally {
      connecting.value = false;
    }
  }

  async function disconnect() {
    stopPolling();
    if (ticker) clearInterval(ticker);
    ticker = null;
    unsubscribe?.();
    unsubscribe = null;
    const d = device.value;
    device.value = null;
    await d?.close().catch(() => {});
  }

  async function reconnect() {
    await disconnect();
    await connect();
  }

  function startPolling() {
    stopPolling();
    poll = setInterval(() => {
      if (!busy.value) device.value?.send("state", "", 2000).catch(() => {});
    }, 700);
  }
  function stopPolling() {
    if (poll) clearInterval(poll);
    poll = null;
  }

  // --- Remote commands (the remote panel and the keyboard) -----------------------

  async function press(cmd: string, arg: string) {
    const d = device.value;
    if (!d || busy.value) return;
    try {
      await d.send(cmd, arg);
      void d.send("state", "", 2000).catch(() => {});
    } catch (e) {
      fail(`#${cmd}${arg ? ` ${arg}` : ""} failed`, e instanceof Error ? e.message : String(e));
    }
  }

  // Briefly lights a wheel label when it's pressed from here.
  const flashed = ref<string | null>(null);
  let flashTimer: ReturnType<typeof setTimeout> | null = null;
  function flash(name: string) {
    flashed.value = name;
    if (flashTimer) clearTimeout(flashTimer);
    flashTimer = setTimeout(() => (flashed.value = null), 180);
  }
  const lit = (name: string) => flashed.value === name || series.held.value.has(name);

  function click(name: ButtonName) {
    flash(name);
    void press("click", name);
  }
  function longPress() {
    flash("select");
    void press("long", "select");
  }

  const base = {
    supported,
    demo,
    device,
    portLabel,
    connecting,
    now,
    wallNow,
    windowS,
    tab,
    st,
    info,
    query,
    seekTo,
    running,
    testing,
    busy,
    alive,
    ...errs,
    ...logs,
    ...series,
    ...prefs,
    connect,
    disconnect,
    reconnect,
    press,
    flash,
    lit,
    click,
    longPress,
  };
  return { ...base, ...createRunner(base), ...createE2E(base) };
}

export type BenchLive = ReturnType<typeof createBenchLive>;
export const BENCH_LIVE: InjectionKey<BenchLive> = Symbol("bench-live");

export function useBenchLive(): BenchLive {
  const ctx = inject(BENCH_LIVE);
  if (!ctx) throw new Error("useBenchLive() outside <BenchLive>");
  return ctx;
}
