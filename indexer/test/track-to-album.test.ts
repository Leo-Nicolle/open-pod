import { describe, it, expect } from "vitest";
import {
  exportTrackToAlbumIndexToBinary,
  importTrackToAlbumIndexFromBinary,
} from "../src/music-indexes";

describe("Track-to-album index", () => {
  it("writes entryCount as the first uint32 of the header", () => {
    const trackToAlbum = new Map([
      [0, 5],
      [1, 5],
      [2, 7],
    ]);
    const binary = exportTrackToAlbumIndexToBinary(trackToAlbum);
    const view = new DataView(
      binary.buffer,
      binary.byteOffset,
      binary.byteLength
    );

    expect(view.getUint32(0, true)).toBe(3);
  });

  it("is exactly 4 + entryCount * 4 bytes (no padding)", () => {
    const trackToAlbum = new Map([
      [0, 1],
      [1, 2],
    ]);
    const binary = exportTrackToAlbumIndexToBinary(trackToAlbum);

    expect(binary.byteLength).toBe(4 + 2 * 4);
  });

  it("round-trips album ids indexed densely by track id", () => {
    const trackToAlbum = new Map([
      [0, 10],
      [1, 10],
      [2, 11],
      [3, 12],
    ]);
    const binary = exportTrackToAlbumIndexToBinary(trackToAlbum);
    const imported = importTrackToAlbumIndexFromBinary(binary);

    expect(imported.entryCount).toBe(4);
    expect(imported.albumIds).toEqual([10, 10, 11, 12]);
  });

  it("defaults a missing track id within range to album 0", () => {
    const sparse = new Map<number, number>();
    sparse.set(0, 3);
    // id 1 intentionally missing
    sparse.set(2, 4);

    const binary = exportTrackToAlbumIndexToBinary(sparse);
    const imported = importTrackToAlbumIndexFromBinary(binary);

    expect(imported.entryCount).toBe(3);
    expect(imported.albumIds).toEqual([3, 0, 4]);
  });

  it("produces an empty index for an empty map", () => {
    const binary = exportTrackToAlbumIndexToBinary(new Map());
    const imported = importTrackToAlbumIndexFromBinary(binary);

    expect(binary.byteLength).toBe(4);
    expect(imported.entryCount).toBe(0);
    expect(imported.albumIds).toEqual([]);
  });
});
