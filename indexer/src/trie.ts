import type { CrawlIndex } from "./types";

type TrieNode = {
  key: string; // segment du préfixe (ex: "beat", "les")
  children: TrieNode[]; // enfants
  results?: SearchResult[]; // défini uniquement en feuille
};

type SearchResult = {
  type: "track" | "artist" | "album" | "genre";
  id: number;
  name: string;
  relevance: number; // 0-100, higher = more relevant
};

type SerializedResult = {
  type: number; // 0=track, 1=artist, 2=album, 3=genre
  id: number;
  nameOffset: number;
  nameLength: number;
  relevance: number;
};

type SerializedNode = {
  keyOffset: number; // dans string pool
  keyLength: number;
  childCount: number;
  firstChildOffset: number; // dans la table de noeuds
  resultCount: number;
  firstResultOffset: number; // dans la table de résultats
};

type SerializedTrie = {
  stringPool: string;
  stringOffsets: number[];
  nodes: SerializedNode[];
  results: SerializedResult[];
};

// Configuration for trie building
type TrieConfig = {
  includePartialMatches: boolean; // Include partial word matches
  caseSensitive: boolean;
  minPrefixLength: number; // Minimum prefix length to index
  maxResults: number; // Maximum results per node
};

function insert(node: TrieNode, key: string, result: SearchResult): void {
  for (let i = 0; i < node.children.length; i++) {
    const child = node.children[i];
    const commonPrefix = getCommonPrefix(key, child.key);

    if (commonPrefix.length === 0) continue;

    if (commonPrefix.length < child.key.length) {
      // Split node
      const suffix = child.key.slice(commonPrefix.length);
      const newChild: TrieNode = {
        key: suffix,
        children: child.children,
        results: child.results,
      };

      child.key = commonPrefix;
      child.children = [newChild];
      delete child.results;
    }

    const remaining = key.slice(commonPrefix.length);
    if (remaining.length === 0) {
      if (!child.results) child.results = [];
      child.results.push(result);
      return;
    }

    insert(child, remaining, result);
    return;
  }

  // No matching prefix — add new leaf
  node.children.push({ key, children: [], results: [result] });
}

function getCommonPrefix(a: string, b: string): string {
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  return a.slice(0, i);
}

// Helper function to normalize strings for search
function normalizeString(str: string, caseSensitive: boolean = false): string {
  let normalized = str.trim();
  if (!caseSensitive) {
    normalized = normalized.toLowerCase();
  }
  // Remove common articles and prepositions for better matching
  normalized = normalized.replace(/^(the|a|an|le|la|les|un|une|des)\s+/i, "");
  return normalized;
}

// Calculate relevance score based on match type and position
function calculateRelevance(
  searchTerm: string,
  matchedTerm: string,
  matchType: "exact" | "prefix" | "partial"
): number {
  const termLength = searchTerm.length;
  const matchLength = matchedTerm.length;

  switch (matchType) {
    case "exact":
      return 100;
    case "prefix":
      return Math.max(50, 100 - (matchLength - termLength) * 2);
    case "partial":
      return Math.max(25, 50 - (matchLength - termLength));
    default:
      return 0;
  }
}

export function buildTrieFromCrawlIndex(
  crawlIndex: CrawlIndex,
  config: TrieConfig = {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 1,
    maxResults: 50,
  }
): TrieNode {
  const root: TrieNode = { key: "", children: [] };

  // Helper function to add entries to trie
  function addToTrie(
    name: string,
    type: "track" | "artist" | "album" | "genre",
    id: number
  ) {
    const normalizedName = normalizeString(name, config.caseSensitive);

    if (normalizedName.length < config.minPrefixLength) return;

    const result: SearchResult = {
      type,
      id,
      name,
      relevance: calculateRelevance(normalizedName, normalizedName, "exact"),
    };

    // Insert full name
    insert(root, normalizedName, result);

    // Insert partial matches if enabled
    if (config.includePartialMatches) {
      const words = normalizedName.split(/\s+/);
      for (const word of words) {
        if (word.length >= config.minPrefixLength && word !== normalizedName) {
          const partialResult: SearchResult = {
            ...result,
            relevance: calculateRelevance(word, normalizedName, "partial"),
          };
          insert(root, word, partialResult);
        }
      }
    }
  }

  // Add all tracks
  for (const [trackId, trackName] of crawlIndex.indexToTrack.entries()) {
    addToTrie(trackName, "track", trackId);
  }

  // Add all artists
  for (const [artistId, artistName] of crawlIndex.indexToArtist.entries()) {
    addToTrie(artistName, "artist", artistId);
  }

  // Add all albums
  for (const [albumId, albumName] of crawlIndex.indexToAlbum.entries()) {
    addToTrie(albumName, "album", albumId);
  }

  // Add all genres
  for (const [genreId, genreName] of crawlIndex.indexToGenre.entries()) {
    addToTrie(genreName, "genre", genreId);
  }

  // Sort results by relevance in each node
  function sortResults(node: TrieNode) {
    if (node.results) {
      node.results.sort((a, b) => b.relevance - a.relevance);
      // Limit results per node
      if (node.results.length > config.maxResults) {
        node.results = node.results.slice(0, config.maxResults);
      }
    }
    for (const child of node.children) {
      sortResults(child);
    }
  }

  sortResults(root);
  return root;
}

