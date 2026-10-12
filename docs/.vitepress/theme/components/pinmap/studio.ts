/**
 * Pure data + logic for the pinout viewer. Kept framework-free so
 * usePinout.ts only has to wire reactive state to these functions.
 */
import { PACKAGE_PINS, arduinoLabel, morphoLabel, type PackageId, type PinDef } from "./pins";
import { PACKAGE_ASSIGNMENTS } from "./mapping";

export type Cat = "power" | "ref" | "reset" | "boot" | "debug" | "io" | "nc";

export interface StudioPin {
  num: number;
  name: string; // base MCU name, e.g. "PA5"
  fullName: string; // e.g. "PC14-OSC32_IN", falls back to name
  type: string; // io code: FT / TTa / FTf / S / RST / B
  dflt: string; // default function (pins.ts `signal`)
  af: string[];
  add: string[];
  ard: string;
  morpho: string;
  note: string;
  maxSpeed: string;
  domain: string;
  remarks: string[];
  cat: Cat;
  blob: string; // default + alternates + additional, for matching
}

export interface MappingRow {
  p: string;
  bus: string;
  pin: string;
  role: string;
  macro: string;
}

export interface Issue {
  base: string;
  sev: "error" | "warn" | "info";
  msg: string;
}

/* --------------------------------------------------------------- tokens */

export const CAT: Record<Cat | "used", string> = {
  power: "oklch(0.72 0.13 25)",
  ref: "oklch(0.72 0.13 95)",
  reset: "oklch(0.72 0.13 60)",
  boot: "oklch(0.72 0.13 310)",
  debug: "oklch(0.72 0.13 250)",
  io: "oklch(0.72 0.13 155)",
  nc: "oklch(0.50 0.010 255)",
  used: "oklch(0.80 0.15 195)",
};
export const PHUES = [205, 25, 155, 285, 75, 330, 120, 250];
/** the viewer background, light or dark (PinoutViewer.vue) */
export const BG = "var(--pv-bg)";

export function mix(color: string, pct: number): string {
  return `color-mix(in oklab, ${color} ${pct}%, ${BG})`;
}

export function periphColor(name: string, periphNames: string[]): string {
  const i = periphNames.indexOf(name);
  return `oklch(0.72 0.14 ${PHUES[(i < 0 ? 0 : i) % PHUES.length]})`;
}

/* ------------------------------------------------------------------ pins */

function catFor(type: PinDef["type"], signal: string): Cat {
  if (type === "Power") return "power";
  if (type === "Reset") return "reset";
  if (type === "Boot") return "boot";
  if (/SWD|JTDI|JTDO|NJTRST|JTMS|JTCK/.test(signal)) return "debug";
  if (/BOOT/.test(signal)) return "boot";
  return "io";
}

export function buildPins(pkg: PackageId): StudioPin[] {
  return [...PACKAGE_PINS[pkg]]
    .sort((a, b) => a.num - b.num)
    .map((p) => ({
      num: p.num,
      name: p.name,
      fullName: p.fullName || p.name,
      type: p.io,
      dflt: p.signal,
      af: p.alt,
      add: p.extra,
      ard: arduinoLabel(p),
      morpho: morphoLabel(p),
      note: p.note || "",
      maxSpeed: p.maxSpeed || "",
      domain: p.domain || "",
      remarks: p.remarks || [],
      cat: catFor(p.type, p.signal),
      blob: [p.signal, p.alt.join(", "), p.extra.join(", ")].join(" "),
    }));
}

export function indexByBase(pins: StudioPin[]): Map<string, StudioPin> {
  const m = new Map<string, StudioPin>();
  for (const p of pins) if (!m.has(p.name)) m.set(p.name, p);
  return m;
}

/* --------------------------------------------------------------- rows */

/** Firmware default mapping, flattened from mapping.json's grouped shape. */
export function firmwareRows(pkg: PackageId): MappingRow[] {
  const out: MappingRow[] = [];
  for (const a of PACKAGE_ASSIGNMENTS[pkg]) {
    for (const p of a.pins) {
      out.push({ p: a.peripheral, bus: a.bus || "", pin: p.pin, role: p.role, macro: p.macro || "" });
    }
  }
  return out;
}

export function usedIndex(rows: MappingRow[]): Map<string, (MappingRow & { i: number })[]> {
  const m = new Map<string, (MappingRow & { i: number })[]>();
  rows.forEach((r, i) => {
    const arr = m.get(r.pin) ?? [];
    arr.push({ ...r, i });
    m.set(r.pin, arr);
  });
  return m;
}

export function peripheralList(rows: MappingRow[]): string[] {
  const out: string[] = [];
  for (const r of rows) if (r.p && !out.includes(r.p)) out.push(r.p);
  return out;
}

/* --------------------------------------------------------- function families */

