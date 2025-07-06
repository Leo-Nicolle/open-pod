import { describe, it, expect } from "vitest";
import {
  buildTrieFromCrawlIndex,
  searchTrie,
  serializeTrie,
  getSerializedTrieStats,
  exportToBinary,
} from "../src/trie";
import { unserialize } from "../src/crawler";
import data from "./stubs/crawl-index.json";
import type { CrawlIndex } from "../src/types";

describe("Trie Implementation", () => {
  const testIndex = unserialize(JSON.stringify(data)) as CrawlIndex;

  it("should build a trie from crawl index", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    expect(trie).toBeDefined();
    expect(trie.key).toBe("");
    expect(trie.children.length).toBeGreaterThan(0);
  });

  it("should find exact matches", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "swift", 10);

    expect(results.length).toBeGreaterThan(0);
    const queenResult = results.find(
      (r) => r.name.toLowerCase() === "swift guad"
    );
    expect(queenResult).toBeDefined();
    expect(queenResult?.type).toBe("artist");
  });

  it("should find partial matches", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const results = searchTrie(trie, "pour", 10);

    expect(results.length).toBeGreaterThan(0);
    const hotelResult = results.find((r) =>
      r.name.toLowerCase().includes("pour")
    );
    expect(hotelResult).toBeDefined();
  });

  it("should handle case insensitive search", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const lowerResults = searchTrie(trie, "swift", 10);
    const upperResults = searchTrie(trie, "SWIFT", 10);

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

describe("Trie Serialization", () => {
  // const testIndex = createTestCrawlIndex();
  const testIndex = unserialize(JSON.stringify(data)) as CrawlIndex;

  it("should serialize and maintain node structure integrity", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    // Basic structure validation
    expect(serialized.nodes.length).toBeGreaterThan(0);
    expect(serialized.results.length).toBeGreaterThan(0);
    expect(serialized.stringPool.length).toBeGreaterThan(0);
    expect(serialized.stringOffsets.length).toBeGreaterThan(0);

    // Validate that all nodes have reasonable values
    for (let i = 0; i < serialized.nodes.length; i++) {
      const node = serialized.nodes[i];

      // Key offset should be within string pool bounds
      expect(node.keyOffset).toBeGreaterThanOrEqual(0);
      expect(node.keyOffset).toBeLessThan(serialized.stringPool.length);

      // Key length should be reasonable (not corrupted)
      expect(node.keyLength).toBeGreaterThanOrEqual(0);
      expect(node.keyLength).toBeLessThan(100); // No single key should be > 100 chars

      // Child count should be reasonable (not corrupted)
      expect(node.childCount).toBeGreaterThanOrEqual(0);
      expect(node.childCount).toBeLessThan(1000); // Should never have 1000+ children

      // If node has children, firstChildOffset should be valid
      if (node.childCount > 0) {
        expect(node.firstChildOffset).toBeGreaterThanOrEqual(0);
        expect(node.firstChildOffset).toBeLessThan(serialized.nodes.length);
        expect(node.firstChildOffset + node.childCount).toBeLessThanOrEqual(
          serialized.nodes.length
        );
      }

      // Result count should be reasonable
      expect(node.resultCount).toBeGreaterThanOrEqual(0);
      expect(node.resultCount).toBeLessThan(100); // No node should have 100+ results

      // If node has results, firstResultOffset should be valid
      if (node.resultCount > 0) {
        expect(node.firstResultOffset).toBeGreaterThanOrEqual(0);
        expect(node.firstResultOffset).toBeLessThan(serialized.results.length);
        expect(node.firstResultOffset + node.resultCount).toBeLessThanOrEqual(
          serialized.results.length
        );
      }
    }
  });

  it("should have valid child offset references", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    // Check that child offsets form a valid tree structure
    for (let i = 0; i < serialized.nodes.length; i++) {
      const node = serialized.nodes[i];

      if (node.childCount > 0) {
        // All children should be valid node indices
        for (let j = 0; j < node.childCount; j++) {
          const childIndex = node.firstChildOffset + j;
          expect(childIndex).toBeLessThan(serialized.nodes.length);
          expect(childIndex).toBeGreaterThanOrEqual(0);

          // Child should exist
          const child = serialized.nodes[childIndex];
          expect(child).toBeDefined();
        }
      }
    }
  });

  it("should have valid string pool references", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    // Check that all string offsets are valid
    for (const offset of serialized.stringOffsets) {
      expect(offset).toBeGreaterThanOrEqual(0);
      expect(offset).toBeLessThan(serialized.stringPool.length);
    }

    // Check that all node key references are valid
    for (const node of serialized.nodes) {
      if (node.keyLength > 0) {
        expect(node.keyOffset + node.keyLength).toBeLessThanOrEqual(
          serialized.stringPool.length
        );
      }
    }

    // Check that all result name references are valid
    for (const result of serialized.results) {
      if (result.nameLength > 0) {
        expect(result.nameOffset + result.nameLength).toBeLessThanOrEqual(
          serialized.stringPool.length
        );
      }
    }
  });

  it("should export to binary format correctly", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);
    const binaryData = exportToBinary(serialized);

    expect(binaryData).toBeInstanceOf(Uint8Array);
    expect(binaryData.length).toBeGreaterThan(16); // At least header size

    // Read and validate header
    const view = new DataView(binaryData.buffer);
    const header = {
      string_pool_size: view.getUint32(0, true),
      node_count: view.getUint32(4, true),
      result_count: view.getUint32(8, true),
      string_offset_count: view.getUint32(12, true),
    };

    expect(header.string_pool_size).toBe(serialized.stringPool.length);
    expect(header.node_count).toBe(serialized.nodes.length);
    expect(header.result_count).toBe(serialized.results.length);
    expect(header.string_offset_count).toBe(serialized.stringOffsets.length);
  });

  it("should not have nodes with excessive children", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    // Check that no node has an unreasonable number of children
    const maxReasonableChildren = 100; // Generous upper bound

    for (let i = 0; i < serialized.nodes.length; i++) {
      const node = serialized.nodes[i];
      expect(node.childCount).toBeLessThan(maxReasonableChildren);
    }
  });

  it("should preserve search functionality after serialization", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const originalResults = searchTrie(trie, "swift", 10);

    // Serialize and check that we can still find the same data
    const serialized = serializeTrie(trie);

    // Find the queen result in serialized data
    const queenResults = serialized.results.filter((result) => {
      const name = serialized.stringPool.slice(
        result.nameOffset,
        result.nameOffset + result.nameLength
      );
      return name.toLowerCase() === "swift guad";
    });

    expect(queenResults.length).toBeGreaterThan(0);
    expect(originalResults.length).toBeGreaterThan(0);
  });

  it("should handle breadth-first node ordering correctly", () => {
    const trie = buildTrieFromCrawlIndex(testIndex);
    const serialized = serializeTrie(trie);

    // Root should be at index 0
    const rootNode = serialized.nodes[0];
    expect(rootNode).toBeDefined();

    // Root should have empty key
    const rootKey = serialized.stringPool.slice(
      rootNode.keyOffset,
      rootNode.keyOffset + rootNode.keyLength
    );
    expect(rootKey).toBe("");

    // If root has children, they should be at consecutive indices starting from 1
    if (rootNode.childCount > 0) {
      expect(rootNode.firstChildOffset).toBe(1);

      // All children should be valid
      for (let i = 0; i < rootNode.childCount; i++) {
        const childIndex = rootNode.firstChildOffset + i;
        expect(childIndex).toBeLessThan(serialized.nodes.length);

        const child = serialized.nodes[childIndex];
        expect(child).toBeDefined();
        expect(child.keyLength).toBeGreaterThan(0); // Children should have non-empty keys
      }
    }
  });

  it("should create deterministic output", () => {
    const trie1 = buildTrieFromCrawlIndex(testIndex);
    const trie2 = buildTrieFromCrawlIndex(testIndex);

    const serialized1 = serializeTrie(trie1);
    const serialized2 = serializeTrie(trie2);

    // Should produce identical serialization
    expect(serialized1.nodes.length).toBe(serialized2.nodes.length);
    expect(serialized1.results.length).toBe(serialized2.results.length);
    expect(serialized1.stringPool).toBe(serialized2.stringPool);

    // Binary output should be identical
    const binary1 = exportToBinary(serialized1);
    const binary2 = exportToBinary(serialized2);

    expect(binary1.length).toBe(binary2.length);
    expect(Array.from(binary1)).toEqual(Array.from(binary2));
  });
});
