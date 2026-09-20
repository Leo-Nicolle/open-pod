// Generates display-ready 128x128 album-art thumbnails for the U5 firmware.
// See .agents/thumbnails-plan.md for the full design rationale.
//
// MCU decode contract (must stay in sync with the firmware):
//   - QOI blobs: standard QOI, 3-channel, sRGB, 128x128, pixel values already
//     quantized to 5/6/5 levels (so 888->565 packing on the MCU is lossless).
//   - raw565 blobs: little-endian RGB565, 128x128, row-major, no header.
//   - Row order is top-left origin, row-major, matching the panel window write.
//   - 565 packing: (R&0xF8)<<8 | (G&0xFC)<<3 | (B>>3)
import fs from "fs/promises";
import path from "path";
import { createHash } from "crypto";
import sharp from "sharp";
import { parseFile } from "music-metadata";

export const THUMB_SIZE = 128;
// Bump when resize kernel / quantization / format assumptions change so a
// stale thumbs.bin can be detected and fully rebuilt.
export const THUMBNAIL_PIPELINE_VERSION = 1;

export type ThumbnailFormat = "qoi" | "raw565";

export type ThumbnailConfig = {
  format: ThumbnailFormat;
  dither: boolean; // ordered (Bayer) dither, off by default per plan 1b
};

// raw565 chosen over QOI after on-device benchmarking on real hardware
// (see .agents/covers-plan.md Phase 0): real covers tie-or-beat QOI on SD
// read+decode latency, raw565 needs zero firmware decoder, and it uniquely
// supports the production AlbumArt module's direct-seek-blit design
// (software/src/storage/AlbumArt.h), which QOI's sequential decoding can't.
export const DEFAULT_THUMBNAIL_CONFIG: ThumbnailConfig = {
  format: "raw565",
  dither: false,
};

export type ThumbnailResult = {
  ok: boolean; // false when a placeholder had to be substituted
  format: ThumbnailFormat;
  width: number;
  height: number;
  bytes: Buffer;
  srcHash: string;
  isPlaceholder: boolean;
};

export type PackedThumbnailEntry = {
  offset: number;
  length: number;
  format: ThumbnailFormat;
  w: number;
  h: number;
  srcHash: string;
  isPlaceholder: boolean;
};

// ---------------------------------------------------------------------------
// Art resolution: embedded picture first, then cover.* next to the tracks.
// ---------------------------------------------------------------------------

const COVER_BASENAMES = ["cover", "folder", "album", "front"];
const COVER_EXTS = [".jpg", ".jpeg", ".png", ".webp"];

export async function resolveAlbumArtSource(
  sampleTrackPath: string
): Promise<Buffer | null> {
  try {
    const { common } = await parseFile(sampleTrackPath, { skipCovers: false });
    const picture = common.picture?.[0];
    if (picture?.data?.length) {
      return Buffer.from(picture.data);
    }
  } catch (err) {
    console.warn(
      `[thumbnails] failed to read embedded art from ${sampleTrackPath}: ${
        (err as Error).message
      }`
    );
  }

  const folder = path.dirname(sampleTrackPath);
  let entries: string[];
  try {
    entries = await fs.readdir(folder);
  } catch {
    return null;
  }
  const lowerEntries = entries.map((name) => ({
    name,
    lower: name.toLowerCase(),
  }));
  for (const base of COVER_BASENAMES) {
    for (const ext of COVER_EXTS) {
      const match = lowerEntries.find((e) => e.lower === `${base}${ext}`);
      if (match) {
        return await fs.readFile(path.join(folder, match.name));
      }
    }
  }
  return null;
}

// ---------------------------------------------------------------------------
// Decode + resize (sharp does all quality-critical work here).
// ---------------------------------------------------------------------------

async function decodeAndNormalize(source: Buffer): Promise<Buffer> {
  return sharp(source)
    .rotate()
    .resize(THUMB_SIZE, THUMB_SIZE, {
      fit: "cover",
      position: "centre",
      kernel: "lanczos3",
    })
    .flatten({ background: { r: 0, g: 0, b: 0 } })
    .toColorspace("srgb")
    .removeAlpha()
    .raw()
    .toBuffer();
}

// ---------------------------------------------------------------------------
// 5/6/5 quantization (channels stay RGB888, values snapped to 565 levels).
// ---------------------------------------------------------------------------