export const FAMS = [
  "SPI1", "SPI2", "SPI3", "I2S1", "I2S2", "I2S3", "I2C1", "I2C2", "I2C3", "FMPI2C1",
  "USART1", "USART2", "USART3", "USART6", "UART4", "UART5", "CAN1", "CAN2",
  "TIM1", "TIM2", "TIM3", "TIM4", "TIM5", "TIM8", "TIM9", "TIM10", "TIM11", "TIM12", "TIM13", "TIM14",
  "ADC", "DAC", "SAI1", "SAI2", "SPDIFRX", "QUADSPI", "SDIO", "DCMI", "FMC", "USB", "RTC", "HDMI_CEC", "MCO", "SWD", "TRACE",
];

const reCache: Record<string, RegExp> = {};
export function famRe(f: string): RegExp {
  if (reCache[f]) return reCache[f];
  const re =
    f === "USB"
      ? /OTG_/
      : f === "SWD"
        ? /SWD|JTMS|JTCK|JTDI|JTDO|NJTRST/
        : new RegExp(f + "(?![0-9])");
  reCache[f] = re;
  return re;
}

export function signalsIn(text: string): string[] {
  return text.match(/[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+/g) ?? [];
}

/* --------------------------------------------------------------- checks */

export function issueList(rows: MappingRow[], byBase: Map<string, StudioPin>): Issue[] {
  const out: Issue[] = [];
  const byPin = new Map<string, MappingRow[]>();
  for (const r of rows) {
    const arr = byPin.get(r.pin) ?? [];
    arr.push(r);
    byPin.set(r.pin, arr);
  }
  for (const [base, rs] of byPin) {
    const p = byBase.get(base);
    if (!p) {
      out.push({ base, sev: "error", msg: "Unknown pin name — not on this package." });
      continue;
    }
    const roles = rs.map((r) => r.role.replace(/\s*\(.*\)/, "").trim());
    const uniq = roles.filter((v, i) => roles.indexOf(v) === i);
    if (rs.length > 1 && uniq.length > 1) {
      out.push({
        base,
        sev: "error",
        msg: `Driven by ${rs.map((r) => r.p).join(" + ")} with different roles (${uniq.join(" / ")}).`,
      });
    } else if (rs.length > 1) {
      out.push({
        base,
        sev: "info",
        msg: `Shared bus line: ${rs.map((r) => r.p).join(", ")} — fine if they share ${uniq[0]}.`,
      });
    }
    if (p.cat === "debug" || /JTDO|NJTRST|JTDI/.test(p.dflt)) {
      out.push({ base, sev: "warn", msg: `${p.dflt} by default — reusing it costs you JTAG/SWO.` });
    }
    if (p.cat === "power" || p.cat === "reset" || p.cat === "boot") {
      out.push({ base, sev: "error", msg: `This is a ${p.cat} pin, not general purpose I/O.` });
    }
    for (const r of rs) {
      for (const sig of signalsIn(r.role)) {
        const fam = FAMS.find((f) => famRe(f).test(sig));
        if (!fam) continue;
        if (p.blob.indexOf(sig) < 0) {
          out.push({ base, sev: "warn", msg: `${sig} is not an alternate function of ${base}.` });
        }
      }
    }
  }
  return out;
}

export function issuesByBase(issues: Issue[]): Map<string, Issue[]> {
  const m = new Map<string, Issue[]>();
  for (const s of issues) {
    const arr = m.get(s.base) ?? [];
    arr.push(s);
    m.set(s.base, arr);
  }
  return m;
}

export const SEV_COLOR: Record<Issue["sev"], string> = {
  error: "var(--pv-error)",
  warn: "var(--pv-warn)",
  info: "var(--pv-info)",
};

/* ------------------------------------------------------------ match set */

export interface MatchFilters {
  fn: string;
  hoverFn: string | null;
  periph: string[];
  hoverPeriph: string | null;
  legend: string | null;
  q: string;
}

export function matchSet(
  pins: StudioPin[],
  used: Map<string, (MappingRow & { i: number })[]>,
  f: MatchFilters
): Set<string> | null {
  const fn = f.hoverFn || f.fn;
  const periphs = f.hoverPeriph ? [f.hoverPeriph] : f.periph;
  const q = f.q.trim().toLowerCase();
  const legend = f.legend;
  if (!fn && !periphs.length && !q && !legend) return null;
  const set = new Set<string>();
  for (const p of pins) {
    let ok = true;
    const rows = used.get(p.name) ?? [];
    if (fn) ok = ok && famRe(fn).test(p.blob);
    if (periphs.length) ok = ok && rows.some((r) => periphs.includes(r.p));
    if (legend) ok = ok && (legend === "used" ? rows.length > 0 : p.cat === legend);
    if (q) {
      const hay = (
        p.name +
        " " +
        p.ard +
        " " +
        p.morpho +
        " " +
        p.blob +
        " " +
        rows.map((r) => `${r.p} ${r.role} ${r.macro}`).join(" ")
      ).toLowerCase();
      ok = ok && hay.includes(q);
    }
    if (ok) set.add(p.name);
  }
  return set;
}
