import { describe, it, expect } from "vitest";
import { unserialize } from "../src/crawler";
import CrawlData from "./stubs/crawl-index.json";
import {
  exportDurationIndexToBinary,
  importDurationIndexFromBinary,
} from "../src/music-indexes";
import type { CrawlIndex, TrackMetadata } from "../src/types";

describe("Duration Index", () => {
  const createTestMetadata = async (): Promise<
    Map<number, TrackMetadata>
  > => {
    const crawlIndex = unserialize(JSON.stringify(CrawlData)) as CrawlIndex;
    return crawlIndex.metadataByTrackIndex;
  };

  it("should write entryCount as the first uint32 of the header", async () => {
    const metadata = await createTestMetadata();
    const binary = exportDurationIndexToBinary(metadata);
    const view = new DataView(binary.buffer, binary.byteOffset, binary.byteLength);

    // Fixture has 349 dense, 0-based track ids (0-348) - entryCount is the
    // highest id + 1, not just metadata.size, though they happen to match
    // here since the fixture has no gaps.
    expect(view.getUint32(0, true)).toBe(349);
    expect(view.getUint32(0, true)).toBe(metadata.size);
  });

  it("should be exactly 4 + entryCount * 2 bytes (no padding)", async () => {
    const metadata = await createTestMetadata();
    const binary = exportDurationIndexToBinary(metadata);

    expect(binary.byteLength).toBe(4 + 349 * 2);
  });

  it("should round a known fixture duration to the nearest second", async () => {
    const metadata = await createTestMetadata();
    const binary = exportDurationIndexToBinary(metadata);
    const imported = importDurationIndexFromBinary(binary);

    // Fixture track id 0 has duration 221.57061224489797s.
    expect(metadata.get(0)?.duration).toBeCloseTo(221.57061224489797);
    expect(imported.durations[0]).toBe(222);
  });

  it("should round-trip every fixture duration", async () => {
    const metadata = await createTestMetadata();
    const binary = exportDurationIndexToBinary(metadata);
    const imported = importDurationIndexFromBinary(binary);

    expect(imported.entryCount).toBe(349);
    expect(imported.durations.length).toBe(349);
    for (const [id, meta] of metadata) {
      expect(imported.durations[id]).toBe(Math.round(meta.duration));
    }
  });

  it("should default a missing track id within range to 0 seconds", () => {
    const sparse = new Map<number, TrackMetadata>();
    const track = (duration: number): TrackMetadata => ({
      title: "t",
      artist: "a",
      album: "al",
      genre: "g",
      year: 2024,
      index: 0,
      duration,
    });
    sparse.set(0, track(120));
    // id 1 intentionally missing
    sparse.set(2, track(90));

    const binary = exportDurationIndexToBinary(sparse);
    const imported = importDurationIndexFromBinary(binary);

    expect(imported.entryCount).toBe(3);
    expect(imported.durations).toEqual([120, 0, 90]);
  });

  it("should produce an empty index for an empty map", () => {
    const binary = exportDurationIndexToBinary(new Map());
    const imported = importDurationIndexFromBinary(binary);

    expect(binary.byteLength).toBe(4);
    expect(imported.entryCount).toBe(0);
    expect(imported.durations).toEqual([]);
  });
});
