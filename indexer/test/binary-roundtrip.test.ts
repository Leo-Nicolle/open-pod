import { describe, it, expect } from "vitest";
import { unserialize } from "../src/crawler";
import CrawlData from "./stubs/crawl-index.json";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  importFromBinary,
} from "../src/trie";
import type { CrawlIndex } from "../src/types";
import { readFile, readdir } from "fs/promises";
import path from "path";
import { __dirname } from "./utils";
import {
  exportIndexesToBinary,
  importStringIndexFromBinary,
  importRelationshipMapFromBinary,
} from "../src/music-indexes";

const keys = [
  "genre_to_tracks",
  "genre_to_artists",
  "genre_to_albums",
  "artist_to_tracks",
  "artist_to_albums",
  "album_to_tracks",
  "artist_index",
  "album_index",
  "genre_index",
  "track_index",
  "path_index",
];

describe("Binary Serialization Round-trip", () => {
  const createTestCrawlIndex = async (): Promise<CrawlIndex> => {
    return Promise.resolve(
      unserialize(JSON.stringify(CrawlData)) as CrawlIndex
    );
  };

  it("should read binary correctly", async () => {
    const binaryData = await readFile(
      path.join(__dirname, "./stubs/music_index.bin")
    );
    const importedSerialized = importFromBinary(binaryData);

    expect(importedSerialized.stringPool.length).toBe(8425);
    expect(importedSerialized.stringOffsets.length).toBe(885);
    expect(importedSerialized.nodes.length).toBe(933);
  });

  it.each(keys)("should export and import binary for %s", async (key) => {
    const crawlIndex = await createTestCrawlIndex();
    const binaries = exportIndexesToBinary(crawlIndex);

    expect(binaries[key]).toBeInstanceOf(Uint8Array);
    if (key === "artist_to_tracks") {
      debugger;
    }
    // Relationship maps
    if (
      key === "genre_to_tracks" ||
      key === "genre_to_artists" ||
      key === "genre_to_albums" ||
      key === "artist_to_tracks" ||
      key === "artist_to_albums" ||
      key === "album_to_tracks"
    ) {
      const imported = importRelationshipMapFromBinary(binaries[key]);
      expect(imported).toHaveProperty("entryCount");
      expect(imported).toHaveProperty("totalTargetCount");
      expect(imported).toHaveProperty("sourceIds");
      expect(imported).toHaveProperty("targetCounts");
      expect(imported).toHaveProperty("targetIds");
    } else {
      // String indexes
      const imported = importStringIndexFromBinary(binaries[key]);
      expect(imported).toHaveProperty("entryCount");
      expect(imported).toHaveProperty("stringData");
      expect(imported).toHaveProperty("stringOffsets");
      expect(imported).toHaveProperty("ids");
    }
  });

  it("should serialize and deserialize correctly", async () => {
    const crawlIndex = await createTestCrawlIndex();
    const trie = buildTrieFromCrawlIndex(crawlIndex);
    const originalSerialized = serializeTrie(trie);
    global.expected = originalSerialized;
    const binaryData = exportToBinary(originalSerialized);
    const importedSerialized = importFromBinary(binaryData);

    const encoded = new TextDecoder().decode(
      new TextEncoder().encode(originalSerialized.stringPool)
    );

    expect(importedSerialized.stringPool.length).toBe(encoded.length);
    expect(importedSerialized.stringPool).toBe(encoded);
    expect(importedSerialized.stringOffsets.length).toBe(
      originalSerialized.stringOffsets.length
    );
    expect(importedSerialized.stringOffsets).toEqual(
      originalSerialized.stringOffsets
    );
    expect(importedSerialized.nodes.length).toBe(
      originalSerialized.nodes.length
    );
    for (let i = 0; i < originalSerialized.nodes.length; i++) {
      const original = originalSerialized.nodes[i];
      const imported = importedSerialized.nodes[i];

      expect(imported.keyOffset).toBe(original.keyOffset);
      expect(imported.keyLength).toBe(original.keyLength);
      expect(imported.childCount).toBe(original.childCount);
      expect(imported.firstChildOffset).toBe(original.firstChildOffset);
      expect(imported.resultCount).toBe(original.resultCount);
      expect(imported.firstResultOffset).toBe(original.firstResultOffset);
    }
    expect(importedSerialized.results.length).toBe(
      originalSerialized.results.length
    );
    for (let i = 0; i < originalSerialized.results.length; i++) {
      const original = originalSerialized.results[i];
      const imported = importedSerialized.results[i];

      expect(imported.type).toBe(original.type);
      expect(imported.id).toBe(original.id);
      expect(imported.nameOffset).toBe(original.nameOffset);
      expect(imported.nameLength).toBe(original.nameLength);
      expect(imported.relevance).toBe(original.relevance);
    }
  });
});
