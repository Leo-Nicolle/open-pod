// One sample file per audio quality the player meets, from a small mono mp3
// to 24-bit FLAC. Generated with ffmpeg (chords over pink noise, so FLAC
// can't compress them to nothing), then put through `opentone convert`, the
// same conversion sync applies, so what lands on the card is what a real
// library would carry. Cached in bench/.cache/samples/.
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, rmSync } from "node:fs";
import { join } from "node:path";

import { SAMPLES } from "./sample-list.ts";

export { BACKGROUND_SAMPLE, HEAVIEST_SAMPLE, SAMPLES, type Sample } from "./sample-list.ts";

const SECONDS = 40;

const signal = (seconds: number) => [
  "-f", "lavfi", "-i",
  "aevalsrc='0.22*sin(2*PI*220*t)*(1+0.5*sin(2*PI*0.5*t))+0.18*sin(2*PI*277.18*t)+0.18*sin(2*PI*329.63*t)" +
    "|0.22*sin(2*PI*220*t)+0.18*sin(2*PI*369.99*t)+0.14*sin(2*PI*440*t*(1+0.001*sin(2*PI*3*t)))'" +
    `:s=96000:d=${seconds}`,
  "-f", "lavfi", "-i", `anoisesrc=c=pink:a=0.08:r=96000:d=${seconds}`,
  "-filter_complex", "amix=inputs=2:normalize=0",
];

/**
 * Builds the card-ready samples into `<cacheDir>/card` (skipping the ones
 * already there) and returns that folder.
 */
export function buildSamples(cacheDir: string, opentoneBin: string): string {
  const srcDir = join(cacheDir, "source");
  const cardDir = join(cacheDir, "card");
  mkdirSync(srcDir, { recursive: true });
  mkdirSync(cardDir, { recursive: true });
  const wanted = new Set(SAMPLES.map((s) => `${s.name}.${s.cardExt}`));
  for (const f of readdirSync(cardDir)) if (!wanted.has(f)) rmSync(join(cardDir, f));

  for (const s of SAMPLES) {
    const src = join(srcDir, `${s.name}.${s.sourceExt}`);
    const dst = join(cardDir, `${s.name}.${s.cardExt}`);
    if (existsSync(dst)) continue;
    console.log(`  sample ${s.name}`);
    if (!existsSync(src)) {
      execFileSync("ffmpeg", ["-loglevel", "error", "-y", ...signal(SECONDS), ...s.args, "-metadata", `title=${s.name}`, src], {
        stdio: "inherit",
      });
    }
    execFileSync(opentoneBin, ["convert", src, dst], { stdio: "ignore" });
  }
  return cardDir;
}
