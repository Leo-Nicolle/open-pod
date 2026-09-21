import raw from "./pins.json";

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
}

export const PINS: PinDef[] = raw as PinDef[];

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
