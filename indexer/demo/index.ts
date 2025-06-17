import { parse, RadixTreeBuilder } from "../src/main";

// --------------------
// EXAMPLE USAGE
// --------------------

const tree = new RadixTreeBuilder();

const entries: [string, number][] = [
  ["hello", 0],
  ["help", 1],
  ["helium", 2],
  ["world", 3],
  ["word", 4],
  ["wolf", 5],
];

for (const [word, id] of entries) {
  debugger;
  tree.add(word, id);
}

tree.finalize();
await tree.saveToFile("songs.radixidx");
const parsed = await parse("songs.radixidx");
