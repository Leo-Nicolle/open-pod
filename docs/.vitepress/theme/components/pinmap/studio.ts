/**
 * Pure data + logic for the Nucleo Pinout Studio, ported from the design
 * handoff (docs/design_handoff_nucleo_pinout_studio). Kept framework-free so
 * PinoutStudio.vue only has to wire reactive state to these functions.
 */
import { PINS, arduinoLabel, morphoLabel, type PinDef } from "./pins";
import { ASSIGNMENTS } from "./mapping";

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
export const BG = "oklch(0.19 0.012 255)";

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

export function buildPins(): StudioPin[] {
  return [...PINS]
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
export function firmwareRows(): MappingRow[] {
  const out: MappingRow[] = [];
  for (const a of ASSIGNMENTS) {
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
  "ADC", "DAC", "SAI1", "SAI2", "SPDIFRX", "QUADSPI", "SDIO", "DCMI", "FMC", "USB", "RTC", "HDMI_CEC", "MCO", "SWD",
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
  error: "oklch(0.78 0.13 25)",
  warn: "oklch(0.80 0.13 60)",
  info: "oklch(0.72 0.13 250)",
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

/* ------------------------------------------------------------------ exports */

function pad(s: string, n: number): string {
  return s.length >= n ? s : s + " ".repeat(n - s.length);
}

export function exportCpp(rows: MappingRow[]): string {
  const groups: Record<string, MappingRow[]> = {};
  const order: string[] = [];
  for (const r of rows) {
    if (!groups[r.p]) { groups[r.p] = []; order.push(r.p); }
    groups[r.p].push(r);
  }
  let out = "#pragma once\n// Generated from the Nucleo pinout studio\n";
  for (const p of order) {
    const bus = groups[p][0].bus;
    out += `\n// ${p}${bus ? ` — ${bus}` : ""}\n`;
    const names = groups[p].map(
      (r) => r.macro || (`${p}_${r.role.replace(/\s*\(.*\)/, "")}`).replace(/[^A-Za-z0-9]+/g, "_").toUpperCase()
    );
    const w = Math.max(...names.map((n) => n.length));
    groups[p].forEach((r, i) => {
      out += `#define ${pad(names[i], w)} ${r.pin}\n`;
    });
  }
  return out;
}

export function exportMdRows(rows: MappingRow[]): string {
  let out = "| Peripheral | Bus | Pin | Role |\n|---|---|---|---|\n";
  for (const r of rows) out += `| ${r.p} | ${r.bus} | \`${r.pin}\` | ${r.role} |\n`;
  return out;
}

export function exportJsonRows(rows: MappingRow[]): string {
  const groups: { peripheral: string; bus: string; pins: { pin: string; role: string; macro?: string }[] }[] = [];
  for (const r of rows) {
    let g = groups.find((x) => x.peripheral === r.p);
    if (!g) { g = { peripheral: r.p, bus: r.bus, pins: [] }; groups.push(g); }
    const e: { pin: string; role: string; macro?: string } = { pin: r.pin, role: r.role };
    if (r.macro) e.macro = r.macro;
    g.pins.push(e);
  }
  return JSON.stringify(groups, null, 2);
}

/** Parses the grouped JSON shape back into flat rows. Throws on invalid JSON. */
export function parseJsonRows(text: string): MappingRow[] {
  const data = JSON.parse(text);
  const next: MappingRow[] = [];
  for (const g of data) {
    for (const p of g.pins || []) {
      next.push({ p: g.peripheral, bus: g.bus || "", pin: p.pin, role: p.role || "", macro: p.macro || "" });
    }
  }
  return next;
}

/* ------------------------------------------------------------------ misc */

export const TAB_STYLE = (active: boolean) => ({
  padding: "6px 11px",
  borderRadius: "5px",
  border: "none",
  cursor: "pointer",
  fontSize: "12px",
  fontWeight: active ? 600 : 400,
  background: active ? "oklch(0.80 0.15 195)" : "transparent",
  color: active ? "oklch(0.19 0.03 195)" : "oklch(0.78 0.010 255)",
  fontFamily: "'Space Grotesk', sans-serif",
  whiteSpace: "nowrap" as const,
});

export const CHIP_STYLE = (active: boolean, color: string) => ({
  display: "inline-flex",
  alignItems: "center",
  gap: "6px",
  padding: "4px 9px",
  borderRadius: "20px",
  cursor: "pointer",
  fontSize: "11.5px",
  fontFamily: "'IBM Plex Mono', monospace",
  background: active ? mix(color, 26) : "oklch(0.235 0.014 255)",
  border: `1px solid ${active ? mix(color, 75) : "oklch(0.32 0.014 255)"}`,
  color: active ? "oklch(0.96 0.006 255)" : "oklch(0.80 0.010 255)",
  transition: "background .12s, border-color .12s",
});
