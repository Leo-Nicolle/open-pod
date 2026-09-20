import { describe, it, expect } from "vitest";
import sharp from "sharp";
import fs from "fs/promises";
import os from "os";
import path from "path";
import {
  THUMB_SIZE,
  THUMBNAIL_PIPELINE_VERSION,
  generateAlbumThumbnail,
  generatePlaceholderRGB,
  quantizeTo565InPlace,
  encodeQOI,
  decodeQOI,
  encodeRaw565,
  decodeRaw565,
  packRGB565,
  packThumbnails,
  canReuseCachedThumbnail,
  computeSrcHash,
  extractThumbnails,
  exportAlbumCoverIndexToBinary,
  ALBUM_COVER_FORMAT_BYTE,
} from "../src/thumbnails";

async function makeGradientCoverJpeg(): Promise<Buffer> {
  // Photographic-ish source: a smooth gradient plus per-pixel noise, at a
  // non-square, non-128 size so resize/crop actually has work to do.
  const width = 300;
  const height = 250;
  const raw = Buffer.alloc(width * height * 3);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = (y * width + x) * 3;
      const noise = (Math.sin(x * 12.9898 + y * 78.233) * 43758.5453) % 1;
      const n = Math.floor(Math.abs(noise) * 20);
      raw[i] = Math.min(255, Math.floor((x / width) * 255) + n);
      raw[i + 1] = Math.min(255, Math.floor((y / height) * 255) + n);
      raw[i + 2] = Math.min(255, 128 + n);
    }
  }
  return sharp(raw, { raw: { width, height, channels: 3 } })
    .jpeg({ quality: 90 })
    .toBuffer();
}

async function makeFlatLogoPng(): Promise<Buffer> {
  return sharp({
    create: {
      width: 200,
      height: 200,
      channels: 3,
      background: { r: 20, g: 120, b: 200 },
    },
  })
    .png()
    .toBuffer();
}

