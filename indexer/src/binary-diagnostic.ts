import fs from "fs/promises";
import { readMetadata } from "./crawler";
import {
  buildTrieFromCrawlIndex,
  serializeTrie,
  exportToBinary,
  type TrieNode,
} from "./trie";

// Structures matching the STM32 code exactly
type TrieHeader = {
  string_pool_size: number;
  node_count: number;
  result_count: number;
  string_offset_count: number;
};

type TrieNodeBinary = {
  key_offset: number;
  key_length: number;
  child_count: number;
  first_child_offset: number;
  result_count: number;
  first_result_offset: number;
};

type SearchResultBinary = {
  type: number;
  id: number;
  name_offset: number;
  name_length: number;
  relevance: number;
};

async function analyzeBinaryFile(filePath: string) {
  console.log("=== BINARY FILE ANALYSIS ===\n");

  const data = await fs.readFile(filePath);
  console.log(`File size: ${data.length} bytes`);

  let offset = 0;

  // Read header
  const header: TrieHeader = {
    string_pool_size: data.readUInt32LE(offset),
    node_count: data.readUInt32LE(offset + 4),
    result_count: data.readUInt32LE(offset + 8),
    string_offset_count: data.readUInt32LE(offset + 12),
  };
  offset += 16;

  console.log("📋 HEADER:");
  console.log(`  String pool size: ${header.string_pool_size} bytes`);
  console.log(`  Node count: ${header.node_count}`);
  console.log(`  Result count: ${header.result_count}`);
  console.log(`  String offset count: ${header.string_offset_count}`);
  console.log();

  // Calculate expected offsets
  const string_pool_padded = (header.string_pool_size + 3) & ~3;
  const string_offsets_offset = offset + string_pool_padded;
  const nodes_offset = string_offsets_offset + header.string_offset_count * 4;
  const results_offset = nodes_offset + header.node_count * 24; // 6 * 4 bytes per node

  console.log("📍 CALCULATED OFFSETS:");
  console.log(
    `  String pool: ${offset} (size: ${header.string_pool_size}, padded: ${string_pool_padded})`
  );
  console.log(
    `  String offsets: ${string_offsets_offset} (${
      header.string_offset_count
    } * 4 = ${header.string_offset_count * 4} bytes)`
  );
  console.log(
    `  Nodes: ${nodes_offset} (${header.node_count} * 24 = ${
      header.node_count * 24
    } bytes)`
  );
  console.log(
    `  Results: ${results_offset} (${header.result_count} * 20 = ${
      header.result_count * 20
    } bytes)`
  );
  console.log(
    `  Expected total: ${results_offset + header.result_count * 20} bytes`
  );
  console.log();

  // Read string pool (first 200 chars for inspection)
  const string_pool_sample = data
    .slice(offset, offset + Math.min(200, header.string_pool_size))
    .toString("utf8");
  console.log("📝 STRING POOL SAMPLE (first 200 chars):");
  console.log(`"${string_pool_sample.replace(/\0/g, "\\0")}"`);
  console.log();

  // Read first few string offsets
  console.log("🔢 FIRST 10 STRING OFFSETS:");
  for (let i = 0; i < Math.min(10, header.string_offset_count); i++) {
    const str_offset = data.readUInt32LE(string_offsets_offset + i * 4);
    console.log(`  [${i}]: ${str_offset}`);
  }
  console.log();

  // Read first few nodes
  console.log("🌳 FIRST 5 NODES:");
  for (let i = 0; i < Math.min(5, header.node_count); i++) {
    const node_offset = nodes_offset + i * 24;
    const node: TrieNodeBinary = {
      key_offset: data.readUInt32LE(node_offset),
      key_length: data.readUInt32LE(node_offset + 4),
      child_count: data.readUInt32LE(node_offset + 8),
      first_child_offset: data.readUInt32LE(node_offset + 12),
      result_count: data.readUInt32LE(node_offset + 16),
      first_result_offset: data.readUInt32LE(node_offset + 20),
    };

    // Read the key string
    let key_string = "";
    if (node.key_length > 0 && node.key_offset < header.string_pool_size) {
      const key_data = data.slice(
        offset + node.key_offset,
        offset + node.key_offset + node.key_length
      );
      key_string = key_data.toString("utf8");
    }

    console.log(`  Node ${i}:`);
    console.log(
      `    Key: "${key_string}" (offset: ${node.key_offset}, length: ${node.key_length})`
    );
    console.log(
      `    Children: ${node.child_count} (first at: ${node.first_child_offset})`
    );
    console.log(
      `    Results: ${node.result_count} (first at: ${node.first_result_offset})`
    );

    // Check for suspicious values
    const issues = [];
    if (node.child_count > 1000)
      issues.push(`🚨 EXCESSIVE CHILDREN: ${node.child_count}`);
    if (node.first_child_offset >= header.node_count && node.child_count > 0)
      issues.push(`🚨 INVALID CHILD OFFSET: ${node.first_child_offset}`);
    if (node.result_count > 100)
      issues.push(`⚠️ MANY RESULTS: ${node.result_count}`);
    if (node.key_offset >= header.string_pool_size && node.key_length > 0)
      issues.push(`🚨 INVALID KEY OFFSET: ${node.key_offset}`);

    if (issues.length > 0) {
      console.log(`    ❌ ISSUES: ${issues.join(", ")}`);
    }
    console.log();
  }

  // Find nodes with most children
  console.log("🔍 NODES WITH MOST CHILDREN:");
  const high_child_nodes = [];

  for (let i = 0; i < header.node_count; i++) {
    const node_offset = nodes_offset + i * 24;
    const child_count = data.readUInt32LE(node_offset + 8);

    if (child_count > 10) {
      const key_offset = data.readUInt32LE(node_offset);
      const key_length = data.readUInt32LE(node_offset + 4);

      let key_string = "";
      if (key_length > 0 && key_offset < header.string_pool_size) {
        const key_data = data.slice(
          offset + key_offset,
          offset + key_offset + key_length
        );
        key_string = key_data.toString("utf8");
      }

      high_child_nodes.push({
        index: i,
        key: key_string,
        child_count,
        key_offset,
        key_length,
      });
    }
  }

  high_child_nodes.sort((a, b) => b.child_count - a.child_count);

  for (const node of high_child_nodes.slice(0, 10)) {
    console.log(
      `  Node ${node.index}: "${node.key}" has ${node.child_count} children`
    );
  }

  if (high_child_nodes.length === 0) {
    console.log("  ✅ No nodes with excessive children found");
  }
  console.log();

  // Read first few results
  console.log("🎯 FIRST 5 RESULTS:");
  for (let i = 0; i < Math.min(5, header.result_count); i++) {
    const result_offset = results_offset + i * 20;
    const result: SearchResultBinary = {
      type: data.readUInt32LE(result_offset),
      id: data.readUInt32LE(result_offset + 4),
      name_offset: data.readUInt32LE(result_offset + 8),
      name_length: data.readUInt32LE(result_offset + 12),
      relevance: data.readUInt32LE(result_offset + 16),
    };

    // Read the name string
    let name_string = "";
    if (
      result.name_length > 0 &&
      result.name_offset < header.string_pool_size
    ) {
      const name_data = data.slice(
        offset + result.name_offset,
        offset + result.name_offset + result.name_length
      );
      name_string = name_data.toString("utf8");
    }

    const type_names = ["Track", "Artist", "Album", "Genre"];
    const type_name = type_names[result.type] || "Unknown";

    console.log(
      `  Result ${i}: [${type_name}] "${name_string}" (ID: ${result.id}, Relevance: ${result.relevance})`
    );
  }
  console.log();

  return {
    header,
    high_child_nodes,
    file_size: data.length,
    calculated_size: results_offset + header.result_count * 20,
  };
}

