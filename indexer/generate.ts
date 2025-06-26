#!/usr/bin/env tsx

import { readMetadata, serialize } from "./src/crawler";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  getSerializedTrieStats,
  exportIndexesToBinary,
} from "./src";
import fs from "fs/promises";
import path from "path";

async function main() {
  const musicPath = process.argv[2];
  const outputPath = process.argv[3] || ".";
  const exists = await fs
    .access(outputPath)
    .then(() => true)
    .catch(() => false);
  const isDirectory = await fs
    .lstat(outputPath)
    .then((stat) => stat.isDirectory())
    .catch(() => false);
  if (!exists || !isDirectory) {
    console.error(
      `Error: Output folder '${outputPath}' does not exist or is not a directory`
    );
    process.exit(1);
  }
  if (!musicPath) {
    console.error(
      "Usage: tsx generate-index.ts <music-directory> [output-file]"
    );
    console.error(
      "Example: tsx generate-index.ts /path/to/music /media/me/SD_card/openpod"
    );
    process.exit(1);
  }

  try {
    // Check if music directory exists
    await fs.access(musicPath);
  } catch {
    console.error(`Error: Music directory '${musicPath}' not found`);
    process.exit(1);
  }

  console.log("=== Music Index Generator ===");
  console.log(`Input: ${musicPath}`);
  console.log(`Output: ${outputPath}`);
  console.log("");

  // Step 1: Crawl music directory
  console.log("📁 Crawling music directory...");
  const crawlIndex = await readMetadata(musicPath);
  await fs.writeFile("test/stubs/crawl-index.json", serialize(crawlIndex));

  console.log(`Found:`);
  console.log(`  - ${crawlIndex.indexToTrack.size} tracks`);
  console.log(`  - ${crawlIndex.indexToArtist.size} artists`);
  console.log(`  - ${crawlIndex.indexToAlbum.size} albums`);
  console.log(`  - ${crawlIndex.indexToGenre.size} genres`);
  console.log("");

  console.log("🌳 Building search trie...");
  const trie = buildTrieFromCrawlIndex(crawlIndex, {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 2,
    maxResults: 20,
  });

  console.log("📦 Serializing trie...");
  const serialized = serializeTrie(trie);
  const stats = getSerializedTrieStats(serialized);
  console.log("📊 Index statistics:");
  console.log(`  - String pool: ${stats.stringPoolSize} bytes`);
  console.log(`  - Nodes: ${stats.nodeCount}`);
  console.log(`  - Results: ${stats.resultCount}`);
  console.log(`  - Total size: ${stats.totalSize} bytes`);

  const binaryData = exportToBinary(serialized);
  console.log(`  - Binary size: ${binaryData.length} bytes`);
  await fs.writeFile(path.resolve(outputPath, "music_index.bin"), binaryData);
  console.log(`✅ Binary file saved: ${outputPath}`);
  const indexes = exportIndexesToBinary(crawlIndex);
  for (const [key, value] of Object.entries(indexes)) {
    await fs.writeFile(path.resolve(outputPath, `${key}.bin`), value);
  }

  console.log("🎵 Music index generation complete!");
  console.log("");
  console.log("Next steps:");
  console.log(`1. Copy the .bin files to your SD card root`);
  console.log("2. Use MusicIndex::init() in your STM32 code");
  console.log("3. Search with MusicIndex::searchByPrefix()");
  console.log("4. Use the path index to get file paths for selected tracks");
}

main().catch((error) => {
  console.error("Error:", error.message);
  process.exit(1);
});