const BAYER_4X4 = [
  [0, 8, 2, 10],
  [12, 4, 14, 6],
  [3, 11, 1, 9],
  [15, 7, 13, 5],
];

function quantizeChannel(value: number, levels: number, bias?: number): number {
  const step = 256 / levels;
  const biased = bias === undefined ? value : value + (bias - 0.5) * step;
  const level = Math.max(0, Math.min(levels - 1, Math.round(biased / step)));
  return level * step;
}

export function quantizeTo565InPlace(
  rgb: Buffer,
  width: number,
  height: number,
  dither: boolean
): void {
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const bias = dither
        ? (BAYER_4X4[y % 4][x % 4] + 0.5) / 16
        : undefined;
      const idx = (y * width + x) * 3;
      rgb[idx] = quantizeChannel(rgb[idx], 32, bias); // R: 5 bits
      rgb[idx + 1] = quantizeChannel(rgb[idx + 1], 64, bias); // G: 6 bits
      rgb[idx + 2] = quantizeChannel(rgb[idx + 2], 32, bias); // B: 5 bits
    }
  }
}

export function packRGB565(r: number, g: number, b: number): number {
  return (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)) & 0xffff;
}

export function encodeRaw565(rgb: Buffer, width: number, height: number): Buffer {
  const out = Buffer.alloc(width * height * 2);
  for (let i = 0; i < width * height; i++) {
    const packed = packRGB565(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    out.writeUInt16LE(packed, i * 2);
  }
  return out;
}

export function decodeRaw565(buf: Buffer, width: number, height: number): Buffer {
  const out = Buffer.alloc(width * height * 3);
  for (let i = 0; i < width * height; i++) {
    const packed = buf.readUInt16LE(i * 2);
    const r5 = (packed >> 11) & 0x1f;
    const g6 = (packed >> 5) & 0x3f;
    const b5 = packed & 0x1f;
    out[i * 3] = (r5 << 3) | (r5 >> 2);
    out[i * 3 + 1] = (g6 << 2) | (g6 >> 4);
    out[i * 3 + 2] = (b5 << 3) | (b5 >> 2);
  }
  return out;
}

// ---------------------------------------------------------------------------
// QOI codec, reimplemented directly from the spec (qoiformat.org) so the
// encoder stays byte-identical to whatever reference decoder the firmware
// uses. 3-channel (RGB) only — album art never needs an alpha channel.
// ---------------------------------------------------------------------------

const QOI_OP_INDEX = 0x00;
const QOI_OP_DIFF = 0x40;
const QOI_OP_LUMA = 0x80;
const QOI_OP_RUN = 0xc0;
const QOI_OP_RGB = 0xfe;
const QOI_MASK_2 = 0xc0;

function wrap8(x: number): number {
  return ((x + 128) & 0xff) - 128;
}

function qoiHash(r: number, g: number, b: number): number {
  return (r * 3 + g * 5 + b * 7 + 255 * 11) % 64;
}

export function encodeQOI(pixels: Buffer, width: number, height: number): Buffer {
  const pixelCount = width * height;
  const maxSize = pixelCount * 4 + 14 + 8;
  const out = Buffer.alloc(maxSize);
  let p = 0;
  out.write("qoif", p, "ascii");
  p += 4;
  out.writeUInt32BE(width, p);
  p += 4;
  out.writeUInt32BE(height, p);
  p += 4;
  out.writeUInt8(3, p); // channels
  p += 1;
  out.writeUInt8(0, p); // colorspace: sRGB
  p += 1;

  const indexR = new Uint8Array(64);
  const indexG = new Uint8Array(64);
  const indexB = new Uint8Array(64);
  let prevR = 0;
  let prevG = 0;
  let prevB = 0;
  let run = 0;

  for (let i = 0; i < pixelCount; i++) {
    const off = i * 3;
    const r = pixels[off];
    const g = pixels[off + 1];
    const b = pixels[off + 2];

    if (r === prevR && g === prevG && b === prevB) {
      run++;
      if (run === 62 || i === pixelCount - 1) {
        out[p++] = QOI_OP_RUN | (run - 1);
        run = 0;
      }
      continue;
    }

    if (run > 0) {
      out[p++] = QOI_OP_RUN | (run - 1);
      run = 0;
    }

    const hash = qoiHash(r, g, b);
    if (indexR[hash] === r && indexG[hash] === g && indexB[hash] === b) {
      out[p++] = QOI_OP_INDEX | hash;
    } else {
      indexR[hash] = r;
      indexG[hash] = g;
      indexB[hash] = b;

      const dr = wrap8(r - prevR);
      const dg = wrap8(g - prevG);
      const db = wrap8(b - prevB);

      if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
        out[p++] = QOI_OP_DIFF | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
      } else {
        const drMg = wrap8(dr - dg);
        const dbMg = wrap8(db - dg);
        if (dg >= -32 && dg <= 31 && drMg >= -8 && drMg <= 7 && dbMg >= -8 && dbMg <= 7) {
          out[p++] = QOI_OP_LUMA | (dg + 32);
          out[p++] = ((drMg + 8) << 4) | (dbMg + 8);
        } else {
          out[p++] = QOI_OP_RGB;
          out[p++] = r;
          out[p++] = g;
          out[p++] = b;
        }
      }
    }

    prevR = r;
    prevG = g;
    prevB = b;
  }

  for (let i = 0; i < 7; i++) out[p++] = 0;
  out[p++] = 1;

  return out.subarray(0, p);
}