async function compareWithGenerated(musicPath: string, binaryPath: string) {
  console.log("=== COMPARING GENERATED VS BINARY ===\n");

  // Generate fresh data
  console.log("🔄 Generating fresh trie data...");
  const crawlIndex = await readMetadata(musicPath);
  const trie = buildTrieFromCrawlIndex(crawlIndex, {
    includePartialMatches: true,
    caseSensitive: false,
    minPrefixLength: 2,
    maxResults: 20,
  });

  const serialized = serializeTrie(trie);
  const binaryData = exportToBinary(serialized);

  console.log("📊 GENERATED DATA:");
  console.log(`  Nodes: ${serialized.nodes.length}`);
  console.log(`  Results: ${serialized.results.length}`);
  console.log(`  String pool: ${serialized.stringPool.length} bytes`);
  console.log(`  Binary size: ${binaryData.length} bytes`);
  console.log();

  // Analyze existing binary file
  const analysis = await analyzeBinaryFile(binaryPath);

  console.log("🔍 COMPARISON:");
  console.log(
    `  Generated nodes: ${serialized.nodes.length} vs Binary nodes: ${analysis.header.node_count}`
  );
  console.log(
    `  Generated results: ${serialized.results.length} vs Binary results: ${analysis.header.result_count}`
  );
  console.log(
    `  Generated string pool: ${serialized.stringPool.length} vs Binary string pool: ${analysis.header.string_pool_size}`
  );
  console.log(
    `  Generated binary size: ${binaryData.length} vs Binary file size: ${analysis.file_size}`
  );
  console.log();

  // Check for discrepancies
  const issues = [];
  if (serialized.nodes.length !== analysis.header.node_count) {
    issues.push(
      `Node count mismatch: ${serialized.nodes.length} vs ${analysis.header.node_count}`
    );
  }
  if (serialized.results.length !== analysis.header.result_count) {
    issues.push(
      `Result count mismatch: ${serialized.results.length} vs ${analysis.header.result_count}`
    );
  }
  if (serialized.stringPool.length !== analysis.header.string_pool_size) {
    issues.push(
      `String pool size mismatch: ${serialized.stringPool.length} vs ${analysis.header.string_pool_size}`
    );
  }
  if (binaryData.length !== analysis.file_size) {
    issues.push(
      `Binary size mismatch: ${binaryData.length} vs ${analysis.file_size}`
    );
  }

  if (issues.length > 0) {
    console.log("❌ ISSUES FOUND:");
    issues.forEach((issue) => console.log(`  - ${issue}`));
  } else {
    console.log("✅ No discrepancies found in basic metrics");
  }
  console.log();

  // Analyze trie structure for potential issues
  console.log("🌳 TRIE STRUCTURE ANALYSIS:");
  analyzeTrieStructure(trie, "", 0, 0);
}

