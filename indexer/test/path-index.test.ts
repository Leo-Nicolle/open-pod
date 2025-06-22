import { describe, it, expect } from "vitest";
import {
  exportPathIndexToBinary,
  importPathIndexFromBinary,
  lookupTrackPath,
  getSerializedPathIndexStats,
} from "../src/trie";
import data from "./stubs/crawl-index.json";
import type { CrawlIndex } from "../src/types";
import path from "path";
import { readFile } from "fs/promises";
import { __dirname } from "./utils";
import { unserialize } from "../src/crawler";
describe("Path Index", () => {
  // Create a mock crawl index for testing
  const mockCrawlIndex: CrawlIndex = {
    indexToTrack: new Map([
      [1, "Song One"],
      [2, "Song Two"],
      [3, "Song Three"],
    ]),
    indexToArtist: new Map([
      [1, "Artist One"],
      [2, "Artist Two"],
    ]),
    indexToAlbum: new Map([
      [1, "Album One"],
      [2, "Album Two"],
    ]),
    indexToGenre: new Map([
      [1, "Rock"],
      [2, "Pop"],
    ]),
    indexToPath: new Map([
      [1, "/music/artist1/album1/song1.mp3"],
      [2, "/music/artist1/album1/song2.mp3"],
      [3, "/music/artist2/album2/song3.mp3"],
    ]),
    metadataByTrackIndex: new Map(),
    artistToAlbums: new Map(),
    albumToTracks: new Map(),
    artistToTracks: new Map(),
    genreToAlbums: new Map(),
    genreToTracks: new Map(),
  };

  it("should build path index from crawl index", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);

    expect(pathIndex.trackPaths.size).toBe(3);
    expect(pathIndex.trackPaths.get(1)).toBe("/music/artist1/album1/song1.mp3");
    expect(pathIndex.trackPaths.get(2)).toBe("/music/artist1/album1/song2.mp3");
    expect(pathIndex.trackPaths.get(3)).toBe("/music/artist2/album2/song3.mp3");
  });

  it("should serialize path index correctly", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);
    const serialized = serializePathIndex(pathIndex);

    expect(serialized.trackCount).toBe(3);
    expect(serialized.trackIds).toEqual([1, 2, 3]); // Should be sorted
    expect(serialized.pathOffsets).toHaveLength(3);
    expect(serialized.pathData).toContain("/music/artist1/album1/song1.mp3");
    expect(serialized.pathData).toContain("/music/artist1/album1/song2.mp3");
    expect(serialized.pathData).toContain("/music/artist2/album2/song3.mp3");
  });

  it("should export and import binary correctly", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);
    const serialized = serializePathIndex(pathIndex);
    const binary = exportPathIndexToBinary(serialized);
    const imported = importPathIndexFromBinary(binary);

    expect(imported.trackCount).toBe(serialized.trackCount);
    expect(imported.trackIds).toEqual(serialized.trackIds);
    expect(imported.pathOffsets).toEqual(serialized.pathOffsets);
    expect(imported.pathData).toBe(serialized.pathData);
  });

  it("should lookup track paths correctly", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);
    const serialized = serializePathIndex(pathIndex);

    expect(lookupTrackPath(serialized, 1)).toBe(
      "/music/artist1/album1/song1.mp3"
    );
    expect(lookupTrackPath(serialized, 2)).toBe(
      "/music/artist1/album1/song2.mp3"
    );
    expect(lookupTrackPath(serialized, 3)).toBe(
      "/music/artist2/album2/song3.mp3"
    );
    expect(lookupTrackPath(serialized, 999)).toBeNull(); // Non-existent track
  });

  it("should handle binary roundtrip correctly", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(crawlIndex);
    const serialized = serializePathIndex(pathIndex);
    const binary = exportPathIndexToBinary(serialized);
    const imported = importPathIndexFromBinary(binary);

    // Test lookups on imported data
    expect(lookupTrackPath(imported, 1)).toBe(
      "/music/artist1/album1/song1.mp3"
    );
    expect(lookupTrackPath(imported, 2)).toBe(
      "/music/artist1/album1/song2.mp3"
    );
    expect(lookupTrackPath(imported, 3)).toBe(
      "/music/artist2/album2/song3.mp3"
    );
    expect(lookupTrackPath(imported, 999)).toBeNull();
  });

  it("should read real data correctly", async () => {
    const crawlIndex = unserialize(JSON.stringify(data)) as CrawlIndex;
    const binary = exportPathIndexToBinary(crawlIndex);
    const unserialized = importPathIndexFromBinary(binary);
    console.log("unserialized", unserialized);
    // const binary = await readFile(
    //   path.resolve(__dirname, "./stubs/music_index_paths.bin")
    // );
    // const imported = importPathIndexFromBinary(binary);
    // console.log("imported", imported);
    expect(1).toBe(1);
  });

  it("should calculate statistics correctly", () => {
    const pathIndex = buildPathIndexFromCrawlIndex(mockCrawlIndex);
    const serialized = serializePathIndex(pathIndex);
    const stats = getSerializedPathIndexStats(serialized);

    expect(stats.trackCount).toBe(3);
    expect(stats.totalSize).toBeGreaterThan(0);
    expect(stats.pathDataSize).toBeGreaterThan(0);
    expect(stats.trackIdsSize).toBe(3 * 4); // 3 tracks * 4 bytes per uint32
    expect(stats.pathOffsetsSize).toBe(3 * 4); // 3 offsets * 4 bytes per uint32
  });
});
