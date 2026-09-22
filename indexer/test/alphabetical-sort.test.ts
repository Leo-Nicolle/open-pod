import { describe, it, expect } from "vitest";
import { sortIndexesAlphabetically } from "../src/crawler";
import type { CrawlIndex, ThumbnailResult } from "../src/types";

// Builds a small CrawlIndex where every id space is intentionally in
// non-alphabetical (crawl/insertion) order, so a passing test proves the
// remap actually reorders things rather than happening to match input order.
function buildUnsortedIndex(): CrawlIndex {
  // Artists, by insertion (id) order: 0=Zeta, 1=Alpha, 2=Mno
  const indexToArtist = new Map([
    [0, "Zeta"],
    [1, "Alpha"],
    [2, "Mno"],
  ]);
  // Albums, by insertion order: 0=Wanderlust, 1=Beta Album
  const indexToAlbum = new Map([
    [0, "Wanderlust"],
    [1, "Beta Album"],
  ]);
  // Genres, by insertion order: 0=Rock, 1=Jazz
  const indexToGenre = new Map([
    [0, "Rock"],
    [1, "Jazz"],
  ]);
  // Tracks, by insertion order (their old ids)
  const indexToTrack = new Map([
    [0, "Track Zulu"], // old 0 -> belongs to album 0 (Wanderlust)
    [1, "Track Alpha"], // old 1 -> belongs to album 0 (Wanderlust)
    [2, "Track Mike"], // old 2 -> belongs to album 1 (Beta Album)
  ]);
  const indexToPath = new Map([
    [0, "a/zulu.mp3"],
    [1, "a/alpha.mp3"],
    [2, "b/mike.mp3"],
  ]);

  const trackToAlbum = new Map([
    [0, 0],
    [1, 0],
    [2, 1],
  ]);

  const albumToTracks = new Map([
    [0, new Set([0, 1])],
    [1, new Set([2])],
  ]);
  const artistToAlbums = new Map([
    [1, new Set([0])], // Alpha -> Wanderlust
    [0, new Set([1])], // Zeta -> Beta Album
  ]);
  const artistToTracks = new Map([
    [1, new Set([0, 1])],
    [0, new Set([2])],
  ]);
  const genreToAlbums = new Map([
    [0, new Set([0])],
    [1, new Set([1])],
  ]);
  const genreToTracks = new Map([
    [0, new Set([0, 1])],
    [1, new Set([2])],
  ]);
  const genreToArtists = new Map([
    [0, new Set([1])],
    [1, new Set([0])],
  ]);

  const metadataByTrackIndex = new Map([
    [
      0,
      {
        title: "Track Zulu",
        artist: "Alpha",
        album: "Wanderlust",
        genre: "Rock",
        index: 1,
        year: 2000,
        duration: 100,
      },
    ],
    [
      1,
      {
        title: "Track Alpha",
        artist: "Alpha",
        album: "Wanderlust",
        genre: "Rock",
        index: 2,
        year: 2000,
        duration: 120,
      },
    ],
    [
      2,
      {
        title: "Track Mike",
        artist: "Zeta",
        album: "Beta Album",
        genre: "Jazz",
        index: 1,
        year: 1999,
        duration: 90,
      },
    ],
  ]);

  const placeholder: ThumbnailResult = {
    rgb565: new Uint8Array(),
    width: 0,
    height: 0,
    isPlaceholder: true,
  } as ThumbnailResult;
  const albumThumbnails = new Map([
    [0, { ...placeholder, isPlaceholder: false }],
    [1, placeholder],
  ]);

  return {
    indexToTrack,
    indexToArtist,
    indexToAlbum,
    indexToGenre,
    indexToPath,
    artistToAlbums,
    albumToTracks,
    artistToTracks,
    genreToAlbums,
    genreToTracks,
    genreToArtists,
    metadataByTrackIndex,
    albumThumbnails,
    trackToAlbum,
  };
}

