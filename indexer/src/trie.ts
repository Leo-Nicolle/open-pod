import { TrieBuilder } from "utrie";

export function wordsToTrie(words: string[]): Uint8Array {
  const trieBuilder = new TrieBuilder();
  for (const word of words) {
    trieBuilder.add(word);
  }
  return trieBuilder.finalize();
}