export function decodeQOI(buf: Buffer): {
  width: number;
  height: number;
  pixels: Buffer;
} {
  if (buf.toString("ascii", 0, 4) !== "qoif") {
    throw new Error("invalid QOI magic");
  }
  const width = buf.readUInt32BE(4);
  const height = buf.readUInt32BE(8);
  const pixelCount = width * height;
  const pixels = Buffer.alloc(pixelCount * 3);

  const indexR = new Uint8Array(64);
  const indexG = new Uint8Array(64);
  const indexB = new Uint8Array(64);
  let r = 0;
  let g = 0;
  let b = 0;
  let p = 14;
  let px = 0;

  while (px < pixelCount) {
    const byte = buf[p++];
    if (byte === QOI_OP_RGB) {
      r = buf[p++];
      g = buf[p++];
      b = buf[p++];
    } else if ((byte & QOI_MASK_2) === QOI_OP_RUN) {
      const run = (byte & 0x3f) + 1;
      for (let k = 0; k < run; k++) {
        pixels[px * 3] = r;
        pixels[px * 3 + 1] = g;
        pixels[px * 3 + 2] = b;
        px++;
      }
      continue;
    } else if ((byte & QOI_MASK_2) === QOI_OP_INDEX) {
      const idx = byte & 0x3f;
      r = indexR[idx];
      g = indexG[idx];
      b = indexB[idx];
    } else if ((byte & QOI_MASK_2) === QOI_OP_DIFF) {
      r = (r + (((byte >> 4) & 0x03) - 2)) & 0xff;
      g = (g + (((byte >> 2) & 0x03) - 2)) & 0xff;
      b = (b + ((byte & 0x03) - 2)) & 0xff;
    } else {
      // QOI_OP_LUMA
      const dg = (byte & 0x3f) - 32;
      const byte2 = buf[p++];
      const drMg = ((byte2 >> 4) & 0x0f) - 8;
      const dbMg = (byte2 & 0x0f) - 8;
      g = (g + dg) & 0xff;
      r = (r + dg + drMg) & 0xff;
      b = (b + dg + dbMg) & 0xff;
    }

    const hash = qoiHash(r, g, b);
    indexR[hash] = r;
    indexG[hash] = g;
    indexB[hash] = b;
    pixels[px * 3] = r;
    pixels[px * 3 + 1] = g;
    pixels[px * 3 + 2] = b;
    px++;
  }

  return { width, height, pixels };
}

// ---------------------------------------------------------------------------
// Placeholder art (no source found / source unreadable). One canonical
// image for every album missing art — not one per album — so packThumbnails
// can dedupe them down to a single "cover0" blob in thumbs.bin.
// ---------------------------------------------------------------------------

const PLACEHOLDER_RGB: readonly [number, number, number] = [64, 64, 72];

export function generatePlaceholderRGB(): Buffer {
  const [r, g, b] = PLACEHOLDER_RGB;
  const buf = Buffer.alloc(THUMB_SIZE * THUMB_SIZE * 3);
  for (let i = 0; i < THUMB_SIZE * THUMB_SIZE; i++) {
    buf[i * 3] = r;
    buf[i * 3 + 1] = g;
    buf[i * 3 + 2] = b;
  }
  return buf;
}