export function serializeTrie(root: TrieNode): SerializedTrie {
  const stringTable = new Map<string, number>();
  const stringPool: string[] = [];
  const stringOffsets: number[] = [];

  const nodes: SerializedNode[] = [];
  const results: SerializedResult[] = [];

  function intern(str: string): number {
    if (stringTable.has(str)) return stringTable.get(str)!;
    const offset = stringPool.join("").length;
    stringTable.set(str, offset);
    stringOffsets.push(offset);
    stringPool.push(str);
    return offset;
  }

  function getTypeNumber(type: string): number {
    switch (type) {
      case "track":
        return 0;
      case "artist":
        return 1;
      case "album":
        return 2;
      case "genre":
        return 3;
      default:
        return 0;
    }
  }

  function walk(node: TrieNode): number {
    const keyOffset = intern(node.key);
    const keyLength = node.key.length;

    const childOffsets = node.children.map(walk);

    const resultOffset = results.length;
    const resultCount = node.results?.length ?? 0;

    if (node.results) {
      for (const result of node.results) {
        const nameOffset = intern(result.name);
        results.push({
          type: getTypeNumber(result.type),
          id: result.id,
          nameOffset,
          nameLength: result.name.length,
          relevance: result.relevance,
        });
      }
    }

    const serialized: SerializedNode = {
      keyOffset,
      keyLength,
      childCount: childOffsets.length,
      firstChildOffset: childOffsets.length > 0 ? nodes.length + 1 : 0,
      resultCount,
      firstResultOffset: resultCount > 0 ? resultOffset : 0,
    };

    const currentOffset = nodes.length;
    nodes.push(serialized);

    return currentOffset;
  }

  walk(root);

  return {
    stringPool: stringPool.join(""),
    stringOffsets,
    nodes,
    results,
  };
}

// Legacy function for backward compatibility
export function buildRadixTree(entries: [string, number][]): TrieNode {
  const root: TrieNode = { key: "", children: [] };
  for (const [name, id] of entries) {
    const result: SearchResult = {
      type: "track",
      id,
      name,
      relevance: 100,
    };
    insert(root, name.toLowerCase(), result);
  }
  return root;
}

// Query functions for the trie
export function searchTrie(
  root: TrieNode,
  query: string,
  maxResults: number = 20
): SearchResult[] {
  const normalizedQuery = normalizeString(query);
  const results: SearchResult[] = [];

  function findNode(node: TrieNode, remainingQuery: string): TrieNode | null {
    if (remainingQuery.length === 0) return node;

    for (const child of node.children) {
      const commonPrefix = getCommonPrefix(remainingQuery, child.key);
      if (commonPrefix.length > 0) {
        if (commonPrefix.length === child.key.length) {
          // Full match of child key, continue with remaining query
          return findNode(child, remainingQuery.slice(commonPrefix.length));
        } else if (commonPrefix.length === remainingQuery.length) {
          // Query is a prefix of child key
          return child;
        }
      }
    }
    return null;
  }

  function collectResults(node: TrieNode) {
    if (node.results) {
      results.push(...node.results);
    }
    for (const child of node.children) {
      collectResults(child);
    }
  }

  const matchedNode = findNode(root, normalizedQuery);
  if (matchedNode) {
    collectResults(matchedNode);
  }

  // Sort by relevance and limit results
  results.sort((a, b) => b.relevance - a.relevance);
  return results.slice(0, maxResults);
}

// Utility functions for working with serialized trie (for STM32)
export function getSerializedTrieStats(serialized: SerializedTrie): {
  stringPoolSize: number;
  nodeCount: number;
  resultCount: number;
  totalSize: number;
} {
  const stringPoolSize = serialized.stringPool.length;
  const nodeCount = serialized.nodes.length;
  const resultCount = serialized.results.length;

  // Calculate approximate memory usage
  const stringPoolBytes = stringPoolSize;
  const stringOffsetsBytes = serialized.stringOffsets.length * 4; // 4 bytes per offset
  const nodesBytes = nodeCount * (4 * 6); // 6 fields * 4 bytes each
  const resultsBytes = resultCount * (4 * 5); // 5 fields * 4 bytes each

  const totalSize =
    stringPoolBytes + stringOffsetsBytes + nodesBytes + resultsBytes;

  return {
    stringPoolSize,
    nodeCount,
    resultCount,
    totalSize,
  };
}

