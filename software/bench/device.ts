// The serial link to the bench firmware (src/app/bench.cpp), over any
// transport: Node's serialport (node-serial.ts) or the browser's Web Serial
// (docs/.vitepress/theme/components/bench/web-serial.ts).
//
// Commands go out as "#<cmd> <args>" and are answered by one "#ok <cmd> ..."
// or "#err <cmd> ..." line. Everything else the firmware prints is a log
// line; the audio monitor's once-a-second lines ([prof ...] and runway=...)
// are parsed and kept, so scenarios can read the window they ran in.

/** A line-oriented serial connection. */
export interface Transport {
  write(text: string): void;
  /** Calls `cb` with each received line (without its line ending). */
  onLine(cb: (line: string) => void): void;
  close(): Promise<void>;
}

export type Fields = Record<string, string>;

/**
 * One `[prof <ms>ms] loops=.. maxLoop=..us | sd 12345us max800us 40KB | ...` line.
 * Older firmware printed maxLoop and busy times in whole ms; both are accepted.
 */
export type ProfSample = {
  t: number;
  windowMs: number;
  loops: number;
  /** Worst single main-loop iteration in the window, in ms. */
  maxLoopMs: number;
  /**
   * Busy time per category over the window, in ms (sd, psW, psR, vsClk, vsWait, audio, ui, isr).
   * busyMs / windowMs is the category's share of CPU time. Categories overlap (sd etc. run inside
   * audio or ui, the isr preempts everything), so the shares don't add up to a total.
   */
  busyMs: Record<string, number>;
  maxUs: Record<string, number>;
};

/** One audio monitor line: `runway=3.2s min=2.9s rate=176KB/s ... underrun=0 ...`. */
export type AudioSample = {
  t: number;
  runwayS: number;
  minRunwayS: number;
  rateKBs: number;
  underruns: number;
  stallMs: number;
  tickUs: number;
  loadMs: number;
  refilling: boolean;
};

/** A wheel button, as the bench console names it. */
export type ButtonName = "select" | "menu" | "next" | "play" | "prev";

/**
 * One wheel event the UI handled (`#ev ...`), whatever made it: the real wheel,
 * the virtual finger or an injected command. `finger` is the finger's angle
 * (degrees, 0 = right, counter-clockwise; -1 = lifted), sent at most ~30 times a second.
 */
export type WheelEvent = {
  /** Host time (Device.now()), ms. */
  t: number;
  /** Board time (millis()), ms: exact spacing between events. */
  boardMs: number;
  type: "down" | "up" | "click" | "long" | "verylong" | "scroll" | "touch" | "untouch" | "finger";
  button?: ButtonName;
  /** Scroll clicks (positive = up) for `scroll`. */
  value?: number;
  /** Degrees for `finger`, -1 = lifted. */
  angle?: number;
};

/** What a gesture or input command answers once the UI has settled. */
export type InputResult = {
  /** From the end of the input (the gesture's last keyframe) to a settled UI, µs. */
  us: number;
  /** Wheel events the UI handled meanwhile. */
  events: number;
};

const ANSI = /\x1b\[[0-9;]*m/g;

/** `key=value key2="quoted value"` -> {key: "value", key2: "quoted value"}. */
export function parseFields(text: string): Fields {
  const out: Fields = {};
  const re = /(\w+)=("([^"]*)"|\S+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(text))) out[m[1]] = m[3] ?? m[2];
  return out;
}

export function parseProf(line: string, t: number): ProfSample | null {
  const head = /\[prof (\d+)ms\] loops=(\d+) maxLoop=(\d+)(ms|us)/.exec(line);
  if (!head) return null;
  // To ms, kept at 10 us resolution.
  const ms = (v: string, unit: string) => (unit === "us" ? Math.round(+v / 10) / 100 : +v);
  const s: ProfSample = {
    t,
    windowMs: +head[1],
    loops: +head[2],
    maxLoopMs: ms(head[3], head[4]),
    busyMs: {},
    maxUs: {},
  };
  for (const part of line.split("|").slice(1)) {
    const m = /^\s*(\w+) (\d+)(ms|us) max(\d+)us/.exec(part);
    if (m) {
      s.busyMs[m[1]] = ms(m[2], m[3]);
      s.maxUs[m[1]] = +m[4];
    }
  }
  return s;
}