// ---------------------------------------------------------------------------
// srcHash: identifies (source bytes + resize/quantize/format params) so a
// re-crawl can skip regenerating and re-packing unchanged thumbnails.
// ---------------------------------------------------------------------------

export function computeSrcHash(
  sourceBytes: Buffer | null,
  config: ThumbnailConfig
): string {
  const hash = createHash("sha256");
  hash.update(String(THUMBNAIL_PIPELINE_VERSION));
  hash.update(config.format);
  hash.update(config.dither ? "dither1" : "dither0");
  hash.update(sourceBytes ?? Buffer.from("placeholder"));
  return hash.digest("hex");
}

// ---------------------------------------------------------------------------
// Per-album entry point.
// ---------------------------------------------------------------------------

export async function generateAlbumThumbnail(opts: {
  source: Buffer | null;
  albumId: number;
  config?: Partial<ThumbnailConfig>;
}): Promise<ThumbnailResult> {
  const config: ThumbnailConfig = {
    ...DEFAULT_THUMBNAIL_CONFIG,
    ...opts.config,
  };

  let rgb: Buffer;
  let isPlaceholder = false;

  if (opts.source) {
    try {
      rgb = await decodeAndNormalize(opts.source);
    } catch (err) {
      console.warn(
        `[thumbnails] failed to decode art for album ${opts.albumId}: ${
          (err as Error).message
        }`
      );
      rgb = generatePlaceholderRGB();
      isPlaceholder = true;
    }
  } else {
    rgb = generatePlaceholderRGB();
    isPlaceholder = true;
  }

  quantizeTo565InPlace(rgb, THUMB_SIZE, THUMB_SIZE, config.dither);

  const bytes =
    config.format === "qoi"
      ? encodeQOI(rgb, THUMB_SIZE, THUMB_SIZE)
      : encodeRaw565(rgb, THUMB_SIZE, THUMB_SIZE);

  const srcHash = computeSrcHash(isPlaceholder ? null : opts.source, config);

  return {
    ok: !isPlaceholder,
    format: config.format,
    width: THUMB_SIZE,
    height: THUMB_SIZE,
    bytes,
    srcHash,
    isPlaceholder,
  };
}

// ---------------------------------------------------------------------------
// Packer: concatenate all album blobs into one thumbs.bin, no per-entry
// framing — offset/length live in the index instead.
//
// Entries with identical srcHash (most commonly every album missing art,
// since they all resolve to the same canonical placeholder) are deduped:
// only the first occurrence is written to the blob, and every later album
// with that hash is redirected to that same offset/length rather than
// storing another copy.
// ---------------------------------------------------------------------------

export function packThumbnails(
  entries: Array<{ albumId: number; result: ThumbnailResult }>
): { blob: Buffer; index: Map<number, PackedThumbnailEntry> } {
  const index = new Map<number, PackedThumbnailEntry>();
  const chunks: Buffer[] = [];
  const byHash = new Map<string, { offset: number; length: number }>();
  let offset = 0;
  for (const { albumId, result } of entries) {
    let location = byHash.get(result.srcHash);
    if (!location) {
      location = { offset, length: result.bytes.length };
      byHash.set(result.srcHash, location);
      chunks.push(result.bytes);
      offset += result.bytes.length;
    }
    index.set(albumId, {
      offset: location.offset,
      length: location.length,
      format: result.format,
      w: result.width,
      h: result.height,
      srcHash: result.srcHash,
      isPlaceholder: result.isPlaceholder,
    });
  }
  return { blob: Buffer.concat(chunks), index };
}

// Shared contract with the firmware's album_cover_entry_t.format byte
// (software/src/storage/Music_lookup.h) — a layout change here needs a
// matching firmware change, not just a new enum value.
export const ALBUM_COVER_FORMAT_BYTE: Record<ThumbnailFormat, number> = {
  qoi: 0,
  raw565: 1,
};