// Binary export always sorts a Map's entries by numeric id before writing
// (see exportStringIndexToBinary), so a Map's own iteration/insertion order
// is irrelevant - what matters is what each id *key* maps to, and that
// ascending-key order is alphabetical. Assert both that way rather than via
// raw Map iteration order.
function byAscendingId<V>(map: Map<number, V>): [number, V][] {
  return Array.from(map.entries()).sort((a, b) => a[0] - b[0]);
}

describe("sortIndexesAlphabetically", () => {
  const sorted = sortIndexesAlphabetically(buildUnsortedIndex());

  it("relabels artist ids so id order is alphabetical", () => {
    expect(byAscendingId(sorted.indexToArtist)).toEqual([
      [0, "Alpha"],
      [1, "Mno"],
      [2, "Zeta"],
    ]);
  });

  it("relabels album ids so id order is alphabetical", () => {
    expect(byAscendingId(sorted.indexToAlbum)).toEqual([
      [0, "Beta Album"],
      [1, "Wanderlust"],
    ]);
  });

  it("relabels genre ids so id order is alphabetical", () => {
    expect(byAscendingId(sorted.indexToGenre)).toEqual([
      [0, "Jazz"],
      [1, "Rock"],
    ]);
  });

  it("relabels track ids so id order is alphabetical", () => {
    expect(byAscendingId(sorted.indexToTrack)).toEqual([
      [0, "Track Alpha"],
      [1, "Track Mike"],
      [2, "Track Zulu"],
    ]);
  });

  it("keeps track->path in sync with the remapped track ids", () => {
    expect(sorted.indexToPath.get(0)).toBe("a/alpha.mp3"); // Track Alpha
    expect(sorted.indexToPath.get(1)).toBe("b/mike.mp3"); // Track Mike
    expect(sorted.indexToPath.get(2)).toBe("a/zulu.mp3"); // Track Zulu
  });

  it("keeps track->album consistent, and tracks within an album stay ordered by (new) track id", () => {
    // Wanderlust ("Track Alpha" id 0, "Track Zulu" id 2) is now album id 1
    expect(sorted.trackToAlbum.get(0)).toBe(1);
    expect(sorted.trackToAlbum.get(2)).toBe(1);
    // Beta Album ("Track Mike" id 1) is now album id 0
    expect(sorted.trackToAlbum.get(1)).toBe(0);

    expect(sorted.albumToTracks.get(1)).toEqual(new Set([0, 2]));
    expect(sorted.albumToTracks.get(0)).toEqual(new Set([1]));
  });

  it("keeps artist/genre relationship maps consistent with the remapped ids", () => {
    // Alpha (new id 0) -> Wanderlust (new id 1)
    expect(sorted.artistToAlbums.get(0)).toEqual(new Set([1]));
    // Zeta (new id 2) -> Beta Album (new id 0)
    expect(sorted.artistToAlbums.get(2)).toEqual(new Set([0]));

    // Jazz (new id 0) -> Zeta (new id 2)
    expect(sorted.genreToArtists.get(0)).toEqual(new Set([2]));
    // Rock (new id 1) -> Alpha (new id 0)
    expect(sorted.genreToArtists.get(1)).toEqual(new Set([0]));
  });

  it("keeps metadata and thumbnails keyed by the remapped track/album ids", () => {
    expect(sorted.metadataByTrackIndex.get(0)?.title).toBe("Track Alpha");
    expect(sorted.metadataByTrackIndex.get(2)?.title).toBe("Track Zulu");

    // Beta Album (new id 0) was old album id 1, which had the placeholder
    expect(sorted.albumThumbnails.get(0)?.isPlaceholder).toBe(true);
    // Wanderlust (new id 1) was old album id 0, the non-placeholder one
    expect(sorted.albumThumbnails.get(1)?.isPlaceholder).toBe(false);
  });
});
