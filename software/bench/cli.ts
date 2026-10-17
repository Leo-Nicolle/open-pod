#!/usr/bin/env node
// Firmware test bench, see docs/tools/bench.md.
//
//   npm run bench -- prepare <card mount> [--tracks 20000] [--seed 1] [--covers] [--force]
//   npm run bench -- run [--port /dev/ttyACM0] [--no-flash] [--only scroll,search] [--no-background] [--no-publish]
//   npm run bench -- e2e [filter] [--port /dev/ttyACM0] [--flash]
//   npm run bench -- compare <revA> <revB>
//   npm run bench -- import <report.json>      (one downloaded from the docs' live page)
import { execFileSync, spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { hostname } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { parseArgs } from "node:util";

import { runBench, SCENARIOS } from "./bench.ts";
import { runE2E, selectTests, sequence } from "./e2e/index.ts";
import { openNodeDevice } from "./node-serial.ts";
import { compareText, publish, revFile, writeReport, type Report } from "./report.ts";
import { buildSamples } from "./samples.ts";

const BENCH = dirname(fileURLToPath(import.meta.url));
const SOFTWARE = resolve(BENCH, "..");
const REPO = resolve(SOFTWARE, "..");
const RESULTS = join(BENCH, "results");
const DOCS_BENCH = join(REPO, "docs/public/bench");
const PIO = join(REPO, "scripts/pio.sh");
const opentone = join(REPO, "syncer/target/release/opentone");

function buildOpenpod() {
  console.log("Building the opentone CLI (release)...");
  execFileSync("cargo", ["build", "--release", "-p", "opentone-cli"], { cwd: join(REPO, "syncer"), stdio: "inherit" });
}

function gitRev(): string {
  return execFileSync("git", ["describe", "--always", "--dirty", "--tags"], { cwd: REPO, encoding: "utf8" }).trim();
}

async function prepare(argv: string[]) {
  const { values, positionals } = parseArgs({
    args: argv,
    allowPositionals: true,
    options: {
      tracks: { type: "string", default: "20000" },
      seed: { type: "string", default: "1" },
      covers: { type: "boolean", default: false },
      force: { type: "boolean", default: false },
    },
  });
  const card = positionals[0];
  if (!card) throw new Error("usage: bench prepare <card mount> [--tracks N] [--seed S] [--covers] [--force]");
  buildOpenpod();
  console.log("Samples:");
  const samples = buildSamples(join(BENCH, ".cache/samples"), opentone);
  const args = ["bench-dataset", card, "--samples", samples, "--tracks", values.tracks!, "--seed", values.seed!];
  if (values.covers) args.push("--covers");
  if (values.force) args.push("--force");
  execFileSync(opentone, args, { stdio: "inherit" });
  spawnSync("sync");
  console.log(`\nCard ready. Unmount it, put it in the Nucleo, then: npm run bench -- run`);
}

async function run(argv: string[]) {
  const { values } = parseArgs({
    args: argv,
    options: {
      port: { type: "string", default: process.env.OPENPOD_PORT ?? "/dev/ttyACM0" },
      "no-flash": { type: "boolean", default: false },
      "no-publish": { type: "boolean", default: false },
      "no-background": { type: "boolean", default: false },
      only: { type: "string" },
    },
  });
  const names = values.only ? values.only.split(",") : Object.keys(SCENARIOS);
  for (const n of names) if (!SCENARIOS[n]) throw new Error(`unknown scenario ${n} (${Object.keys(SCENARIOS).join(", ")})`);

  // The port opens first: flashing goes through the ST-Link's SWD side and
  // doesn't touch the serial link, so the boot lines aren't missed.
  const d = await openNodeDevice(values.port!);
  try {
    const booted = d.waitForBoot(90000);
    if (values["no-flash"]) {
      console.log("Press the Nucleo's reset button...");
    } else {
      console.log("Flashing the bench firmware...");
      execFileSync(PIO, ["run", "-e", "bench", "-t", "upload"], { cwd: SOFTWARE, stdio: ["ignore", "ignore", "inherit"] });
    }
    const boot = await booted;
    console.log(`Booted ${boot.rev} in ${boot.ms} ms`);
    if (boot.rev !== gitRev()) console.warn(`warning: firmware ${boot.rev}, working tree ${gitRev()}`);

    const report = await runBench(d, {
      scenarios: names,
      boot,
      host: hostname(),
      background: !values["no-background"],
      onProgress: (p) => {
        if (p.kind === "start") process.stdout.write(`- ${p.scenario}... `);
        else if (p.kind === "done") console.log(`done in ${p.seconds.toFixed(0)} s`);
        else console.log(`FAILED: ${p.message}`);
      },
    });
    console.log(`Dataset: ${JSON.stringify(report.dataset)}`);

    const file = writeReport(RESULTS, report, d.log);
    console.log(`\nReport: ${file}`);
    if (!values["no-publish"]) {
      publish(DOCS_BENCH, report);
      console.log(`Published to ${DOCS_BENCH}`);
    }
    if (report.errors.length) process.exitCode = 1;
  } finally {
    await d.close();
    restoreTty(values.port!);
  }
}

/** Runs the e2e tests (bench/e2e/) on the board as it is, or flashed first. */
async function e2e(argv: string[]) {
  const { values, positionals } = parseArgs({
    args: argv,
    allowPositionals: true,
    options: {
      port: { type: "string", default: process.env.OPENPOD_PORT ?? "/dev/ttyACM0" },
      flash: { type: "boolean", default: false },
    },
  });
  const tests = selectTests(positionals[0]);
  if (!tests.length) throw new Error(`no test matches "${positionals[0]}"`);
  const d = await openNodeDevice(values.port!);
  try {
    if (values.flash) {
      const booted = d.waitForBoot(90000);
      console.log("Flashing the bench firmware...");
      execFileSync(PIO, ["run", "-e", "bench", "-t", "upload"], { cwd: SOFTWARE, stdio: ["ignore", "ignore", "inherit"] });
      await booted;
    }
    const boot = await d.send("boot", "", 5000);
    console.log(`Firmware ${boot.rev}, ${tests.length} test(s)\n`);
    const results = await runE2E(d, tests, {
      onProgress: (p) => {
        if (p.kind === "start") process.stdout.write(`- ${p.name}... `);
        else if (p.result.ok) console.log(`ok (${(p.result.ms / 1000).toFixed(1)} s)`);
        else {
          console.log("FAILED");
          if (p.result.step) console.log(`    while: ${p.result.step}`);
          console.log(`    ${p.result.error}`);
          console.log(`    events: ${sequence(p.result.events.slice(-20)) || "(none)"}`);
        }
      },
    });
    const failed = results.filter((r) => !r.ok).length;
    console.log(`\n${results.length - failed} passed, ${failed} failed`);
    if (failed) process.exitCode = 1;
  } finally {
    await d.close();
    restoreTty(values.port!);
  }
}

/** Leaves the tty with VMIN=1: Chrome < 156 on Linux keeps the last program's
 * VMIN, and with 0 the docs' live page sees the port as lost. */
function restoreTty(port: string) {
  if (process.platform === "linux") spawnSync("stty", ["-F", port, "min", "1", "time", "0"]);
}

function loadReport(rev: string): Report {
  for (const p of [rev, join(RESULTS, `${revFile(rev)}.json`), join(DOCS_BENCH, `${revFile(rev)}.json`)]) {
    if (existsSync(p) && p.endsWith(".json")) return JSON.parse(readFileSync(p, "utf8"));
  }
  throw new Error(`no report for ${rev} in ${RESULTS} or ${DOCS_BENCH}`);
}

function compare(argv: string[]) {
  const { values, positionals } = parseArgs({
    args: argv,
    allowPositionals: true,
    options: { threshold: { type: "string", default: "0.1" } },
  });
  if (positionals.length !== 2) throw new Error("usage: bench compare <revA> <revB> [--threshold 0.1]");
  const { text, regressions } = compareText(loadReport(positionals[0]), loadReport(positionals[1]), +values.threshold!);
  console.log(text);
  if (regressions) {
    console.log(`\n${regressions} regression(s)`);
    process.exitCode = 1;
  }
}

/** Saves and publishes a report downloaded from the docs' live page. */
function importReport(argv: string[]) {
  if (argv.length !== 1) throw new Error("usage: bench import <report.json>");
  const report: Report = JSON.parse(readFileSync(argv[0], "utf8"));
  if (!report.rev || !report.scenarios) throw new Error(`${argv[0]} isn't a bench report`);
  const log: string[] = (report as any).log ?? [];
  delete (report as any).log;
  console.log(`Report: ${writeReport(RESULTS, report, log)}`);
  publish(DOCS_BENCH, report);
  console.log(`Published to ${DOCS_BENCH}`);
}

const [cmd, ...rest] = process.argv.slice(2);
const commands: Record<string, (a: string[]) => unknown> = { prepare, run, e2e, compare, import: importReport };
if (!commands[cmd]) {
  console.log("usage: bench <prepare|run|e2e|compare|import> ...  (see docs/tools/bench.md)");
  process.exit(cmd ? 1 : 0);
}
Promise.resolve()
  .then(() => commands[cmd](rest))
  .catch((e) => {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  });