// Format: [uint32 entryCount][{uint32 offset, uint32 length, uint8 format}]*
// entryCount, directly indexed by (dense, 0-based) album id — mirrors
// exportDurationIndexToBinary/exportTrackToAlbumIndexToBinary's dense
// fixed-width layout. Lives here (not music-indexes.ts) because it depends
// on packThumbnails' output, which only exists post-crawl in generate.ts.
export function exportAlbumCoverIndexToBinary(
  index: Map<number, PackedThumbnailEntry>
): Uint8Array {
  const maxId = Math.max(-1, ...Array.from(index.keys()));
  const entryCount = maxId + 1;
  const entrySize = 9; // 4 (offset) + 4 (length) + 1 (format)
  const buffer = new ArrayBuffer(4 + entryCount * entrySize);
  const view = new DataView(buffer);
  view.setUint32(0, entryCount, true);
  for (let id = 0; id < entryCount; id++) {
    const entry = index.get(id);
    const base = 4 + id * entrySize;
    view.setUint32(base, entry?.offset ?? 0, true);
    view.setUint32(base + 4, entry?.length ?? 0, true);
    view.setUint8(base + 8, entry ? ALBUM_COVER_FORMAT_BYTE[entry.format] : 0);
  }
  return new Uint8Array(buffer);
}

// Whether a previously-packed entry can be reused as-is on a re-crawl
// (same srcHash, and the caller has already checked pipelineVersion matches).
export function canReuseCachedThumbnail(
  prevEntry: PackedThumbnailEntry | undefined,
  newSrcHash: string
): boolean {
  return prevEntry !== undefined && prevEntry.srcHash === newSrcHash;
}

// ---------------------------------------------------------------------------
// Debug helper: unpack thumbs.bin back into individual PNGs, one per album,
// per plan section 9's round-trip preview. Reads the manifest generate.ts
// writes next to thumbs.bin, so this points at that same output directory.
// ---------------------------------------------------------------------------

export type ThumbsManifest = {
  pipelineVersion: number;
  entries: Array<{ albumId: number } & PackedThumbnailEntry>;
};

export type ExtractedThumbnail = {
  albumId: number;
  file: string;
  isPlaceholder: boolean;
};

export async function extractThumbnails(
  inputDir: string,
  outputDir: string
): Promise<ExtractedThumbnail[]> {
  const manifest: ThumbsManifest = JSON.parse(
    await fs.readFile(path.join(inputDir, "thumbs.manifest.json"), "utf-8")
  );
  const blob = await fs.readFile(path.join(inputDir, "thumbs.bin"));

  await fs.mkdir(outputDir, { recursive: true });

  const results: ExtractedThumbnail[] = [];
  for (const entry of manifest.entries) {
    const slice = blob.subarray(entry.offset, entry.offset + entry.length);
    const rgb =
      entry.format === "qoi"
        ? decodeQOI(slice).pixels
        : decodeRaw565(slice, entry.w, entry.h);

    const file = path.join(
      outputDir,
      `album-${entry.albumId}${entry.isPlaceholder ? ".placeholder" : ""}.png`
    );
    await sharp(rgb, { raw: { width: entry.w, height: entry.h, channels: 3 } })
      .png()
      .toFile(file);

    results.push({ albumId: entry.albumId, file, isPlaceholder: entry.isPlaceholder });
  }
  return results;
}

// ---------------------------------------------------------------------------
// Phase 1 benchmark helper: run both formats over a corpus of source images
// and report median sizes, to decide qoi vs raw565 per plan 1a.
// ---------------------------------------------------------------------------

export type FormatBenchmarkReport = {
  count: number;
  qoi: { median: number; min: number; max: number; mean: number };
  raw565: { size: number }; // fixed size, no need to measure
};

export async function benchmarkThumbnailFormats(
  sourcePaths: string[]
): Promise<FormatBenchmarkReport> {
  const qoiSizes: number[] = [];
  for (const sourcePath of sourcePaths) {
    const source = await fs.readFile(sourcePath);
    const result = await generateAlbumThumbnail({
      source,
      albumId: qoiSizes.length,
      config: { format: "qoi" },
    });
    qoiSizes.push(result.bytes.length);
  }
  qoiSizes.sort((a, b) => a - b);
  const mid = Math.floor(qoiSizes.length / 2);
  const median = qoiSizes.length
    ? qoiSizes.length % 2
      ? qoiSizes[mid]
      : (qoiSizes[mid - 1] + qoiSizes[mid]) / 2
    : 0;
  const mean = qoiSizes.length
    ? qoiSizes.reduce((a, b) => a + b, 0) / qoiSizes.length
    : 0;

  return {
    count: qoiSizes.length,
    qoi: {
      median,
      min: qoiSizes[0] ?? 0,
      max: qoiSizes[qoiSizes.length - 1] ?? 0,
      mean,
    },
    raw565: { size: THUMB_SIZE * THUMB_SIZE * 2 },
  };
}
