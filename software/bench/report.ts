// Bench reports: one JSON per firmware revision in bench/results/, and the
// copies + index the docs page reads (docs/public/bench/).
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

import { METRICS, revFile, summarize, type IndexEntry, type Metric, type Report } from "./metrics.ts";

export * from "./metrics.ts";

export function writeReport(resultsDir: string, r: Report, log: string[]): string {
  mkdirSync(resultsDir, { recursive: true });
  const base = join(resultsDir, revFile(r.rev));
  writeFileSync(`${base}.json`, JSON.stringify(r, null, 2) + "\n");
  writeFileSync(`${base}.log`, log.join("\n") + "\n");
  return `${base}.json`;
}

/** Copies the report to docs/public/bench/ and updates its index.json (one entry per rev, oldest first). */
export function publish(docsDir: string, r: Report) {
  mkdirSync(docsDir, { recursive: true });
  const file = `${revFile(r.rev)}.json`;
  writeFileSync(join(docsDir, file), JSON.stringify(r, null, 2) + "\n");
  const indexPath = join(docsDir, "index.json");
  const index: { metrics: Metric[]; runs: IndexEntry[] } = existsSync(indexPath)
    ? JSON.parse(readFileSync(indexPath, "utf8"))
    : { metrics: METRICS, runs: [] };
  index.metrics = METRICS;
  index.runs = index.runs.filter((e) => e.rev !== r.rev);
  index.runs.push({ rev: r.rev, date: r.date, file, dataset: r.dataset, summary: summarize(r) });
  index.runs.sort((a, b) => a.date.localeCompare(b.date));
  writeFileSync(indexPath, JSON.stringify(index, null, 2) + "\n");
}
