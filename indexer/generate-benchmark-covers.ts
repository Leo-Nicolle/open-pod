#!/usr/bin/env tsx
// Produces two labeled sets of cover fixtures (QOI and raw565) from the same
// music library, for the on-device format benchmark in
// software/test/onboard/. The production `generate.ts` only ever writes one
// format (DEFAULT_THUMBNAIL_CONFIG); this script exists purely to give the
// benchmark firmware something to compare against, on real hardware, before
// that default is chosen.
//
// Usage: tsx generate-benchmark-covers.ts <music-directory> <output-dir>
// Writes <output-dir>/bench_qoi/{thumbs.bin,album_to_cover.bin} and
// <output-dir>/bench_raw565/{thumbs.bin,album_to_cover.bin}. Copy both
// bench_* folders onto the SD card under /openpod/ for the benchmark to read.

import fs from "fs/promises";
import path from "path";
import { readMetadata } from "./src/crawler";
import {
  packThumbnails,
  exportAlbumCoverIndexToBinary,
  type ThumbnailFormat,
} from "./src/thumbnails";

async function generateSet(
  musicPath: string,
  outputPath: string,
  format: ThumbnailFormat
) {
  const dir = path.join(outputPath, `bench_${format}`);
  await fs.mkdir(dir, { recursive: true });

  console.log(`📁 Crawling for ${format}...`);
  const crawlIndex = await readMetadata(musicPath, { format });

  const entries = Array.from(crawlIndex.albumThumbnails.entries())
    .sort(([a], [b]) => a - b)
    .map(([albumId, result]) => ({ albumId, result }));
  const { blob, index } = packThumbnails(entries);

  await fs.writeFile(path.join(dir, "thumbs.bin"), blob);
  await fs.writeFile(
    path.join(dir, "album_to_cover.bin"),
    exportAlbumCoverIndexToBinary(index)
  );

  const placeholderCount = entries.filter(
    (e) => e.result.isPlaceholder
  ).length;
  console.log(
    `  - ${format}: ${entries.length} covers (${placeholderCount} placeholders), thumbs.bin = ${blob.length} bytes -> ${dir}`
  );
}

async function main() {
  const musicPath = process.argv[2];
  const outputPath = process.argv[3];
  if (!musicPath || !outputPath) {
    console.error(
      "Usage: tsx generate-benchmark-covers.ts <music-directory> <output-dir>"
    );
    process.exit(1);
  }

  await generateSet(musicPath, outputPath, "qoi");
  await generateSet(musicPath, outputPath, "raw565");

  console.log("");
  console.log("✅ Benchmark fixtures generated.");
  console.log(
    `Copy ${path.join(outputPath, "bench_qoi")} and ${path.join(
      outputPath,
      "bench_raw565"
    )} onto the SD card under /openpod/ for the cover_benchmark firmware.`
  );
}

main().catch((error) => {
  console.error("Error:", error.message);
  process.exit(1);
});
