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
  /** short tab label */
  label: string;
  /** dev board whose headers are in board.ts, null when there is no board view */
  board: string | null;
  blurb: string;
}

/** First entry is the default package shown by the viewer. */
export const PACKAGES: PackageInfo[] = [
  {
    id: "LQFP100",
    chip: "STM32F446VET6",
    label: "LQFP100 · final board",
    board: null,
    blurb: "Pin allocation of the final Openpod board (datasheets/lqfp100-pins.md), developed on a NUCLEO-F446ZE.",
  },
  {
    id: "LQFP64",
    chip: "STM32F446RET6",
    label: "LQFP64 · Nucleo prototype",
    board: "Nucleo-F446RE",
    blurb: "How the breadboard prototype is wired on the Nucleo-F446RE.",
  },
];

export const PACKAGE_PINS: Record<PackageId, PinDef[]> = {
  LQFP64: lqfp64 as PinDef[],
  LQFP100: lqfp100 as PinDef[],
};

export function arduinoLabel(p: PinDef): string {
  return p.arduino.map((a) => a.pin).join(", ");
}

export function morphoLabel(p: PinDef): string {
  return p.morpho.map((m) => `${m.conn}-${m.pin}`).join(", ");
}
