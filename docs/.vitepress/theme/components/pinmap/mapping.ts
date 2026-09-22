import raw from "./mapping.json";

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

/** Firmware pin mapping — extracted from software/src/pinout.h and
 *  software/src/rendering/ILI9341_driver.h (plus main.cpp Wire3 / SPI defaults). */
export const ASSIGNMENTS: Assignment[] = raw as Assignment[];

export const DEFAULT_MAPPING_JSON = JSON.stringify(raw, null, 2);

/** Parse a user-edited mapping JSON string. Returns [] on invalid input. */
export function parseMapping(json: string): Assignment[] {
  const s = (json ?? "").trim();
  if (!s) return [];
  try {
    const data = JSON.parse(s);
    if (!Array.isArray(data)) return [];
    return data.filter(
      (a): a is Assignment =>
        a && typeof a === "object" && typeof a.peripheral === "string" && Array.isArray(a.pins)
    );
  } catch {
    return [];
  }
}

function pad(s: string, n: number): string {
  return s.length >= n ? s : s + " ".repeat(n - s.length);
}

/** Export the mapping as C++ #defines, mirroring the pinout.h / driver style. */
export function exportCode(assignments: Assignment[]): string {
  const lines: string[] = [];
  lines.push("// ─────────────────────────────────────────────");
  lines.push("// STM32F446RE pin mapping (generated)");
  lines.push("// ─────────────────────────────────────────────");
  lines.push("");
  for (const a of assignments) {
    lines.push(`// ${a.peripheral}${a.bus ? ` — ${a.bus}` : ""}`);
    for (const p of a.pins) {
      if (p.macro) {
        lines.push(`#define ${pad(p.macro, 12)} ${pad(p.pin, 4)}  // ${p.role}`);
      } else {
        lines.push(`// ${pad(p.pin, 5)} : ${p.role}`);
      }
    }
    lines.push("");
  }
  return lines.join("\n").replace(/\n+$/, "\n");
}

/** Export the mapping as a markdown table. */
export function exportMarkdown(assignments: Assignment[]): string {
  const rows = ["| Peripheral | Bus | Pin | Role |", "|---|---|---|---|"];
  for (const a of assignments) {
    for (const p of a.pins) {
      rows.push(`| ${a.peripheral} | ${a.bus ?? "—"} | \`${p.pin}\` | ${p.role} |`);
    }
  }
  return rows.join("\n") + "\n";
}