describe("thumbnails pipeline", () => {
  it("produces a 128x128 QOI thumbnail from a photographic source", async () => {
    const source = await makeGradientCoverJpeg();
    const result = await generateAlbumThumbnail({
      source,
      albumId: 1,
      config: { format: "qoi" },
    });

    expect(result.ok).toBe(true);
    expect(result.isPlaceholder).toBe(false);
    expect(result.format).toBe("qoi");
    expect(result.width).toBe(THUMB_SIZE);
    expect(result.height).toBe(THUMB_SIZE);
    expect(result.bytes.subarray(0, 4).toString("ascii")).toBe("qoif");
  });

  it("round-trips QOI byte-exactly back to the quantized pixels", async () => {
    const source = await makeGradientCoverJpeg();
    const raw = await sharp(source)
      .resize(THUMB_SIZE, THUMB_SIZE, {
        fit: "cover",
        position: "centre",
        kernel: "lanczos3",
      })
      .removeAlpha()
      .raw()
      .toBuffer();

    quantizeTo565InPlace(raw, THUMB_SIZE, THUMB_SIZE, false);
    const encoded = encodeQOI(raw, THUMB_SIZE, THUMB_SIZE);
    const decoded = decodeQOI(encoded);

    expect(decoded.width).toBe(THUMB_SIZE);
    expect(decoded.height).toBe(THUMB_SIZE);
    expect(Buffer.compare(decoded.pixels, raw)).toBe(0);
  });

  it("is deterministic across two runs on the same input (byte-exact)", async () => {
    const source = await makeFlatLogoPng();
    const a = await generateAlbumThumbnail({ source, albumId: 5 });
    const b = await generateAlbumThumbnail({ source, albumId: 5 });
    expect(Buffer.compare(a.bytes, b.bytes)).toBe(0);
    expect(a.srcHash).toBe(b.srcHash);
  });

  it("quantizes each channel to valid 5/6/5 levels", async () => {
    const source = await makeGradientCoverJpeg();
    const raw = await sharp(source)
      .resize(THUMB_SIZE, THUMB_SIZE, { fit: "cover" })
      .removeAlpha()
      .raw()
      .toBuffer();
    quantizeTo565InPlace(raw, THUMB_SIZE, THUMB_SIZE, false);
    for (let i = 0; i < THUMB_SIZE * THUMB_SIZE; i++) {
      expect(raw[i * 3] & 0x07).toBe(0); // R: low 3 bits zero (5-bit)
      expect(raw[i * 3 + 1] & 0x03).toBe(0); // G: low 2 bits zero (6-bit)
      expect(raw[i * 3 + 2] & 0x07).toBe(0); // B: low 3 bits zero (5-bit)
    }
  });

  it("packs RGB565 per the documented formula", () => {
    expect(packRGB565(0xf8, 0xfc, 0xf8)).toBe(0xffff);
    expect(packRGB565(0, 0, 0)).toBe(0);
    expect(packRGB565(0xf8, 0, 0)).toBe(0xf800);
  });

  it("raw565 output is a fixed 32KB size", async () => {
    const source = await makeFlatLogoPng();
    const result = await generateAlbumThumbnail({
      source,
      albumId: 2,
      config: { format: "raw565" },
    });
    expect(result.bytes.length).toBe(THUMB_SIZE * THUMB_SIZE * 2);
  });

  it("falls back to the same canonical placeholder regardless of album", async () => {
    const a = await generateAlbumThumbnail({ source: null, albumId: 42 });
    const b = await generateAlbumThumbnail({ source: null, albumId: 42 });
    const c = await generateAlbumThumbnail({ source: null, albumId: 43 });

    expect(a.isPlaceholder).toBe(true);
    expect(a.ok).toBe(false);
    // Every album missing art shares one "cover0" placeholder, byte-for-byte,
    // so the packer can dedupe them into a single blob.
    expect(Buffer.compare(a.bytes, b.bytes)).toBe(0);
    expect(Buffer.compare(a.bytes, c.bytes)).toBe(0);
    expect(a.srcHash).toBe(c.srcHash);
  });

  it("falls back to a placeholder on unreadable/corrupt image data", async () => {
    const garbage = Buffer.from("not an image at all, just bytes");
    const result = await generateAlbumThumbnail({
      source: garbage,
      albumId: 7,
    });
    expect(result.isPlaceholder).toBe(true);
    expect(result.width).toBe(THUMB_SIZE);
    expect(result.height).toBe(THUMB_SIZE);
  });

  it("packs multiple distinct thumbnails into one blob with correct offsets", async () => {
    const source1 = await makeFlatLogoPng();
    const source2 = await makeGradientCoverJpeg();
    const r1 = await generateAlbumThumbnail({ source: source1, albumId: 1 });
    const r2 = await generateAlbumThumbnail({ source: source2, albumId: 2 });

    const { blob, index } = packThumbnails([
      { albumId: 1, result: r1 },
      { albumId: 2, result: r2 },
    ]);

    expect(blob.length).toBe(r1.bytes.length + r2.bytes.length);
    const e1 = index.get(1)!;
    const e2 = index.get(2)!;
    expect(e1.offset).toBe(0);
    expect(e1.length).toBe(r1.bytes.length);
    expect(e2.offset).toBe(r1.bytes.length);
    expect(Buffer.compare(blob.subarray(e1.offset, e1.offset + e1.length), r1.bytes)).toBe(0);
    expect(Buffer.compare(blob.subarray(e2.offset, e2.offset + e2.length), r2.bytes)).toBe(0);
  });

  it("dedupes albums with no art down to one shared placeholder blob", async () => {
    const withArt = await generateAlbumThumbnail({
      source: await makeFlatLogoPng(),
      albumId: 1,
    });
    const noArtA = await generateAlbumThumbnail({ source: null, albumId: 2 });
    const noArtB = await generateAlbumThumbnail({ source: null, albumId: 3 });

    const { blob, index } = packThumbnails([
      { albumId: 1, result: withArt },
      { albumId: 2, result: noArtA },
      { albumId: 3, result: noArtB },
    ]);

    // Only two distinct blobs actually got written: the real cover once,
    // and the shared placeholder once — not three.
    expect(blob.length).toBe(withArt.bytes.length + noArtA.bytes.length);

    const e2 = index.get(2)!;
    const e3 = index.get(3)!;
    expect(e2.offset).toBe(e3.offset);
    expect(e2.length).toBe(e3.length);
    expect(e2.isPlaceholder).toBe(true);
    expect(e3.isPlaceholder).toBe(true);
  });

  it("detects reusable cache entries by srcHash", async () => {
    const source = await makeFlatLogoPng();
    const config = { format: "qoi" as const };
    const hash1 = computeSrcHash(source, { ...config, dither: false });
    const hash2 = computeSrcHash(source, { ...config, dither: false });
    const hash3 = computeSrcHash(source, { ...config, dither: true });

    expect(canReuseCachedThumbnail({ offset: 0, length: 10, format: "qoi", w: 128, h: 128, srcHash: hash1, isPlaceholder: false }, hash2)).toBe(true);
    expect(canReuseCachedThumbnail({ offset: 0, length: 10, format: "qoi", w: 128, h: 128, srcHash: hash1, isPlaceholder: false }, hash3)).toBe(false);
    expect(canReuseCachedThumbnail(undefined, hash1)).toBe(false);
  });

  it("generatePlaceholderRGB always fills a full 128x128 buffer", () => {
    const rgb = generatePlaceholderRGB();
    expect(rgb.length).toBe(THUMB_SIZE * THUMB_SIZE * 3);
  });

  it("encodeRaw565 is byte-order little-endian per pixel", () => {
    const rgb = Buffer.from([0xf8, 0xfc, 0xf8]); // -> 0xffff
    const out = encodeRaw565(rgb, 1, 1);
    expect(out.length).toBe(2);
    expect(out.readUInt16LE(0)).toBe(0xffff);
  });

  it("decodeRaw565 loses no further information beyond the initial 565 pack", async () => {
    // decodeRaw565 does the standard 5/6/5 -> 8/8/8 bit-replication expansion
    // (fills the low bits for a full 0-255 preview range), so it won't be
    // byte-exact against our truncated quantizeTo565InPlace output — but
    // re-packing what it produces must recover the exact same 565 value.
    const source = await makeFlatLogoPng();
    const raw = await sharp(source)
      .resize(THUMB_SIZE, THUMB_SIZE, { fit: "cover" })
      .removeAlpha()
      .raw()
      .toBuffer();
    quantizeTo565InPlace(raw, THUMB_SIZE, THUMB_SIZE, false);

    const packed = encodeRaw565(raw, THUMB_SIZE, THUMB_SIZE);
    const unpacked = decodeRaw565(packed, THUMB_SIZE, THUMB_SIZE);
    const repacked = encodeRaw565(unpacked, THUMB_SIZE, THUMB_SIZE);
    expect(Buffer.compare(repacked, packed)).toBe(0);
  });

  it("extractThumbnails reads thumbs.bin + manifest and writes one PNG per album", async () => {
    const withArt = await generateAlbumThumbnail({
      source: await makeFlatLogoPng(),
      albumId: 1,
    });
    const noArt = await generateAlbumThumbnail({ source: null, albumId: 2 });
    const { blob, index } = packThumbnails([
      { albumId: 1, result: withArt },
      { albumId: 2, result: noArt },
    ]);

    const dir = await fs.mkdtemp(path.join(os.tmpdir(), "thumbs-extract-"));
    try {
      await fs.writeFile(path.join(dir, "thumbs.bin"), blob);
      await fs.writeFile(
        path.join(dir, "thumbs.manifest.json"),
        JSON.stringify({
          pipelineVersion: THUMBNAIL_PIPELINE_VERSION,
          entries: Array.from(index.entries()).map(([albumId, entry]) => ({
            albumId,
            ...entry,
          })),
        })
      );

      const outDir = path.join(dir, "out");
      const extracted = await extractThumbnails(dir, outDir);

      expect(extracted.length).toBe(2);
      const withArtEntry = extracted.find((e) => e.albumId === 1)!;
      const noArtEntry = extracted.find((e) => e.albumId === 2)!;
      expect(withArtEntry.isPlaceholder).toBe(false);
      expect(noArtEntry.isPlaceholder).toBe(true);
      expect(path.basename(noArtEntry.file)).toBe("album-2.placeholder.png");

      for (const entry of extracted) {
        const stat = await fs.stat(entry.file);
        expect(stat.isFile()).toBe(true);
        const metadata = await sharp(entry.file).metadata();
        expect(metadata.width).toBe(THUMB_SIZE);
        expect(metadata.height).toBe(THUMB_SIZE);
      }
    } finally {
      await fs.rm(dir, { recursive: true, force: true });
    }
  });

  it("exportAlbumCoverIndexToBinary writes a dense {offset,length,format} record per album", async () => {
    const withArt = await generateAlbumThumbnail({
      source: await makeFlatLogoPng(),
      albumId: 0,
      config: { format: "raw565" },
    });
    const noArt = await generateAlbumThumbnail({
      source: null,
      albumId: 1,
      config: { format: "raw565" },
    });
    const { index } = packThumbnails([
      { albumId: 0, result: withArt },
      { albumId: 1, result: noArt },
    ]);

    const binary = exportAlbumCoverIndexToBinary(index);
    const view = new DataView(
      binary.buffer,
      binary.byteOffset,
      binary.byteLength
    );

    expect(view.getUint32(0, true)).toBe(2);
    expect(binary.byteLength).toBe(4 + 2 * 9);

    const e0 = index.get(0)!;
    expect(view.getUint32(4, true)).toBe(e0.offset);
    expect(view.getUint32(8, true)).toBe(e0.length);
    expect(view.getUint8(12)).toBe(ALBUM_COVER_FORMAT_BYTE.raw565);

    const e1 = index.get(1)!;
    expect(view.getUint32(13, true)).toBe(e1.offset);
    expect(view.getUint32(17, true)).toBe(e1.length);
    expect(view.getUint8(21)).toBe(ALBUM_COVER_FORMAT_BYTE.raw565);
  });

  it("exportAlbumCoverIndexToBinary defaults a missing album id within range to a zero-length entry", () => {
    const sparse = new Map<
      number,
      { offset: number; length: number; format: "raw565"; w: number; h: number; srcHash: string; isPlaceholder: boolean }
    >();
    sparse.set(0, { offset: 100, length: 32768, format: "raw565", w: 128, h: 128, srcHash: "a", isPlaceholder: false });
    // album id 1 intentionally missing
    sparse.set(2, { offset: 200, length: 32768, format: "raw565", w: 128, h: 128, srcHash: "b", isPlaceholder: false });

    const binary = exportAlbumCoverIndexToBinary(sparse);
    const view = new DataView(binary.buffer, binary.byteOffset, binary.byteLength);

    expect(view.getUint32(0, true)).toBe(3);
    // gap entry (album id 1): offset=0, length=0, format=0
    expect(view.getUint32(4 + 9, true)).toBe(0);
    expect(view.getUint32(8 + 9, true)).toBe(0);
    expect(view.getUint8(12 + 9)).toBe(0);
  });

  it("exportAlbumCoverIndexToBinary produces an empty index for an empty map", () => {
    const binary = exportAlbumCoverIndexToBinary(new Map());
    const view = new DataView(binary.buffer, binary.byteOffset, binary.byteLength);

    expect(binary.byteLength).toBe(4);
    expect(view.getUint32(0, true)).toBe(0);
  });
});
