export interface Usage {
  /** Raw pin reference as supplied by the user ("PA5", "21", "PC13", ...). */
  pin: string;
  /** Human description of what the pin is used for. */
  label: string;
}

const PIN_KEYS = ["pin", "name", "num", "number", "position", "pos", "port", "chip", "io"];
const LABEL_KEYS = [
  "label",
  "function",
  "func",
  "use",
  "usage",
  "description",
  "desc",
  "purpose",
  "signal",
  "role",
  "assignment",
  "what",
];

function normalize(s: string): string {
  return s.trim().toLowerCase().replace(/\s+/g, "");
}

/** Find the column index whose header matches one of `keys`. */
function matchCol(headers: string[], keys: string[]): number {
  for (const key of keys) {
    const idx = headers.findIndex((h) => normalize(h) === key);
    if (idx >= 0) return idx;
  }
  for (const key of keys) {
    const idx = headers.findIndex((h) => normalize(h).includes(key));
    if (idx >= 0) return idx;
  }
  return -1;
}

function splitCsv(line: string): string[] {
  const out: string[] = [];
  let cur = "";
  let inQ = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (inQ) {
      if (c === '"') {
        if (line[i + 1] === '"') {
          cur += '"';
          i++;
        } else {
          inQ = false;
        }
      } else {
        cur += c;
      }
    } else if (c === '"') {
      inQ = true;
    } else if (c === "," || c === ";" || c === "\t") {
      out.push(cur);
      cur = "";
    } else {
      cur += c;
    }
  }
  out.push(cur);
  return out.map((s) => s.trim());
}

function splitMdRow(line: string): string[] {
  return line
    .split("|")
    .map((s) => s.trim())
    .filter((s, i, arr) => !(i === 0 && s === "") && !(i === arr.length - 1 && s === ""));
}

function isMdSeparator(line: string): boolean {
  return /^\s*\|?[\s:|-]+\|?\s*$/.test(line) && /-/.test(line);
}

function parseJson(input: string): Usage[] | null {
  let data: unknown;
  try {
    data = JSON.parse(input);
  } catch {
    return null;
  }
  if (!data) return [];
  // Array of objects
  if (Array.isArray(data)) {
    const out: Usage[] = [];
    for (const item of data) {
      if (item == null) continue;
      if (typeof item === "string") {
        out.push({ pin: item, label: "" });
        continue;
      }
      if (typeof item === "object") {
        const rec = item as Record<string, unknown>;
        const pinKey = Object.keys(rec).find((k) => PIN_KEYS.includes(normalize(k))) ?? "pin";
        const labelKey =
          Object.keys(rec).find((k) => LABEL_KEYS.includes(normalize(k))) ?? "label";
        const pin = rec[pinKey];
        const label = rec[labelKey];
        if (pin == null && label == null) continue;
        out.push({
          pin: pin != null ? String(pin) : "",
          label: label != null ? String(label) : "",
        });
      }
    }
    return out;
  }
  // Object map: { "PA5": "SPI1_SCK", ... }
  if (typeof data === "object") {
    const out: Usage[] = [];
    for (const [k, v] of Object.entries(data as Record<string, unknown>)) {
      out.push({ pin: k, label: v == null ? "" : String(v) });
    }
    return out;
  }
  return null;
}

function parseTable(input: string): Usage[] | null {
  const lines = input.split(/\r?\n/).filter((l) => l.includes("|"));
  if (lines.length === 0) return null;
  const rows = lines.map(splitMdRow).filter((r) => r.length > 0 && !isMdSeparator(r.join("|")));
  if (rows.length === 0) return null;
  const first = rows[0];
  const pinIdx = matchCol(first, PIN_KEYS);
  const labelIdx = matchCol(first, LABEL_KEYS);
  const hasHeader = pinIdx >= 0 || labelIdx >= 0;
  const dataRows = hasHeader ? rows.slice(1) : rows;
  const out: Usage[] = [];
  for (const row of dataRows) {
    if (row.every((c) => !c)) continue;
    const pin = hasHeader && pinIdx >= 0 ? row[pinIdx] : row[0];
    const label = hasHeader && labelIdx >= 0 ? row[labelIdx] : (row[1] ?? "");
    if (!pin && !label) continue;
    out.push({ pin: pin ?? "", label: label ?? "" });
  }
  return out;
}

function parseCsv(input: string): Usage[] {
  const lines = input.split(/\r?\n/).filter((l) => l.trim().length > 0);
  if (lines.length === 0) return [];
  const rows = lines.map(splitCsv);
  const first = rows[0];
  const pinIdx = matchCol(first, PIN_KEYS);
  const labelIdx = matchCol(first, LABEL_KEYS);
  const hasHeader = pinIdx >= 0 || labelIdx >= 0;
  const dataRows = hasHeader ? rows.slice(1) : rows;
  const out: Usage[] = [];
  for (const row of dataRows) {
    if (row.every((c) => !c)) continue;
    const pin = hasHeader && pinIdx >= 0 ? row[pinIdx] : row[0];
    const label = hasHeader && labelIdx >= 0 ? row[labelIdx] : (row[1] ?? "");
    if (!pin && !label) continue;
    out.push({ pin: pin ?? "", label: label ?? "" });
  }
  return out;
}

/**
 * Parse a "which pins are used" spec into a list of { pin, label }.
 * Accepted formats (auto-detected):
 *  - JSON array   : [{"pin":"PA5","label":"SPI1_SCK"}, ...]
 *  - JSON object  : {"PA5":"SPI1_SCK", ...}
 *  - CSV          : pin,function\nPA5,SPI1_SCK\n...
 *  - Markdown table: | Pin | Function |\n|---|---|\n| PA5 | SPI1_SCK |
 */
export function parseUsage(input: string): Usage[] {
  const s = (input ?? "").trim();
  if (!s) return [];
  if (s.startsWith("[") || s.startsWith("{")) {
    const j = parseJson(s);
    if (j) return j;
  }
  const t = parseTable(s);
  if (t) return t;
  return parseCsv(s);
}
