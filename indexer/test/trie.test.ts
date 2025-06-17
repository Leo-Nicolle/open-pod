import { describe, it, expect } from "vitest";
import {
  buildTrieFromCrawlIndex,
  searchTrie,
  serializeTrie,
  getSerializedTrieStats,
} from "../src/trie";
import type { CrawlIndex } from "../src/types";

// Create a mock crawl index for testing
function createTestCrawlIndex(): CrawlIndex {
  const indexToTrack = new Map([
    [0, "Bohemian Rhapsody"],
    [1, "Stairway to Heaven"],
    [2, "Hotel California"],
    [3, "Sweet Child O' Mine"],
    [4, "Imagine"],
  ]);

  const indexToArtist = new Map([
    [0, "Queen"],
    [1, "Led Zeppelin"],
    [2, "Eagles"],
    [3, "Guns N' Roses"],
    [4, "John Lennon"],
  ]);

  const indexToAlbum = new Map([
    [0, "A Night at the Opera"],
    [1, "Led Zeppelin IV"],
    [2, "Hotel California"],
    [3, "Appetite for Destruction"],
    [4, "Imagine"],
  ]);

  const indexToGenre = new Map([
    [0, "Rock"],
    [1, "Classic Rock"],
    [2, "Pop"],
  ]);

  const indexToPath = new Map([
    [0, "Queen/A Night at the Opera/Bohemian Rhapsody.mp3"],
    [1, "Led Zeppelin/Led Zeppelin IV/Stairway to Heaven.mp3"],
    [2, "Eagles/Hotel California/Hotel California.mp3"],
    [3, "Guns N Roses/Appetite for Destruction/Sweet Child O Mine.mp3"],
    [4, "John Lennon/Imagine/Imagine.mp3"],
  ]);

  // Create simple relationship maps
  const artistToAlbums = new Map();
  const albumToTracks = new Map();
  const artistToTracks = new Map();
  const genreToTracks = new Map();
  const genreToAlbums = new Map();
  const metadataByTrackIndex = new Map();

  // Populate with test data
  for (let i = 0; i < 5; i++) {
    artistToAlbums.set(i, new Set([i]));
    albumToTracks.set(i, new Set([i]));
    artistToTracks.set(i, new Set([i]));
    genreToTracks.set(i % 3, new Set([i]));
    genreToAlbums.set(i % 3, new Set([i]));

    metadataByTrackIndex.set(i, {
      title: indexToTrack.get(i)!,
      artist: indexToArtist.get(i)!,
      album: indexToAlbum.get(i)!,
      genre: indexToGenre.get(i % 3)!,
      year: 1970 + i * 5,
      index: i + 1,
      duration: 180 + i * 30,
    });
  }

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
}

describe("Trie Implementation", () => {
  const testIndex = createTestCrawlIndex();

  it("should build a trie from crawl index", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    expect(trie).toBeDefined();
    expect(trie.key).toBe("");
    expect(trie.children.length).toBeGreaterThan(0);
  });

  it("should find exact matches", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "queen", 10);

    expect(results.length).toBeGreaterThan(0);
    const queenResult = results.find((r) => r.name.toLowerCase() === "queen");
    expect(queenResult).toBeDefined();
    expect(queenResult?.type).toBe("artist");
  });

  it("should find partial matches", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "hotel", 10);

    expect(results.length).toBeGreaterThan(0);
    const hotelResult = results.find((r) =>
      r.name.toLowerCase().includes("hotel")
    );
    expect(hotelResult).toBeDefined();
  });

  it("should handle case insensitive search", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const lowerResults = searchTrie(trie, "queen", 10);
    const upperResults = searchTrie(trie, "QUEEN", 10);

    expect(lowerResults.length).toBe(upperResults.length);
  });

  it("should serialize trie correctly", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    expect(serialized.stringPool).toBeDefined();
    expect(serialized.nodes.length).toBeGreaterThan(0);
    expect(serialized.results.length).toBeGreaterThan(0);
    expect(serialized.stringOffsets.length).toBeGreaterThan(0);
  });

  it("should calculate stats correctly", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);
    const stats = getSerializedTrieStats(serialized);

    expect(stats.stringPoolSize).toBeGreaterThan(0);
    expect(stats.nodeCount).toBeGreaterThan(0);
    expect(stats.resultCount).toBeGreaterThan(0);
    expect(stats.totalSize).toBeGreaterThan(0);
  });

  it("should sort results by relevance", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "rock", 10);

    if (results.length > 1) {
      for (let i = 1; i < results.length; i++) {
        expect(results[i - 1].relevance).toBeGreaterThanOrEqual(
          results[i].relevance
        );
      }
    }
  });

  it("should limit results correctly", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const maxResults = 3;
    const results = searchTrie(trie, "a", maxResults);

    expect(results.length).toBeLessThanOrEqual(maxResults);
  });

  it("should handle empty queries", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "", 10);

    // Empty query should return no results or all results depending on implementation
    expect(results).toBeDefined();
    expect(Array.isArray(results)).toBe(true);
  });

  it("should handle non-existent queries", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "xyznothingfound", 10);

    expect(results.length).toBe(0);
  });
});
