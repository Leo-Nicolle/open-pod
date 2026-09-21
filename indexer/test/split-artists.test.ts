import { describe, it, expect } from "vitest";
import { splitArtists } from "../src/utils";

describe("splitArtists", () => {
  it("splits ampersand-joined collaborations into individual artists", () => {
    expect(splitArtists("Artist A & Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
  });

  it("splits three-way collaborations", () => {
    expect(splitArtists("Artist A & Artist B & Artist C")).toEqual([
      "Artist A",
      "Artist B",
      "Artist C",
    ]);
  });

  it("resolves different tag combinations of the same artists to the same set", () => {
    // The reported bug: "A & B", "A & B & C" and "A & C" used to become three
    // distinct, noisy artist entries instead of a shared A/B/C.
    expect(splitArtists("Artist A & Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
    expect(splitArtists("Artist A & Artist B & Artist C")).toEqual([
      "Artist A",
      "Artist B",
      "Artist C",
    ]);
    expect(splitArtists("Artist A & Artist C")).toEqual([
      "Artist A",
      "Artist C",
    ]);
  });

  it("splits featuring credits", () => {
    expect(splitArtists("Artist A feat. Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
    expect(splitArtists("Artist A ft. Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
    expect(splitArtists("Artist A featuring Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
    expect(splitArtists("Artist A with Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
  });

  it("splits comma and slash separated tags", () => {
    expect(splitArtists("Artist A, Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
    expect(splitArtists("Artist A/Artist B")).toEqual([
      "Artist A",
      "Artist B",
    ]);
  });

  it("de-dupes case-insensitively", () => {
    expect(splitArtists("Artist A & artist a")).toEqual(["Artist A"]);
  });

  it("sanitizes each split name (accents, whitespace)", () => {
    expect(splitArtists("Café Artist  &   Öther Artist")).toEqual([
      "Cafe Artist",
      "Other Artist",
    ]);
  });

  it("returns a single-element array for a solo artist", () => {
    expect(splitArtists("Solo Artist")).toEqual(["Solo Artist"]);
  });

  it("returns an empty array for a missing or empty tag", () => {
    expect(splitArtists(undefined)).toEqual([]);
    expect(splitArtists(null)).toEqual([]);
    expect(splitArtists("")).toEqual([]);
    expect(splitArtists("   ")).toEqual([]);
  });
});
