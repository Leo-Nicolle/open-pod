#!/usr/bin/env tsx

/**
 * Example usage of the path index functionality
 * This shows how to generate both the trie and path index, and how to use them together
 */

import { readMetadata } from "./src/crawler";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  buildPathIndexFromCrawlIndex,
  serializePathIndex,
  exportPathIndexToBinary,
  importFromBinary,
  importPathIndexFromBinary,
  searchTrie,
  lookupTrackPath,
} from "./src/trie";
import fs from "fs/promises";

async function exampleUsage() {
  console.log("=== Path Index Example Usage ===\n");

  // For this example, we'll use a mock crawl index
  // In real usage, you'd get this from readMetadata()
  const mockCrawlIndex = {
    indexToTrack: new Map([
      [1, "Bohemian Rhapsody"],
      [2, "Stairway to Heaven"],
      [3, "Hotel California"],
      [4, "Sweet Child O' Mine"],
    ]),
    indexToArtist: new Map([
      [1, "Queen"],
      [2, "Led Zeppelin"],
      [3, "Eagles"],
      [4, "Guns N' Roses"],
    ]),
    indexToAlbum: new Map([
      [1, "A Night at the Opera"],
      [2, "Led Zeppelin IV"],
      [3, "Hotel California"],
      [4, "Appetite for Destruction"],
    ]),
    indexToGenre: new Map([
      [1, "Rock"],
      [2, "Hard Rock"],
    ]),
    indexToPath: new Map([
      [1, "/music/Queen/A Night at the Opera/01 - Bohemian Rhapsody.mp3"],
      [2, "/music/Led Zeppelin/Led Zeppelin IV/04 - Stairway to Heaven.mp3"],
      [3, "/music/Eagles/Hotel California/01 - Hotel California.mp3"],
      [
        4,
        "/music/Guns N Roses/Appetite for Destruction/02 - Sweet Child O Mine.mp3",
      ],
    ]),
    metadataByTrackIndex: new Map(),
    artistToAlbums: new Map(),
    albumToTracks: new Map(),
    artistToTracks: new Map(),
    genreToAlbums: new Map(),
    genreToTracks: new Map(),
  };

  // Step 1: Build and serialize the trie
  console.log("1. Building search trie...");
  const trie = buildTrieFromCrawlIndex(mockCrawlIndex, {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 2,
    maxResults: 20,
  });

  const serializedTrie = serializeTrie(trie);
  const trieBinary = exportToBinary(serializedTrie);
  console.log(`   Trie binary size: ${trieBinary.length} bytes`);

  // Step 2: Build and serialize the path index
  console.log("2. Building path index...");
  const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);
  const serializedPathIndex = serializePathIndex(pathIndex);
  const pathBinary = exportPathIndexToBinary(serializedPathIndex);
  console.log(`   Path index binary size: ${pathBinary.length} bytes`);

  // Step 3: Save both files (simulate what would happen in real usage)
  await fs.writeFile("example_trie.bin", trieBinary);
  await fs.writeFile("example_paths.bin", pathBinary);
  console.log("3. Saved binary files: example_trie.bin, example_paths.bin");

  // Step 4: Load both files back (simulate STM32 usage)
  console.log("4. Loading binary files back...");
  const loadedTrieBinary = await fs.readFile("example_trie.bin");
  const loadedPathBinary = await fs.readFile("example_paths.bin");

  const loadedTrie = importFromBinary(loadedTrieBinary);
  const loadedPathIndex = importPathIndexFromBinary(loadedPathBinary);

  // Step 5: Demonstrate search and path lookup workflow
  console.log("\n=== Search and Path Lookup Demo ===");

  const searchQueries = ["hotel", "stair", "queen"];

  for (const query of searchQueries) {
    console.log(`\nSearching for: "${query}"`);

    // Search the trie (this would be done on STM32)
    const searchResults = searchTrie(trie, query, 10);

    console.log(`Found ${searchResults.length} results:`);

    for (const result of searchResults) {
      console.log(
        `  - [${result.type}] ${result.name} (ID: ${result.id}, Relevance: ${result.relevance})`
      );

      // If it's a track, show how to get the path
      if (result.type === "track") {
        const trackPath = lookupTrackPath(loadedPathIndex, result.id);
        console.log(`    📁 Path: ${trackPath}`);
      }
    }
  }

  // Step 6: Demonstrate direct track path lookup
  console.log("\n=== Direct Track Path Lookup ===");
  const trackIds = [1, 2, 3, 4, 999]; // Include non-existent ID

  for (const trackId of trackIds) {
    const path = lookupTrackPath(loadedPathIndex, trackId);
    if (path) {
      console.log(`Track ${trackId}: ${path}`);
    } else {
      console.log(`Track ${trackId}: Not found`);
    }
  }

  // Cleanup
  await fs.unlink("example_trie.bin");
  await fs.unlink("example_paths.bin");

  console.log("\n=== Example Complete ===");
  console.log("This demonstrates the complete workflow:");
  console.log("1. Build trie and path index from music collection");
  console.log("2. Export both to binary files");
  console.log("3. Load on STM32 and use for search + path lookup");
  console.log(
    "4. User searches → gets results → selects track → gets file path"
  );
}

// Run the example
exampleUsage().catch(console.error);
