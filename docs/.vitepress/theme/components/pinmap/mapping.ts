import lqfp64 from "../../../../public/pinout/LQFP64/mapping.json";
import lqfp100 from "../../../../public/pinout/LQFP100/mapping.json";
import type { PackageId } from "./pins";

export interface AssignmentPin {
  pin: string;
  role: string;
  macro?: string;
  note?: string;
}

export interface Assignment {
  peripheral: string;
  bus?: string;
  pins: AssignmentPin[];
}

/** Default pin mapping per package. LQFP64 is the firmware's — extracted from
 *  software/src/pinout.h and software/src/rendering/ILI9341_driver.h (plus
 *  main.cpp Wire3 / SPI defaults). LQFP100 is datasheets/lqfp100-pins.md. */
export const PACKAGE_ASSIGNMENTS: Record<PackageId, Assignment[]> = {
  LQFP64: lqfp64 as Assignment[],
  LQFP100: lqfp100 as Assignment[],
};
