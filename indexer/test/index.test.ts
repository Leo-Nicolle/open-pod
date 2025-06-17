import { describe, it } from "vitest";
import { readMetadata } from "../src/main"; // adapte le chemin si besoin

describe("Radix tree builder and parser", () => {
  it("should read flac metadata", async () => {
    const indexes = await readMetadata("/home/leo/Music/tidal/");
    // printIndexes(indexes);

    // const tree = buildRadixTree(Array.from(indexes.albumToTracks.entries()));
    // const serialized = serializeTrie(tree);
    // console.log("ICI", serialized);
    // const metadata = await parseFile(
    //   "/home/leo/Music/tidal/Albums/Swift Guad/Hécatombe 2.0/02.Pourquoi.flac"
    // );
    // expect(metadata.common.title).toBe("Pourquoi");
    // expect(metadata.common.artist).toBe("Swift Guad");
    // expect(metadata.common.album).toBe("Hécatombe 2.0");
    // expect(metadata.common.year).toBe(2014);
    // console.log(metadata.common.title, metadata.common.artist);
  });
});
