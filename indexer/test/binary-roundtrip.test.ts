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

describe("Binary Serialization Round-trip", () => {
  // Create a simple test dataset
  const createTestCrawlIndex2 = (): CrawlIndex => {
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

    // Add test data
    indexToTrack.set(0, "Hello World");
    indexToTrack.set(1, "Test Song");
    indexToTrack.set(2, "Another Track");

    indexToArtist.set(0, "Test Artist");
    indexToArtist.set(1, "Another Artist");

    indexToAlbum.set(0, "Test Album");
    indexToAlbum.set(1, "Another Album");

    indexToGenre.set(0, "Rock");
    indexToGenre.set(1, "Jazz");

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

  const createTestCrawlIndex = async (): Promise<CrawlIndex> => {
    // const data = await fs.readFile("./stubs/crawl-index.json", "utf-8");
    return Promise.resolve(
      unserialize(JSON.stringify(CrawlData)) as CrawlIndex
    );
  };

  it("should read binary correctly", async () => {
    const ls = await readdir(".");
    const binaryData = await readFile(
      path.join(__dirname, "./stubs/music_index.bin")
    );
    const importedSerialized = importFromBinary(binaryData);

    expect(importedSerialized.stringPool.length).toBe(8425);
    // expect(importedSerialized.stringPool).toBe(encoded);
    expect(importedSerialized.stringOffsets.length).toBe(885);
    // expect(importedSerialized.stringOffsets).toEqual(
    //   originalSerialized.stringOffsets
    // );
    expect(importedSerialized.nodes.length).toBe(933);
  });

  it("should serialize and deserialize correctly", async () => {
    // Step 1: Create test data
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
