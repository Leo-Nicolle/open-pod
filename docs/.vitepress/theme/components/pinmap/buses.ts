import raw from "../../../../public/pinout/buses.json";

export interface BusInfo {
  /** peripheral, matched against alternate-function names (see studio.ts famRe) */
  family: string;
  bus: string;
  busClock: string;
  maxRate: string;
  detail: string;
  source: string;
}

/** Peripheral bus limits from the STM32F446 datasheet. Same chip in every
 *  package, so one table serves both. */
export const BUSES: BusInfo[] = raw as BusInfo[];

const byFamily = new Map(BUSES.map((b) => [b.family, b]));

export function busFor(family: string): BusInfo | undefined {
  return byFamily.get(family);
}
