import type { SerializedPathIndex, SerializedTrie, TrieNode } from "./types";

// Diagnostic utilities for analyzing trie structure
export type TrieNodeStats = {
  key: string;
  depth: number;
  childCount: number;
  resultCount: number;
  path: string; // Full path from root to this node
  children: string[]; // Keys of child nodes
  hasResults: boolean;
};

export type TrieAnalysis = {
  totalNodes: number;
  maxDepth: number;
  maxChildCount: number;
  averageChildCount: number;
  nodesWithManyChildren: TrieNodeStats[]; // Nodes with > threshold children
  depthDistribution: Map<number, number>; // depth -> count of nodes at that depth
  childCountDistribution: Map<number, number>; // childCount -> count of nodes with that many children
  problematicNodes: TrieNodeStats[]; // Nodes that might cause performance issues
};

export function analyzeTrieStructure(
  root: TrieNode,
  childCountThreshold: number = 100
): TrieAnalysis {
  const nodeStats: TrieNodeStats[] = [];
  const depthDistribution = new Map<number, number>();
  const childCountDistribution = new Map<number, number>();

  let maxDepth = 0;
  let maxChildCount = 0;
  let totalChildCount = 0;

  function traverse(node: TrieNode, depth: number, path: string) {
    const childCount = node.children.length;
    const resultCount = node.results?.length || 0;
    const fullPath = path + (path ? "/" : "") + node.key;

    const stats: TrieNodeStats = {
      key: node.key,
      depth,
      childCount,
      resultCount,
      path: fullPath,
      children: node.children.map((child) => child.key),
      hasResults: !!node.results,
    };

    nodeStats.push(stats);

    // Update statistics
    maxDepth = Math.max(maxDepth, depth);
    maxChildCount = Math.max(maxChildCount, childCount);
    totalChildCount += childCount;

    // Update distributions
    depthDistribution.set(depth, (depthDistribution.get(depth) || 0) + 1);
    childCountDistribution.set(
      childCount,
      (childCountDistribution.get(childCount) || 0) + 1
    );

    // Recursively analyze children
    for (const child of node.children) {
      traverse(child, depth + 1, fullPath);
    }
  }

  traverse(root, 0, "");

  const averageChildCount =
    nodeStats.length > 0 ? totalChildCount / nodeStats.length : 0;

  // Find nodes with many children
  const nodesWithManyChildren = nodeStats
    .filter((stats) => stats.childCount >= childCountThreshold)
    .sort((a, b) => b.childCount - a.childCount);

  // Find potentially problematic nodes (high child count or deep nesting)
  const problematicNodes = nodeStats
    .filter(
      (stats) =>
        stats.childCount >= childCountThreshold ||
        stats.depth > 10 ||
        (stats.childCount > 50 && stats.depth > 5)
    )
    .sort((a, b) => b.childCount - a.childCount);

  return {
    totalNodes: nodeStats.length,
    maxDepth,
    maxChildCount,
    averageChildCount,
    nodesWithManyChildren,
    depthDistribution,
    childCountDistribution,
    problematicNodes,
  };
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

export function printTrieAnalysis(analysis: TrieAnalysis): void {
  console.log("=== TRIE STRUCTURE ANALYSIS ===\n");

  console.log("📊 General Statistics:");
  console.log(`  Total nodes: ${analysis.totalNodes}`);
  console.log(`  Maximum depth: ${analysis.maxDepth}`);
  console.log(`  Maximum children per node: ${analysis.maxChildCount}`);
  console.log(
    `  Average children per node: ${analysis.averageChildCount.toFixed(2)}\n`
  );

  console.log("📈 Depth Distribution:");
  const sortedDepths = Array.from(analysis.depthDistribution.entries()).sort(
    (a, b) => a[0] - b[0]
  );
  for (const [depth, count] of sortedDepths) {
    const bar = "█".repeat(
      Math.min(50, Math.floor(count / Math.max(1, analysis.totalNodes / 50)))
    );
    console.log(
      `  Depth ${depth.toString().padStart(2)}: ${count
        .toString()
        .padStart(4)} nodes ${bar}`
    );
  }
  console.log();

  console.log("👥 Child Count Distribution (top 10):");
  const sortedChildCounts = Array.from(
    analysis.childCountDistribution.entries()
  )
    .sort((a, b) => b[1] - a[1])
    .slice(0, 10);
  for (const [childCount, nodeCount] of sortedChildCounts) {
    console.log(
      `  ${childCount.toString().padStart(3)} children: ${nodeCount
        .toString()
        .padStart(4)} nodes`
    );
  }
  console.log();

  if (analysis.nodesWithManyChildren.length > 0) {
    console.log("⚠️  NODES WITH MANY CHILDREN (potential performance issues):");
    for (const node of analysis.nodesWithManyChildren.slice(0, 10)) {
      console.log(
        `  🔴 ${node.childCount} children at "${node.path}" (depth ${node.depth})`
      );
      console.log(`     Key: "${node.key}"`);
      console.log(`     Results: ${node.resultCount}`);
      if (node.children.length <= 20) {
        console.log(
          `     Children: [${node.children.slice(0, 10).join(", ")}${
            node.children.length > 10 ? "..." : ""
          }]`
        );
      } else {
        console.log(
          `     First 10 children: [${node.children
            .slice(0, 10)
            .join(", ")}...]`
        );
      }
      console.log();
    }
  } else {
    console.log("✅ No nodes with excessive children found.\n");
  }

  if (analysis.problematicNodes.length > 0) {
    console.log("🚨 POTENTIALLY PROBLEMATIC NODES:");
    for (const node of analysis.problematicNodes.slice(0, 5)) {
      const issues = [];
      if (node.childCount >= 100) issues.push(`${node.childCount} children`);
      if (node.depth > 10) issues.push(`depth ${node.depth}`);
      if (node.childCount > 50 && node.depth > 5)
        issues.push("deep + many children");

      console.log(`  ⚠️  "${node.path}" - ${issues.join(", ")}`);
    }
    console.log();
  }
}

export function getSerializedPathIndexStats(serialized: SerializedPathIndex) {
  const encoder = new TextEncoder();
  const pathDataBytes = encoder.encode(serialized.pathData).length;
  const trackIdsBytes = serialized.trackIds.length * 4;
  const pathOffsetsBytes = serialized.pathOffsets.length * 4;
  const headerBytes = 8;

  const totalSize =
    headerBytes + trackIdsBytes + pathOffsetsBytes + pathDataBytes;

  return {
    trackCount: serialized.trackCount,
    pathDataSize: pathDataBytes,
    trackIdsSize: trackIdsBytes,
    pathOffsetsSize: pathOffsetsBytes,
    totalSize,
  };
}