// Export serialized trie to binary format for STM32
export function exportToBinary(serialized: SerializedTrie): Uint8Array {
  const stats = getSerializedTrieStats(serialized);

  // Calculate total size needed
  const headerSize = 16; // 4 uint32 values for counts
  const totalSize = headerSize + stats.totalSize;

  const buffer = new ArrayBuffer(totalSize);
  const view = new DataView(buffer);
  let offset = 0;

  // Write header
  view.setUint32(offset, serialized.stringPool.length, true);
  offset += 4;
  view.setUint32(offset, serialized.nodes.length, true);
  offset += 4;
  view.setUint32(offset, serialized.results.length, true);
  offset += 4;
  view.setUint32(offset, serialized.stringOffsets.length, true);
  offset += 4;

  // Write string pool
  const encoder = new TextEncoder();
  const stringBytes = encoder.encode(serialized.stringPool);
  new Uint8Array(buffer, offset, stringBytes.length).set(stringBytes);
  offset += stringBytes.length;

  // Align to 4-byte boundary
  while (offset % 4 !== 0) {
    view.setUint8(offset++, 0);
  }

  // Write string offsets
  for (const stringOffset of serialized.stringOffsets) {
    view.setUint32(offset, stringOffset, true);
    offset += 4;
  }

  // Write nodes
  for (const node of serialized.nodes) {
    view.setUint32(offset, node.keyOffset, true);
    offset += 4;
    view.setUint32(offset, node.keyLength, true);
    offset += 4;
    view.setUint32(offset, node.childCount, true);
    offset += 4;
    view.setUint32(offset, node.firstChildOffset, true);
    offset += 4;
    view.setUint32(offset, node.resultCount, true);
    offset += 4;
    view.setUint32(offset, node.firstResultOffset, true);
    offset += 4;
  }

  // Write results
  for (const result of serialized.results) {
    view.setUint32(offset, result.type, true);
    offset += 4;
    view.setUint32(offset, result.id, true);
    offset += 4;
    view.setUint32(offset, result.nameOffset, true);
    offset += 4;
    view.setUint32(offset, result.nameLength, true);
    offset += 4;
    view.setUint32(offset, result.relevance, true);
    offset += 4;
  }

  return new Uint8Array(buffer);
}

// Generate C header file for STM32
export function generateCHeader(
  serialized: SerializedTrie,
  variableName: string = "music_index"
): string {
  const binaryData = exportToBinary(serialized);
  const stats = getSerializedTrieStats(serialized);

  let header = `// Auto-generated music index for STM32\n`;
  header += `// Generated on ${new Date().toISOString()}\n\n`;
  header += `#ifndef MUSIC_INDEX_H\n`;
  header += `#define MUSIC_INDEX_H\n\n`;
  header += `#include <stdint.h>\n\n`;

  // Add statistics as comments
  header += `// Index Statistics:\n`;
  header += `// - String pool size: ${stats.stringPoolSize} bytes\n`;
  header += `// - Node count: ${stats.nodeCount}\n`;
  header += `// - Result count: ${stats.resultCount}\n`;
  header += `// - Total size: ${stats.totalSize} bytes\n\n`;

  // Add data structures
  header += `typedef struct {\n`;
  header += `    uint32_t key_offset;\n`;
  header += `    uint32_t key_length;\n`;
  header += `    uint32_t child_count;\n`;
  header += `    uint32_t first_child_offset;\n`;
  header += `    uint32_t result_count;\n`;
  header += `    uint32_t first_result_offset;\n`;
  header += `} trie_node_t;\n\n`;

  header += `typedef struct {\n`;
  header += `    uint32_t type;  // 0=track, 1=artist, 2=album, 3=genre\n`;
  header += `    uint32_t id;\n`;
  header += `    uint32_t name_offset;\n`;
  header += `    uint32_t name_length;\n`;
  header += `    uint32_t relevance;\n`;
  header += `} search_result_t;\n\n`;

  // Add binary data
  header += `extern const uint8_t ${variableName}_data[${binaryData.length}];\n`;
  header += `extern const uint32_t ${variableName}_size;\n\n`;

  header += `#endif // MUSIC_INDEX_H\n`;

  return header;
}

// Demo function to show usage
export function createMusicIndexDemo(crawlIndex: CrawlIndex): {
  trie: TrieNode;
  serialized: SerializedTrie;
  stats: ReturnType<typeof getSerializedTrieStats>;
  binaryData: Uint8Array;
  cHeader: string;
} {
  console.log("Building trie from crawl index...");
  const trie = buildTrieFromCrawlIndex(crawlIndex, {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 2,
    maxResults: 20,
  });

  console.log("Serializing trie...");
  const serialized = serializeTrie(trie);

  console.log("Calculating stats...");
  const stats = getSerializedTrieStats(serialized);

  console.log("Exporting to binary...");
  const binaryData = exportToBinary(serialized);

  console.log("Generating C header...");
  const cHeader = generateCHeader(serialized);

  console.log(`Trie built successfully!`);
  console.log(`- String pool: ${stats.stringPoolSize} bytes`);
  console.log(`- Nodes: ${stats.nodeCount}`);
  console.log(`- Results: ${stats.resultCount}`);
  console.log(`- Total size: ${stats.totalSize} bytes`);
  console.log(`- Binary size: ${binaryData.length} bytes`);

  return {
    trie,
    serialized,
    stats,
    binaryData,
    cHeader,
  };
}
