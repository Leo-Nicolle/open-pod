import lqfp64 from "../../../../public/pinout/LQFP64/pins.json";
import lqfp100 from "../../../../public/pinout/LQFP100/pins.json";

export type PinType = "Power" | "I/O" | "Reset" | "Boot";

export interface ArduinoRef {
  pin: string;
  func?: string;
}

export interface MorphoRef {
  conn: string;
  pin: number;
}

export interface PinDef {
  num: number;
  name: string;
  fullName?: string;
  type: PinType;
  io: string;
  signal: string;
  alt: string[];
  extra: string[];
  arduino: ArduinoRef[];
  morpho: MorphoRef[];
  note?: string;
  /** max I/O output frequency (DS10693 Table 58), I/O pins only */
  maxSpeed?: string;
  /** supply domain of the I/O, I/O pins only */
  domain?: string;
  /** datasheet remarks: limits, straps, reset state… */
  remarks?: string[];
}

export type PackageId = "LQFP64" | "LQFP100";

export interface PackageInfo {
  id: PackageId;
  chip: string;
  /** dev board whose headers are in board.ts, null when there is no board view */
  board: string | null;
  blurb: string;
}

export const PACKAGES: PackageInfo[] = [
  {
    id: "LQFP64",
    chip: "STM32F446RET6",
    board: "Nucleo-F446RE",
    blurb: "LQFP64 as wired on the MP3 project board.",
  },
  {
    id: "LQFP100",
    chip: "STM32F446VET6",
    board: null,
    blurb: "LQFP100 allocation for the final board (datasheets/lqfp100-pins.md), developed on a NUCLEO-F446ZE.",
  },
];

export const PACKAGE_PINS: Record<PackageId, PinDef[]> = {
  LQFP64: lqfp64 as PinDef[],
  LQFP100: lqfp100 as PinDef[],
};

export const PINS: PinDef[] = PACKAGE_PINS.LQFP64;

const byName = new Map<string, PinDef>();
const byNum = new Map<number, PinDef>();
for (const p of PINS) {
  byName.set(p.name.toUpperCase(), p);
  if (p.fullName) byName.set(p.fullName.toUpperCase(), p);
  byNum.set(p.num, p);
}

/** Normalize a user-provided pin reference ("PA5", "pa5", "5", "14") to a PinDef. */
export function findPin(ref: string | number): PinDef | undefined {
  if (typeof ref === "number") return byNum.get(ref);
  const s = String(ref).trim();
  if (!s) return undefined;
  const upper = s.toUpperCase();
  if (byName.has(upper)) return byName.get(upper);
  if (/^\d+$/.test(s)) return byNum.get(parseInt(s, 10));
  // strip a trailing "-WKUP"/"-BOOT1" style suffix, e.g. "PA0-WKUP"
  const base = upper.replace(/[-_].*$/, "");
  if (byName.has(base)) return byName.get(base);
  return undefined;
}

export function arduinoLabel(p: PinDef): string {
  return p.arduino.map((a) => a.pin).join(", ");
}

export function morphoLabel(p: PinDef): string {
  return p.morpho.map((m) => `${m.conn}-${m.pin}`).join(", ");
}
