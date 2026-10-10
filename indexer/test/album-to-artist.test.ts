import { describe, it, expect } from "vitest";
import {
  exportAlbumToArtistIndexToBinary,
  importTrackToAlbumIndexFromBinary,
} from "../src/music-indexes";

describe("Album-to-artist index", () => {
  it("picks the artist credited on most of the album's tracks", () => {
    // album 0: artist 4 twice, artist 2 once; album 1: artist 3 only
    const trackToAlbum = new Map([
      [0, 0],
      [1, 0],
      [2, 0],
      [3, 1],
    ]);
    const trackToArtist = new Map([
      [0, 4],
      [1, 2],
      [2, 4],
      [3, 3],
    ]);
    const imported = importTrackToAlbumIndexFromBinary(
      exportAlbumToArtistIndexToBinary(trackToAlbum, trackToArtist)
    );

    expect(imported.entryCount).toBe(2);
    expect(imported.albumIds).toEqual([4, 3]);
  });

  it("breaks ties toward the lowest artist id", () => {
    const trackToAlbum = new Map([
      [0, 0],
      [1, 0],
    ]);
    const trackToArtist = new Map([
      [0, 9],
      [1, 5],
    ]);
    const imported = importTrackToAlbumIndexFromBinary(
      exportAlbumToArtistIndexToBinary(trackToAlbum, trackToArtist)
    );

    expect(imported.albumIds).toEqual([5]);
  });
});