export function parseEvent(line: string, t: number): WheelEvent | null {
  const m = /^#ev (\w+)\s*(.*)$/.exec(line);
  if (!m) return null;
  const f = parseFields(m[2]);
  const e: WheelEvent = { t, boardMs: +(f.t ?? 0), type: m[1] as WheelEvent["type"] };
  if (f.b) e.button = f.b as ButtonName;
  if (f.v !== undefined) e.value = +f.v;
  if (f.a !== undefined) e.angle = +f.a;
  return e;
}

export function parseAudio(line: string, t: number): AudioSample | null {
  if (!line.includes("runway=") || !line.includes("underrun=")) return null;
  const f = parseFields(line);
  const num = (k: string) => parseFloat(f[k] ?? "0");
  return {
    t,
    runwayS: num("runway"),
    minRunwayS: num("min"),
    rateKBs: num("rate"),
    underruns: num("underrun"),
    stallMs: num("stall"),
    tickUs: num("tick"),
    loadMs: num("load"),
    refilling: line.includes(" refilling"),
  };
}

export class BenchError extends Error {}

type Waiter = {
  cmd: string;
  resolve: (f: Fields) => void;
  reject: (e: Error) => void;
  timer: ReturnType<typeof setTimeout>;
};

export class Device {
  readonly prof: ProfSample[] = [];
  readonly audio: AudioSample[] = [];
  /** Every wheel event echoed by the board (`#ev`), finger positions included. */
  readonly events: WheelEvent[] = [];
  /** Every line received, for the raw log saved next to the report. */
  readonly log: string[] = [];
  private waiters: Waiter[] = [];
  private lineListeners: ((line: string) => void)[] = [];
  private transport: Transport;
  private start = Date.now();

  constructor(transport: Transport) {
    this.transport = transport;
    transport.onLine((raw) => this.onLine(raw.replace(ANSI, "").replace(/\r$/, "")));
  }

  close(): Promise<void> {
    return this.transport.close();
  }

  /** Calls `cb` with every line received from now on; returns an unsubscribe function. */
  subscribe(cb: (line: string, t: number) => void): () => void {
    const l = (line: string) => cb(line, this.now());
    this.lineListeners.push(l);
    return () => {
      this.lineListeners = this.lineListeners.filter((x) => x !== l);
    };
  }

  /** Milliseconds since the port was opened. */
  now(): number {
    return Date.now() - this.start;
  }

  private onLine(line: string) {
    const t = this.now();
    this.log.push(`${t}\t${line}`);
    const prof = parseProf(line, t);
    if (prof) this.prof.push(prof);
    const audio = parseAudio(line, t);
    if (audio) this.audio.push(audio);
    const ev = parseEvent(line, t);
    if (ev) this.events.push(ev);
    for (const l of this.lineListeners) l(line);

    const m = /^#(ok|err) (\S+)\s*(.*)$/.exec(line);
    if (!m) return;
    const i = this.waiters.findIndex((w) => w.cmd === m[2]);
    if (i < 0) return;
    const [w] = this.waiters.splice(i, 1);
    clearTimeout(w.timer);
    if (m[1] === "ok") w.resolve({ ...parseFields(m[3]), _raw: m[3] });
    else w.reject(new BenchError(`#${m[2]}: ${m[3]}`));
  }

  /** Resolves with the first line matching `re` (seen from now on). */
  waitForLine(re: RegExp, timeoutMs: number): Promise<string> {
    return new Promise((resolve, reject) => {
      const listener = (line: string) => {
        if (!re.test(line)) return;
        cleanup();
        resolve(line);
      };
      const timer = setTimeout(() => {
        cleanup();
        reject(new BenchError(`timed out waiting for ${re}`));
      }, timeoutMs);
      const cleanup = () => {
        clearTimeout(timer);
        this.lineListeners = this.lineListeners.filter((l) => l !== listener);
      };
      this.lineListeners.push(listener);
    });
  }

