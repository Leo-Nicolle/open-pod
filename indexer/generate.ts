#!/usr/bin/env tsx

import { readMetadata, serialize } from "./src/crawler";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  getSerializedTrieStats,
  importFromBinary,
} from "./src/trie";
import fs from "fs/promises";
import path from "path";

async function main() {
  const musicPath = process.argv[2];
  const outputPath = process.argv[3] || "./music_index.bin";

  if (!musicPath) {
    console.error(
      "Usage: tsx generate-index.ts <music-directory> [output-file]"
    );
    console.error(
      "Example: tsx generate-index.ts /path/to/music ./music_index.bin"
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

  // Step 2: Build trie
  console.log("🌳 Building search trie...");
  const trie = buildTrieFromCrawlIndex(crawlIndex, {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 2,
    maxResults: 20,
  });
  debugger;

  // Step 3: Serialize trie
  console.log("📦 Serializing trie...");
  const serialized = serializeTrie(trie);

  // Step 4: Get statistics
  const stats = getSerializedTrieStats(serialized);
  console.log("📊 Index statistics:");
  console.log(`  - String pool: ${stats.stringPoolSize} bytes`);
  console.log(`  - Nodes: ${stats.nodeCount}`);
  console.log(`  - Results: ${stats.resultCount}`);
  console.log(`  - Total size: ${stats.totalSize} bytes`);

  // Step 5: Export to binary
  console.log("💾 Exporting to binary...");
  const binaryData = exportToBinary(serialized);
  console.log(`  - Binary size: ${binaryData.length} bytes`);

  // Step 6: Save binary file
  await fs.writeFile(outputPath, binaryData);
  console.log(`✅ Binary file saved: ${outputPath}`);

  const read = await fs.readFile(outputPath);
  const parsed = importFromBinary(read);
  const trieStats = getSerializedTrieStats(parsed);
  console.log("📖 Parsed binary statistics:");
  console.log(`  - String pool: ${trieStats.stringPoolSize} bytes`);
  console.log(`  - Nodes: ${trieStats.nodeCount}`);
  console.log(`  - Results: ${trieStats.resultCount}`);
  console.log(`  - Total size: ${trieStats.totalSize} bytes`);

  console.log("");
  console.log("🎵 Music index generation complete!");
  console.log("");
  console.log("Next steps:");
  console.log(`1. Copy ${path.basename(outputPath)} to your SD card root`);
  console.log("2. Use MusicIndex::init() in your STM32 code");
  console.log("3. Search with MusicIndex::searchByPrefix()");
}

main().catch((error) => {
  console.error("Error:", error.message);
  process.exit(1);
});
