import { describe, it, expect } from "vitest";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  importFromBinary,
} from "../src/trie";
import type { CrawlIndex } from "../src/types";

describe("Accent Sanitization", () => {
  // Create test data with accented characters
  const createAccentedTestData = (): CrawlIndex => {
    const indexToTrack = new Map<number, string>();
    const indexToArtist = new Map<number, string>();
    const indexToAlbum = new Map<number, string>();
    const indexToGenre = new Map<number, string>();
    const indexToPath = new Map<number, string>();

    const artistToAlbums = new Map<number, Set<number>>();
    const albumToTracks = new Map<number, Set<number>>();
    const artistToTracks = new Map<number, Set<number>>();
    const genreToTracks = new Map<number, Set<number>>();
    const genreToAlbums = new Map<number, Set<number>>();
    const metadataByTrackIndex = new Map();

    // Add test data with French accents and special characters
    indexToTrack.set(0, "Café de la Paix");
    indexToTrack.set(1, "L'été à Paris");
    indexToTrack.set(2, "Noël en Provence");
    indexToTrack.set(3, "Cœur brisé");

    indexToArtist.set(0, "François Müller");
    indexToArtist.set(1, "Céline Dión");

    indexToAlbum.set(0, "Chansons d'été");
    indexToAlbum.set(1, "Mélodies françaises");

    indexToGenre.set(0, "Français");
    indexToGenre.set(1, "Européen");

    return {
      indexToTrack,
      indexToArtist,
      indexToAlbum,
      indexToGenre,
      indexToPath,
      artistToAlbums,
      albumToTracks,
      artistToTracks,
      genreToTracks,
      genreToAlbums,
      metadataByTrackIndex,
    };
  };

  it("should remove accents and convert to ASCII", () => {
    console.log("\n=== ACCENT SANITIZATION TEST ===");

    // Step 1: Create test data with accents
    const crawlIndex = createAccentedTestData();
    console.log("✓ Created test data with accented characters");

    // Step 2: Build trie
    const trie = buildTrieFromCrawlIndex(crawlIndex);
    console.log("✓ Built trie from crawl index");

    // Step 3: Serialize trie
    const serialized = serializeTrie(trie);
    console.log("✓ Serialized trie");
    console.log(`  - String pool: "${serialized.stringPool}"`);

    // Step 4: Check that string pool contains only ASCII characters
    const hasNonAscii = /[^\x00-\x7F]/.test(serialized.stringPool);
    expect(hasNonAscii).toBe(false);
    console.log("✓ String pool contains only ASCII characters");

    // Step 5: Export to binary and import back
    const binaryData = exportToBinary(serialized);
    const imported = importFromBinary(binaryData);
    console.log("✓ Binary round-trip successful");

    // Step 6: Verify that imported data also has no non-ASCII characters
    const importedHasNonAscii = /[^\x00-\x7F]/.test(imported.stringPool);
    expect(importedHasNonAscii).toBe(false);
    console.log("✓ Imported string pool contains only ASCII characters");

    // Step 7: Verify specific transformations
    expect(serialized.stringPool).toContain("cafe"); // "Café" -> "cafe"
    expect(serialized.stringPool).toContain("ete"); // "été" -> "ete"
    expect(serialized.stringPool).toContain("noel"); // "Noël" -> "noel"
    expect(serialized.stringPool).toContain("coeur"); // "Cœur" -> "coeur"
    expect(serialized.stringPool).toContain("francois"); // "François" -> "francois"
    expect(serialized.stringPool).toContain("celine"); // "Céline" -> "celine"
    expect(serialized.stringPool).toContain("muller"); // "Müller" -> "muller"
    expect(serialized.stringPool).toContain("dion"); // "Dión" -> "dion"

    console.log("✓ All accent transformations verified");
    console.log("\n✅ Accent sanitization test PASSED!");
  });

  it("should handle UTF-8 encoding correctly", () => {
    console.log("\n=== UTF-8 ENCODING TEST ===");

    const crawlIndex = createAccentedTestData();
    const trie = buildTrieFromCrawlIndex(crawlIndex);
    const serialized = serializeTrie(trie);

    // Check that string pool byte length matches character length (ASCII only)
    const encoder = new TextEncoder();
    const stringPoolBytes = encoder.encode(serialized.stringPool);

    console.log(
      `String pool character length: ${serialized.stringPool.length}`
    );
    console.log(`String pool byte length: ${stringPoolBytes.length}`);

    // For ASCII-only strings, byte length should equal character length
    expect(stringPoolBytes.length).toBe(serialized.stringPool.length);
    console.log(
      "✓ String pool byte length equals character length (ASCII confirmed)"
    );

    // Test binary serialization
    const binaryData = exportToBinary(serialized);
    const imported = importFromBinary(binaryData);

    // Verify round-trip integrity
    expect(imported.stringPool).toBe(serialized.stringPool);
    expect(imported.stringOffsets).toEqual(serialized.stringOffsets);

    console.log("✓ UTF-8 encoding test PASSED!");
  });
});