  /** Sends `#<cmd> <args>` and resolves with the fields of its `#ok` answer. */
  send(cmd: string, args = "", timeoutMs = 15000): Promise<Fields> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.waiters = this.waiters.filter((w) => w.timer !== timer);
        reject(new BenchError(`#${cmd} ${args}: no answer in ${timeoutMs} ms`));
      }, timeoutMs);
      this.waiters.push({ cmd, resolve, reject, timer });
      this.transport.write(`#${cmd}${args ? " " + args : ""}\n`);
    });
  }

  /** Waits for the `#boot` line the firmware prints at the end of setup().
   * The ST-Link's serial port has no reset line: flashing, or the reset
   * button, restarts the board. */
  async waitForBoot(timeoutMs = 60000): Promise<Fields> {
    const line = await this.waitForLine(/^#boot /, timeoutMs);
    return parseFields(line);
  }

  // --- Input helpers: each resolves once the UI has settled ---------------

  async click(button: "select" | "menu" | "next" | "play" | "prev"): Promise<number> {
    return +(await this.send("click", button)).us;
  }

  async long(button: "select" | "menu" | "next" | "play" | "prev"): Promise<number> {
    return +(await this.send("long", button)).us;
  }

  /** Negative clicks scroll lists down (towards the end). */
  async scroll(clicks: number): Promise<number> {
    return +(await this.send("scroll", String(clicks))).us;
  }

  async state(): Promise<Fields> {
    return this.send("state");
  }

  // --- Virtual finger: the board plays these through the wheel's sensor
  // layer (debounce, lift-off hold, long-press timing, fast-scroll window),
  // and answers once the gesture has ended and the UI has settled. ---------

  /**
   * Touches at `startDeg`, turns by `deg` over `ms` at a steady speed, stays
   * still `stillMs` (default 100), lifts. Positive = counter-clockwise =
   * scroll up; 15° is one click (24 a turn). At most 2000°/s.
   */
  async spin(deg: number, ms: number, startDeg = 0, stillMs = 100): Promise<InputResult> {
    return result(await this.send("spin", `${Math.round(deg)} ${Math.round(ms)} ${Math.round(startDeg)} ${Math.round(stillMs)}`, ms + stillMs + 15000));
  }

  /** Holds the button(s) for `ms`: a click under 800, a long press from 800, very long from 2000. */
  async hold(buttons: ButtonName | ButtonName[], ms: number): Promise<InputResult> {
    const names = Array.isArray(buttons) ? buttons.join("+") : buttons;
    return result(await this.send("hold", `${names} ${Math.round(ms)}`, ms + 15000));
  }

  /**
   * Raw keyframes: at `at` ms the finger is at `angle` (unwrapped degrees,
   * null = lifted) with `buttons` held; the angle is interpolated between
   * two touching keyframes.
   */
  async gesture(keys: { at: number; angle: number | null; buttons?: ButtonName[] }[]): Promise<InputResult> {
    const text = keys
      .map((k) => `${Math.round(k.at)}:${k.angle === null ? "up" : Math.round(k.angle)}${k.buttons?.length ? "+" + k.buttons.join("+") : ""}`)
      .join(" ");
    const end = keys.length ? keys[keys.length - 1].at : 0;
    return result(await this.send("gesture", text, end + 15000));
  }

  /** Live control (the page's wheel): puts the finger at `angle`, or lifts it (null). */
  finger(angle: number | null): Promise<Fields> {
    return this.send("finger", angle === null ? "up" : String(Math.round(angle)), 2000);
  }

  /** Live control: presses or releases a button. */
  button(name: ButtonName, down: boolean): Promise<Fields> {
    return this.send("btn", `${name} ${down ? 1 : 0}`, 2000);
  }

  sleep(ms: number): Promise<void> {
    return new Promise((r) => setTimeout(r, ms));
  }
}

function result(f: Fields): InputResult {
  return { us: +f.us, events: +(f.events ?? 0) };
}
