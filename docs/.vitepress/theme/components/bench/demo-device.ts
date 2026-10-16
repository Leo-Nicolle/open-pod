// A pretend bench board for the live page's demo mode (?demo): answers the
// bench commands and prints audio monitor lines like the firmware does, so the
// page can be tried (and developed) without a Nucleo. Scenarios don't run on
// it: there's no real UI behind the answers. A small model of the wheel turns
// the virtual finger (#finger/#btn/#spin/#hold/#gesture) into "#ev" lines with
// the firmware's rules: 15° a click, clicks held 80 ms and dropped on lift-off,
// 20 ms debounce, long press at 800 ms, very long at 2000 ms.
import { Device, type Transport } from "@bench/device.ts";

export function demoDevice(): Device {
  const listeners: ((line: string) => void)[] = [];
  const emit = (line: string) => listeners.forEach((l) => l(line));
  const start = Date.now();
  let sel = 0;
  let playing = true;
  let pos = 12;
  let runway = 3.2;
  let busyUntil = 0;
  let fastUntil = 0;
  let fastWindow = { start: 0, clicks: 0 };

  // --- Wheel model -------------------------------------------------------
  const BUTTONS = ["select", "menu", "next", "play", "prev"];
  type Key = { at: number; angle: number | null; buttons: number };
  const finger = { angle: null as number | null, buttons: 0 };
  const wheel = {
    last: null as number | null,
    acc: 0,
    held: [] as { at: number; clicks: number }[],
    raw: BUTTONS.map(() => ({ down: false, since: 0 })),
    btn: BUTTONS.map(() => ({ pressed: false, start: 0, long: false, vlong: false })),
    echoAt: 0,
    echoed: -1,
  };
  let gesture: { cmd: string; keys: Key[]; start: number; events: number } | null = null;
  let events = 0;
  const ev = (text: string) => {
    events++;
    emit(`#ev ${text} t=${Date.now() - start}`);
  };
  // Fast-scroll gears, as ScrollSpeed (software/src/state/fast_scroll.cpp), on a list
  // long enough for 3-letter jumps.
  const tuning: Record<string, number> = { s1: 16, s2: 28, s3: 40, win: 12, dwell: 520 };
  const MAX_LETTERS = 3;
  const speed = { hist: [] as { t: number; n: number }[], dir: 0, gear: 0, last: 0 };
  const gearLetters = () => (speed.gear ? MAX_LETTERS - speed.gear + 1 : 0);
  const scrollSpeed = (clicks: number, at: number) => {
    const d = clicks < 0 ? -1 : 1;
    const since = at - speed.last;
    if (!speed.hist.length || d !== speed.dir || since >= tuning.dwell) speed.hist = [];
    speed.dir = d;
    if (since > 0) speed.last = at;
    speed.hist = [...speed.hist, { t: at, n: Math.min(255, Math.abs(clicks)) }].slice(-25);
    // Average over the last `win` clicks.
    const win = Math.max(1, Math.min(24, tuning.win));
    let sum = 0, v = 0, full = false;
    for (let i = speed.hist.length - 1; i >= 0; i--) {
      if (i < speed.hist.length - 1) {
        v = Math.floor((sum * 1000) / Math.max(1, at - speed.hist[i].t));
        if (sum >= win) {
          full = true;
          break;
        }
      }
      sum += speed.hist[i].n;
    }
    v = Math.min(0xffff, v);
    const th = [tuning.s1, tuning.s2, tuning.s3];
    if (full && speed.gear < MAX_LETTERS && v >= th[speed.gear]) {
      speed.gear++;
      speed.hist = [{ t: at, n: 0 }];
    }
    emit(`#ev fast l=${gearLetters()} v=${v} t=${at - start}`);
  };
  // Held still: one gear down. Lifted: rows.
  const gearTick = (now: number) => {
    if (!speed.gear || now - speed.last < tuning.dwell) return;
    speed.gear--;
    speed.last = now;
    speed.hist = [];
    emit(`#ev fast l=${gearLetters()} v=0 t=${now - start}`);
  };
  const gearLift = () => {
    if (speed.gear) emit(`#ev fast l=0 v=0 t=${Date.now() - start}`);
    speed.hist = [];
    speed.gear = 0;
  };
  const onScroll = (v: number, at = Date.now()) => {
    ev(`scroll v=${v}`);
    scrollSpeed(v, at);
    const now = Date.now();
    if (now - fastWindow.start > 300) fastWindow = { start: now, clicks: 0 };
    fastWindow.clicks += Math.abs(v);
    if (Math.abs(v) > 1 || fastWindow.clicks >= 6) fastUntil = now + 600;
    sel = Math.max(0, sel - (Math.abs(v) > 1 ? Math.sign(v) * 7 : v));
  };
  const onClick = (b: string) => {
    if (b === "play") playing = !playing;
    if (b === "next" || b === "prev") pos = 0;
  };
  const wheelTick = () => {
    const now = Date.now();
    if (gesture) {
      const t = now - gesture.start;
      const k = gesture.keys;
      let i = 0;
      while (i + 1 < k.length && k[i + 1].at <= t) i++;
      let a = k[i].angle;
      if (a !== null && i + 1 < k.length && k[i + 1].angle !== null) {
        a += ((k[i + 1].angle! - a) * (t - k[i].at)) / (k[i + 1].at - k[i].at);
      }
      finger.angle = a;
      finger.buttons = k[i].buttons;
      if (t >= k[k.length - 1].at) {
        const g = gesture;
        gesture = null;
        const settled = 5000 + Math.random() * 15000;
        setTimeout(() => emit(`#ok ${g.cmd} us=${Math.round(settled)} events=${events - g.events}`), settled / 1000 + 20);
      }
    }
    // Finger: touch edges, clicks from the turned angle, the lift-off hold.
    const a = finger.angle === null ? null : Math.round(((finger.angle % 360) + 360) % 360);
    if (a !== null && wheel.last === null) {
      wheel.acc = 0;
      ev("touch");
    } else if (a === null && wheel.last !== null) {
      wheel.held = [];
      ev("untouch");
      gearLift();
    }
    if (a !== null && wheel.last !== null) {
      let d = a - wheel.last;
      if (d > 180) d -= 360;
      else if (d < -180) d += 360;
      wheel.acc += d;
      const clicks = Math.trunc(wheel.acc / 15);
      if (clicks) {
        wheel.acc -= clicks * 15;
        wheel.held.push({ at: now, clicks });
      }
    }
    wheel.last = a;
    const due = wheel.held.filter((h) => now - h.at >= 80);
    if (due.length) {
      wheel.held = wheel.held.slice(due.length);
      const v = due.reduce((s, h) => s + h.clicks, 0);
      if (v) onScroll(v, due[due.length - 1].at);
    }
    gearTick(now);
    // Buttons: debounce, click on release, long and very long while held.
    BUTTONS.forEach((name, i) => {
      const down = !!(finger.buttons & (1 << i));
      const r = wheel.raw[i];
      const b = wheel.btn[i];
      if (down !== r.down) Object.assign(r, { down, since: now });
      if (r.down !== b.pressed && now - r.since >= 20) {
        b.pressed = r.down;
        if (b.pressed) {
          Object.assign(b, { start: now, long: false, vlong: false });
          ev(`down b=${name}`);
        } else {
          ev(`up b=${name}`);
          if (!b.long) {
            ev(`click b=${name}`);
            onClick(name);
          }
        }
      }
      if (b.pressed && !b.long && now - b.start >= 800) {
        b.long = true;
        ev(`long b=${name}`);
      }
      if (b.pressed && !b.vlong && now - b.start >= 2000) {
        b.vlong = true;
        ev(`verylong b=${name}`);
      }
    });
    const echo = a ?? -1;
    if (echo !== wheel.echoed && now - wheel.echoAt >= 33) {
      wheel.echoed = echo;
      wheel.echoAt = now;
      emit(`#ev finger a=${echo} t=${now - start}`);
    }
  };
  const wheelTimer = setInterval(wheelTick, 5);
  const mask = (names: string) =>
    names.split("+").reduce((m, n) => (BUTTONS.includes(n) ? m | (1 << BUTTONS.indexOf(n)) : NaN), 0);
  const play = (cmd: string, keys: Key[]) => {
    if (gesture) return emit(`#err ${cmd} busy with ${gesture.cmd}`);
    if (!keys.length || keys.some((k) => Number.isNaN(k.buttons))) return emit(`#err ${cmd} bad arguments`);
    gesture = { cmd, keys, start: Date.now(), events };
  };

  const timer = setInterval(() => {
    const t = (Date.now() - start) / 1000;
    const busy = Date.now() < busyUntil;
    if (playing) pos++;
    runway = Math.max(0, Math.min(4, runway + (busy ? -0.6 : 0.35) + (Math.random() - 0.5) * 0.3));
    const underrun = runway < 0.3 ? 1 + Math.floor(Math.random() * 3) : 0;
    const isr = playing ? 150 + Math.random() * 40 : 2;
    const sd = playing ? 40 + Math.random() * 30 + (busy ? 60 : 0) : 1;
    const ui = busy ? 120 + Math.random() * 120 : 3 + Math.random() * 4;
    // Top-level loop steps. loop() never sleeps, so they fill the second (the
    // loop just spins faster when idle); the feeder interrupt runs inside them.
    const raw = {
      ui,
      audio: sd + 60 + Math.random() * 20,
      wheel: 40 + Math.random() * 10,
      input: 8 + Math.random() * 4,
      bench: 2 + Math.random(),
      bt: 5 + Math.random() * 2,
      misc: 3 + Math.random(),
    };
    const k = 975 / Object.values(raw).reduce((a, b) => a + b, 0);
    const step = Object.fromEntries(Object.entries(raw).map(([n, v]) => [n, v * k])) as typeof raw;
    const { audio, wheel, input, bench, bt, misc } = step;
    const loop = 990;
    const us = (ms: number) => Math.round(1000 * ms);
    emit(
      `[prof 1000ms] loops=${Math.round(5000 - ui * 10)} maxLoop=${us(0.4 + (busy ? 20 + Math.random() * 25 : Math.random() * 3))}us` +
        ` | sd ${us(sd)}us max900us 120KB | audio ${us(audio)}us max2000us | ui ${us(step.ui)}us max30000us | isr ${us(isr)}us max60us 154KB` +
        ` | wheel ${us(wheel)}us max400us | input ${us(input)}us max200us | bench ${us(bench)}us max900us` +
        ` | bt ${us(bt)}us max300us | misc ${us(misc)}us max100us | loop ${us(loop)}us max25000us`,
    );
    emit(
      `\x1b[36m[audio] \x1b[0mruntime runway=${runway.toFixed(1)}s min=${Math.max(0, runway - 0.4).toFixed(1)}s rate=${playing ? Math.round(150 + Math.sin(t) * 6) : 0}KB/s buf=512KB fifoMin=10KB underrun=${underrun} stall=0ms tick=1010us load=40ms chunk=800us`,
    );
  }, 1000);

  const state = () =>
    `#ok state route=5 sel=${sel} total=20008 top=${Math.max(0, sel - 4)} playing=${playing ? 1 : 0} track=17 pos=${pos} dur=40 vol=60 repeat=0 search=0 hits=0 fast=${Date.now() < fastUntil ? 1 : 0} seek=0 screen=1 anim=0 query="" header="Tracks" now="07 flac 24-48" item="Track ${sel + 1}"`;

  const transport: Transport = {
    onLine: (cb) => void listeners.push(cb),
    close: async () => {
      clearInterval(timer);
      clearInterval(wheelTimer);
    },
    write: (text) => {
      const [cmd, ...rest] = text.trim().slice(1).split(" ");
      const arg = rest.join(" ");
      const later = (ms: number, line: string) => setTimeout(() => emit(line), ms);
      switch (cmd) {
        case "boot":
          return later(5, "#ok boot ms=4210 lookup_ms=1830 search_index_ms=610 rev=demo");
        case "dataset":
          return later(5, '#ok dataset {"generator":1,"seed":1,"tracks":20008} tracks_loaded=20008 albums_loaded=1530 artists_loaded=764 genres_loaded=41');
        case "state":
          return later(5, state());
        case "scroll": {
          const n = +arg;
          events = 0;
          onScroll(n);
          busyUntil = Date.now() + 1200;
          const us = Math.abs(n) > 1 ? 30000 + Math.random() * 25000 : 9000 + Math.random() * 9000;
          return later(us / 1000, `#ok scroll us=${Math.round(us)} events=1`);
        }
        case "click":
        case "long": {
          events = 0;
          ev(`${cmd} b=${arg}`);
          if (cmd === "click") onClick(arg);
          busyUntil = Date.now() + 1500;
          const us = arg === "select" || arg === "menu" ? 790000 + Math.random() * 60000 : 20000 + Math.random() * 20000;
          return later(us / 1000, `#ok ${cmd} us=${Math.round(us)} events=1`);
        }
        case "type":
          return later(60, `#ok type us=${40000 + arg.length * 30000} max_us=${80000 + Math.round(Math.random() * 30000)} chars=${arg.length} hits=${Math.max(0, 64 - arg.length * 9)}`);
        case "repeat":
          return later(5, `#ok repeat on=${+arg ? 1 : 0}`);
        case "clear":
          return later(5, "#ok clear");
        case "seek":
          pos = +arg;
          return later(40, `#ok seek us=${38000 + Math.round(Math.random() * 9000)}`);
        case "finger":
          finger.angle = arg === "up" ? null : +arg;
          return later(2, "#ok finger");
        case "btn": {
          const [name, down] = arg.split(" ");
          const m = mask(name);
          if (Number.isNaN(m)) return later(2, "#err btn usage: btn <button> <1|0>");
          finger.buttons = +down ? finger.buttons | m : finger.buttons & ~m;
          return later(2, "#ok btn");
        }
        case "spin": {
          const [deg, ms, from = 0, still = 100] = arg.split(" ").map(Number);
          return play("spin", [
            { at: 0, angle: from, buttons: 0 },
            { at: 30, angle: from, buttons: 0 },
            { at: 30 + ms, angle: from + deg, buttons: 0 },
            { at: 30 + ms + still, angle: from + deg, buttons: 0 },
            { at: 30 + ms + still, angle: null, buttons: 0 },
          ]);
        }
        case "hold": {
          const [names, ms] = arg.split(" ");
          return play("hold", [
            { at: 0, angle: null, buttons: mask(names) },
            { at: +ms, angle: null, buttons: 0 },
          ]);
        }
        case "gesture":
          return play(
            "gesture",
            arg.split(" ").map((tok) => {
              const [at, rest] = tok.split(":");
              const [angle, ...buttons] = rest.split("+");
              return { at: +at, angle: angle === "up" ? null : +angle, buttons: buttons.length ? mask(buttons.join("+")) : 0 };
            }),
          );
        case "scrollcfg": {
          for (const [k, v] of arg.split(" ").filter(Boolean).map((kv) => kv.split("="))) {
            if (!(k in tuning)) return later(2, `#err scrollcfg unknown ${k}`);
            tuning[k] = +v;
          }
          const t = tuning;
          return later(2, `#ok scrollcfg s1=${t.s1} s2=${t.s2} s3=${t.s3} win=${t.win} dwell=${t.dwell}`);
        }
        case "stats":
          return later(5, `#ok stats uptime_ms=${Date.now() - start} free_ram=18000`);
        default:
          return later(5, `#err ${cmd} unknown command`);
      }
    },
  };
  return new Device(transport);
}
