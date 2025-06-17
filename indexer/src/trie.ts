import type { TrieNodeStats } from "./stats";
import type { CrawlIndex } from "./types";
import type {
  TrieNode,
  SearchResult,
  SerializedTrie,
  SerializedNode,
  SerializedResult,
  TrieConfig,
} from "./types";

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

  // Remove accents and convert to ASCII-safe characters
  normalized = removeAccents(normalized);

  // Remove common articles and prepositions for better matching
  normalized = normalized.replace(/^(the|a|an|le|la|les|un|une|des)\s+/i, "");

  // Ensure only ASCII characters remain
  normalized = normalized.replace(/[^\x00-\x7F]/g, "");

  return normalized;
}

// Function to remove accents and convert to ASCII equivalents
function removeAccents(str: string): string {
  const accentMap: { [key: string]: string } = {
    à: "a",
    á: "a",
    â: "a",
    ã: "a",
    ä: "a",
    å: "a",
    æ: "ae",
    ç: "c",
    è: "e",
    é: "e",
    ê: "e",
    ë: "e",
    ì: "i",
    í: "i",
    î: "i",
    ï: "i",
    ñ: "n",
    ò: "o",
    ó: "o",
    ô: "o",
    õ: "o",
    ö: "o",
    ø: "o",
    œ: "oe",
    ù: "u",
    ú: "u",
    û: "u",
    ü: "u",
    ý: "y",
    ÿ: "y",
    À: "A",
    Á: "A",
    Â: "A",
    Ã: "A",
    Ä: "A",
    Å: "A",
    Æ: "AE",
    Ç: "C",
    È: "E",
    É: "E",
    Ê: "E",
    Ë: "E",
    Ì: "I",
    Í: "I",
    Î: "I",
    Ï: "I",
    Ñ: "N",
    Ò: "O",
    Ó: "O",
    Ô: "O",
    Õ: "O",
    Ö: "O",
    Ø: "O",
    Œ: "OE",
    Ù: "U",
    Ú: "U",
    Û: "U",
    Ü: "U",
    Ý: "Y",
    Ÿ: "Y",
  };

  return str.replace(
    /[àáâãäåæçèéêëìíîïñòóôõöøœùúûüýÿÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØŒÙÚÛÜÝŸ]/g,
    (match) => accentMap[match] || match
  );
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

    // Also sanitize the original name to ensure ASCII-only characters in the binary
    const sanitizedName = removeAccents(name)
      .replace(/[^\x00-\x7F]/g, "")
      .toLocaleLowerCase();

    const result: SearchResult = {
      type,
      id,
      name: sanitizedName,
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

  // Map from TrieNode to its index in the nodes array
  const nodeIndexMap = new Map<TrieNode, number>();

  function intern(str: string): number {
    if (stringTable.has(str)) return stringTable.get(str)!;
    const offset = stringPool.join("").length;
    stringPool.push(str);
    stringOffsets.push(offset);
    stringTable.set(str, offset);
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

  // First pass: assign indices to all nodes in breadth-first order
  function assignIndices() {
    const queue: TrieNode[] = [root];
    let nodeIndex = 0;

    while (queue.length > 0) {
      const node = queue.shift()!;
      nodeIndexMap.set(node, nodeIndex++);

      // Add children to queue
      for (const child of node.children) {
        queue.push(child);
      }
    }
  }

  // Second pass: serialize nodes in the assigned order
  function serializeNodes() {
    const queue: TrieNode[] = [root];

    while (queue.length > 0) {
      const node = queue.shift()!;

      const keyOffset = intern(node.key);
      const keyLength = node.key.length;

      const resultOffset = results.length;
      const resultCount = node.results?.length ?? 0;

      // Add results for this node
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

      // Calculate child information
      const childCount = node.children.length;
      const firstChildOffset =
        childCount > 0 ? nodeIndexMap.get(node.children[0])! : 0;

      const serialized: SerializedNode = {
        keyOffset,
        keyLength,
        childCount,
        firstChildOffset,
        resultCount,
        firstResultOffset: resultCount > 0 ? resultOffset : 0,
      };

      nodes.push(serialized);

      // Add children to queue for processing
      for (const child of node.children) {
        queue.push(child);
      }
    }
  }

  // Execute the two-pass serialization
  assignIndices();
  serializeNodes();

  return {
    stringPool: stringPool.join(""),
    stringOffsets,
    nodes,
    results,
  };
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
export function getSerializedTrieStats(serialized: SerializedTrie) {
  const stringPoolSize = serialized.stringPool.length;
  const nodeCount = serialized.nodes.length;
  const resultCount = serialized.results.length;

  // Calculate exact memory usage including alignment
  // Use TextEncoder to get the actual byte length of the string pool
  const encoder = new TextEncoder();
  const stringPoolBytes = encoder.encode(serialized.stringPool).length;
  // Add padding for 4-byte alignment after string pool
  const stringPoolPadding = 0; //(4 - (stringPoolBytes % 4)) % 4;
  const stringOffsetsBytes = serialized.stringOffsets.length * 4; // 4 bytes per offset
  const nodesBytes = nodeCount * (4 * 6); // 6 fields * 4 bytes each
  const resultsBytes = resultCount * (4 * 5); // 5 fields * 4 bytes each

  const totalSize =
    stringPoolBytes +
    stringPoolPadding +
    stringOffsetsBytes +
    nodesBytes +
    resultsBytes;

  return {
    stringPoolSize,
    stringOffsetCount: serialized.stringOffsets.length,
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
  view.setUint32(offset, stats.stringPoolSize, true);
  offset += 4;
  view.setUint32(offset, stats.nodeCount, true);
  offset += 4;
  view.setUint32(offset, stats.resultCount, true);
  offset += 4;
  view.setUint32(offset, stats.stringOffsetCount, true);
  offset += 4;
  // Write string pool
  const encoder = new TextEncoder();
  const stringBytes = encoder.encode(serialized.stringPool);

  // const stringPool = new TextDecoder().decode(stringBytes);
  // console.log("test", stringPool === serialized.stringPool);
  // console.log("test2", stringBytes.length, serialized.stringPool.length);

  new Uint8Array(buffer, offset, stats.stringPoolSize).set(stringBytes);
  offset += stats.stringPoolSize;

  // Write string offsets
  for (let i = 0; i < serialized.stringOffsets.length; i++) {
    view.setUint32(offset, serialized.stringOffsets[i], true);
    offset += 4;
  }

  // Write nodes
  for (let i = 0; i < serialized.nodes.length; i++) {
    const node = serialized.nodes[i];

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
  for (let i = 0; i < serialized.results.length; i++) {
    const result = serialized.results[i];

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

export function importFromBinary(buffer: Uint8Array): SerializedTrie {
  // read header:
  const view = new DataView(
    buffer.buffer,
    buffer.byteOffset,
    buffer.byteLength
  );
  let offset = 0;
  const stringPoolSize = view.getUint32(offset, true);
  offset += 4;
  const nodeCount = view.getUint32(offset, true);
  offset += 4;
  const resultCount = view.getUint32(offset, true);
  offset += 4;
  const stringOffsetCount = view.getUint32(offset, true);
  offset += 4;

  // read string pool
  const stringPoolBytes = buffer.slice(offset, offset + stringPoolSize);
  offset += stringPoolSize;
  const stringPool = new TextDecoder().decode(stringPoolBytes);
  // read string offsets
  const stringOffsets: number[] = [];
  for (let i = 0; i < stringOffsetCount; i++) {
    const stringOffset = view.getUint32(offset, true);
    stringOffsets.push(stringOffset);
    offset += 4;
  }
  // read nodes
  const nodes: SerializedNode[] = [];
  for (let i = 0; i < nodeCount; i++) {
    const keyOffset = view.getUint32(offset, true);
    offset += 4;
    const keyLength = view.getUint32(offset, true);
    offset += 4;
    const childCount = view.getUint32(offset, true);
    offset += 4;
    const firstChildOffset = view.getUint32(offset, true);
    offset += 4;
    const resultCount = view.getUint32(offset, true);
    offset += 4;
    const firstResultOffset = view.getUint32(offset, true);
    offset += 4;

    nodes.push({
      keyOffset,
      keyLength,
      childCount,
      firstChildOffset,
      resultCount,
      firstResultOffset,
    });
  }
  console.log(`  Read ${nodes.length} nodes`);
  // read results
  const results: SerializedResult[] = [];
  for (let i = 0; i < resultCount; i++) {
    const type = view.getUint32(offset, true);
    offset += 4;
    const id = view.getUint32(offset, true);
    offset += 4;
    const nameOffset = view.getUint32(offset, true);
    offset += 4;
    const nameLength = view.getUint32(offset, true);
    offset += 4;
    const relevance = view.getUint32(offset, true);
    offset += 4;

    results.push({
      type,
      id,
      nameOffset,
      nameLength,
      relevance,
    });
  }
  console.log(`  Read ${results.length} results`);
  // Reconstruct the trie from the serialized data
  const root: TrieNode = { key: "", children: [] };
  const nodeMap = new Map<number, TrieNode>();
  nodeMap.set(0, root); // Root node at index 0
  for (let i = 0; i < nodes.length; i++) {
    const nodeData = nodes[i];
    const key = stringPool.slice(
      stringOffsets[nodeData.keyOffset],
      stringOffsets[nodeData.keyOffset] + nodeData.keyLength
    );
    const node: TrieNode = {
      key,
      children: [],
      results: [],
    };
    nodeMap.set(i, node);

    const indexToType: {
      [key: number]: "track" | "artist" | "album" | "genre";
    } = {
      0: "track",
      1: "artist",
      2: "album",
      3: "genre",
    };
    // Add results to the node
    if (nodeData.resultCount > 0) {
      for (let j = 0; j < nodeData.resultCount; j++) {
        const resultIndex = nodeData.firstResultOffset + j;
        const resultData = results[resultIndex];
        const name = stringPool.slice(
          stringOffsets[resultData.nameOffset],
          stringOffsets[resultData.nameOffset] + resultData.nameLength
        );
        const result: SearchResult = {
          type: indexToType[resultData.type],
          id: resultData.id,
          name,
          relevance: resultData.relevance,
        };
        node.results!.push(result);
      }
    }

    // Add children
    if (nodeData.childCount > 0) {
      for (let j = 0; j < nodeData.childCount; j++) {
        const childIndex = nodeData.firstChildOffset + j;
        const childNode = nodeMap.get(childIndex);
        if (childNode) {
          node.children.push(childNode);
        }
      }
    }
  }
  // Reconstruct the root node's children
  for (let i = 1; i < nodes.length; i++) {
    const nodeData = nodes[i];
    const parentNode = nodeMap.get(nodeData.firstChildOffset);
    if (parentNode) {
      const childNode = nodeMap.get(i);
      if (childNode) {
        parentNode.children.push(childNode);
      }
    }
  }
  console.log(`Reconstructed trie with ${nodeMap.size} nodes`);
  return {
    stringPool: stringPool,
    stringOffsets,
    nodes,
    results,
  };
}

export function findNodesWithPattern(
  root: TrieNode,
  pattern: RegExp
): TrieNodeStats[] {
  const matches: TrieNodeStats[] = [];

  function traverse(node: TrieNode, depth: number, path: string) {
    const fullPath = path + (path ? "/" : "") + node.key;

    if (pattern.test(node.key) || pattern.test(fullPath)) {
      matches.push({
        key: node.key,
        depth,
        childCount: node.children.length,
        resultCount: node.results?.length || 0,
        path: fullPath,
        children: node.children.map((child) => child.key),
        hasResults: !!node.results,
      });
    }

    for (const child of node.children) {
      traverse(child, depth + 1, fullPath);
    }
  }

  traverse(root, 0, "");
  return matches;
}
