import { describe, it, expect, afterAll, beforeAll } from "vitest";
import fs from "fs/promises";
import { parseFile } from "music-metadata";

import path from "path";
import {
  RadixTreeBuilder,
  parse,
  ParsedRadixTree,
  ParsedRadixNode,
  crawl,
  readMetadata,
} from "../src/main"; // adapte le chemin si besoin

const TMP_FILE = path.join(__dirname, "test.radixidx");

describe("Radix tree builder and parser", () => {
  const entries: [string, number][] = [
    ["hello", 0],
    ["help", 1],
    ["helium", 2],
    ["world", 3],
    ["word", 4],
    ["wolf", 5],
  ];

  beforeAll(async () => {
    const tree = new RadixTreeBuilder();
    for (const [word, id] of entries) {
      tree.add(word, id);
    }
    tree.finalize();
    await tree.saveToFile(TMP_FILE);
  });

  it.skip("should parse the binary radix file correctly", async () => {
    const parsed = await parse(TMP_FILE);
    const found: [string, number][] = [];
    console.log("Parsed Radix Tree:");
    console.log(
      `Root node: ${parsed.root.fragment}, payload: ${parsed.root.payload}`
    );
    function walk(node: ParsedRadixNode, prefix = "") {
      if (!node) return;
      console.log(`Visiting node: ${node.fragment}, payload: ${node.payload}`);
      const current = prefix + node.fragment;
      if (node.payload !== null) {
        found.push([current, node.payload]);
      }
      for (const child of node.children) {
        walk(child, current);
      }
    }

    walk(parsed.root);

    // Tri pour comparaison stable
    const sortedExpected = entries
      .slice()
      .sort((a, b) => a[0].localeCompare(b[0]));
    const sortedFound = found.slice().sort((a, b) => a[0].localeCompare(b[0]));

    expect(sortedFound).toEqual(sortedExpected);
  });

  it("should read flac metadata", async () => {
    await readMetadata("/home/leo/Music/tidal/");
    // const metadata = await parseFile(
    //   "/home/leo/Music/tidal/Albums/Swift Guad/Hécatombe 2.0/02.Pourquoi.flac"
    // );
    // expect(metadata.common.title).toBe("Pourquoi");
    // expect(metadata.common.artist).toBe("Swift Guad");
    // expect(metadata.common.album).toBe("Hécatombe 2.0");
    // expect(metadata.common.year).toBe(2014);
    // console.log(metadata.common.title, metadata.common.artist);
  });

  afterAll(async () => {
    await fs.unlink(TMP_FILE);
  });
});