function analyzeTrieStructure(
  node: TrieNode,
  path: string,
  depth: number,
  nodeIndex: number
) {
  if (depth > 10) return; // Prevent deep recursion

  if (node.children.length > 50) {
    console.log(
      `  ⚠️ Node at "${path}" has ${node.children.length} children (index: ${nodeIndex})`
    );

    // Show first 10 children
    console.log(
      `    First 10 children: ${node.children
        .slice(0, 10)
        .map((c) => `"${c.key}"`)
        .join(", ")}`
    );
  }

  // Recursively check children
  for (let i = 0; i < node.children.length; i++) {
    const child = node.children[i];
    const childPath = path + (path ? "/" : "") + child.key;
    analyzeTrieStructure(child, childPath, depth + 1, nodeIndex + i + 1);
  }
}

async function main() {
  const musicPath = process.argv[2] || "~/Music/clean";
  const binaryPath = process.argv[3] || "./music_index.bin";

  console.log("=== BINARY DIAGNOSTIC TOOL ===\n");
  console.log(`Music path: ${musicPath}`);
  console.log(`Binary path: ${binaryPath}`);
  console.log();

  try {
    // Check if binary file exists
    await fs.access(binaryPath);

    // First analyze the existing binary file
    await analyzeBinaryFile(binaryPath);

    // Then compare with freshly generated data
    await compareWithGenerated(musicPath, binaryPath);
  } catch (error: any) {
    console.error("Error:", error);

    if (error.code === "ENOENT") {
      console.log("\n💡 Binary file not found. Generating fresh analysis...");

      // Generate and analyze fresh data
      const crawlIndex = await readMetadata(musicPath);
      const trie = buildTrieFromCrawlIndex(crawlIndex);
      const serialized = serializeTrie(trie);
      const binaryData = exportToBinary(serialized);

      // Save for analysis
      await fs.writeFile(binaryPath, binaryData);
      console.log(`Generated ${binaryPath} for analysis`);

      // Analyze the generated file
      await analyzeBinaryFile(binaryPath);
    }
  }
}

if (require.main === module) {
  main().catch(console.error);
}

export { analyzeBinaryFile, compareWithGenerated };
