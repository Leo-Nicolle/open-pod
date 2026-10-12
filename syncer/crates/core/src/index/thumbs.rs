//! 160×160 album-art thumbnails (`thumbs.bin` + `album_to_cover.bin`),
//! ported from `indexer/src/thumbnails.ts`.
//!
//! Firmware contract: raw565 blobs are little-endian RGB565, row-major, no
//! header; QOI blobs are standard 3-channel QOI with values pre-snapped to
//! 565 levels. The resize is `image`'s Lanczos3 rather than sharp's, so pixels
//! differ slightly from the TS output; hashes, sizes and layout do not.

use std::io::Cursor;

use image::imageops::FilterType;
use image::{DynamicImage, ImageDecoder, ImageReader};
use serde::Serialize;
use sha2::{Digest, Sha256};

pub const THUMB_SIZE: u32 = 160;
/// Bump when resize / quantisation / format assumptions change.
pub const PIPELINE_VERSION: u32 = 2;
const PLACEHOLDER_RGB: [u8; 3] = [64, 64, 72];

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum Format {
    Qoi,
    #[default]
    Raw565,
}

impl Format {
    fn name(self) -> &'static str {
        match self {
            Format::Qoi => "qoi",
            Format::Raw565 => "raw565",
        }
    }
    /// `album_cover_entry_t.format` in software/src/storage/Music_lookup.h.
    fn byte(self) -> u8 {
        match self {
            Format::Qoi => 0,
            Format::Raw565 => 1,
        }
    }
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Config {
    pub format: Format,
    pub dither: bool,
}

#[derive(Debug, Clone)]
pub struct Thumbnail {
    pub bytes: Vec<u8>,
    pub src_hash: String,
    pub is_placeholder: bool,
    /// Why the source art couldn't be used, if it couldn't.
    pub warning: Option<String>,
}

pub fn src_hash(source: Option<&[u8]>, config: Config) -> String {
    let mut h = Sha256::new();
    h.update(PIPELINE_VERSION.to_string());
    h.update(config.format.name());
    h.update(if config.dither { "dither1" } else { "dither0" });
    h.update(source.unwrap_or(b"placeholder"));
    hex::encode(h.finalize())
}

fn decode_and_normalize(source: &[u8]) -> image::ImageResult<Vec<u8>> {
    let mut decoder = ImageReader::new(Cursor::new(source)).with_guessed_format()?.into_decoder()?;
    let orientation = decoder.orientation()?;
    let mut img = DynamicImage::from_decoder(decoder)?;
    img.apply_orientation(orientation);
    let img = img.resize_to_fill(THUMB_SIZE, THUMB_SIZE, FilterType::Lanczos3);
    let rgba = img.to_rgba8();
    // Flatten onto black.
    let mut rgb = Vec::with_capacity((THUMB_SIZE * THUMB_SIZE * 3) as usize);
    for p in rgba.pixels() {
        let a = p[3] as u32;
        for c in &p.0[..3] {
            rgb.push(((*c as u32 * a + 127) / 255) as u8);
        }
    }
    Ok(rgb)
}

fn placeholder_rgb() -> Vec<u8> {
    PLACEHOLDER_RGB.repeat((THUMB_SIZE * THUMB_SIZE) as usize)
}

const BAYER_4X4: [[u8; 4]; 4] = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];

/// JS Math.round (half towards +inf).
fn js_round(x: f64) -> f64 {
    (x + 0.5).floor()
}

fn quantize_channel(value: u8, levels: u32, bias: Option<f64>) -> u8 {
    let step = 256.0 / levels as f64;
    let biased = match bias {
        Some(b) => value as f64 + (b - 0.5) * step,
        None => value as f64,
    };
    let level = js_round(biased / step).clamp(0.0, (levels - 1) as f64);
    (level * step) as u8
}

pub fn quantize_565(rgb: &mut [u8], width: u32, dither: bool) {
    for (i, px) in rgb.chunks_exact_mut(3).enumerate() {
        let (x, y) = (i as u32 % width, i as u32 / width);
        let bias = dither.then(|| (BAYER_4X4[(y % 4) as usize][(x % 4) as usize] as f64 + 0.5) / 16.0);
        px[0] = quantize_channel(px[0], 32, bias);
        px[1] = quantize_channel(px[1], 64, bias);
        px[2] = quantize_channel(px[2], 32, bias);
    }
}

pub fn encode_raw565(rgb: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(rgb.len() / 3 * 2);
    for p in rgb.chunks_exact(3) {
        let packed = ((p[0] as u16 & 0xf8) << 8) | ((p[1] as u16 & 0xfc) << 3) | (p[2] as u16 >> 3);
        out.extend_from_slice(&packed.to_le_bytes());
    }
    out
}

/// QOI encoder, RGB only (qoiformat.org), same op choices as the TS one.
pub fn encode_qoi(px: &[u8], width: u32, height: u32) -> Vec<u8> {
    let count = (width * height) as usize;
    let mut out = Vec::with_capacity(14 + count * 4 + 8);
    out.extend_from_slice(b"qoif");
    out.extend_from_slice(&width.to_be_bytes());
    out.extend_from_slice(&height.to_be_bytes());
    out.extend_from_slice(&[3, 0]);
    let mut index = [[0u8; 3]; 64];
    let mut prev = [0u8; 3];
    let mut run = 0u8;
    let wrap = |x: i32| ((x + 128) & 0xff) - 128;
    for i in 0..count {
        let p = [px[i * 3], px[i * 3 + 1], px[i * 3 + 2]];
        if p == prev {
            run += 1;
            if run == 62 || i == count - 1 {
                out.push(0xc0 | (run - 1));
                run = 0;
            }
            continue;
        }
        if run > 0 {
            out.push(0xc0 | (run - 1));
            run = 0;
        }
        let hash = ((p[0] as usize * 3 + p[1] as usize * 5 + p[2] as usize * 7 + 255 * 11) % 64) as u8;
        if index[hash as usize] == p {
            out.push(hash);
        } else {
            index[hash as usize] = p;
            let dr = wrap(p[0] as i32 - prev[0] as i32);
            let dg = wrap(p[1] as i32 - prev[1] as i32);
            let db = wrap(p[2] as i32 - prev[2] as i32);
            if (-2..=1).contains(&dr) && (-2..=1).contains(&dg) && (-2..=1).contains(&db) {
                out.push(0x40 | (((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2)) as u8);
            } else {
                let dr_dg = wrap(dr - dg);
                let db_dg = wrap(db - dg);
                if (-32..=31).contains(&dg) && (-8..=7).contains(&dr_dg) && (-8..=7).contains(&db_dg) {
                    out.push(0x80 | (dg + 32) as u8);
                    out.push((((dr_dg + 8) << 4) | (db_dg + 8)) as u8);
                } else {
                    out.extend_from_slice(&[0xfe, p[0], p[1], p[2]]);
                }
            }
        }
        prev = p;
    }
    out.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 1]);
    out
}

/// Thumbnail for one album; a missing or undecodable source gets the shared
/// placeholder (which packs down to a single blob). `track` names the
/// art's file in the warning.
pub fn generate(source: Option<&[u8]>, config: Config, track: &str) -> Thumbnail {
    let mut warning = None;
    let decoded = source.and_then(|s| match decode_and_normalize(s) {
        Ok(rgb) => Some(rgb),
        Err(e) => {
            warning = Some(format!("cover art of {track} can't be read ({e}); the album gets the placeholder"));
            None
        }
    });
    let is_placeholder = decoded.is_none();
    let mut rgb = decoded.unwrap_or_else(placeholder_rgb);
    quantize_565(&mut rgb, THUMB_SIZE, config.dither);
    let bytes = match config.format {
        Format::Qoi => encode_qoi(&rgb, THUMB_SIZE, THUMB_SIZE),
        Format::Raw565 => encode_raw565(&rgb),
    };
    let hash_source = if is_placeholder { None } else { source };
    Thumbnail { bytes, src_hash: src_hash(hash_source, config), is_placeholder, warning }
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PackedEntry {
    pub album_id: u32,
    pub offset: u32,
    pub length: u32,
    pub format: Format,
    pub w: u32,
    pub h: u32,
    pub src_hash: String,
    pub is_placeholder: bool,
}

pub struct Packed {
    pub blob: Vec<u8>,
    pub entries: Vec<PackedEntry>,
}

/// Concatenates blobs (album id order); identical `src_hash` is stored once.
pub fn pack(thumbs: &[Thumbnail], format: Format) -> Packed {
    let mut blob = Vec::new();
    let mut by_hash = std::collections::HashMap::new();
    let mut entries = Vec::with_capacity(thumbs.len());
    for (album_id, t) in thumbs.iter().enumerate() {
        let (offset, length) = *by_hash.entry(t.src_hash.clone()).or_insert_with(|| {
            let loc = (blob.len() as u32, t.bytes.len() as u32);
            blob.extend_from_slice(&t.bytes);
            loc
        });
        entries.push(PackedEntry {
            album_id: album_id as u32,
            offset,
            length,
            format,
            w: THUMB_SIZE,
            h: THUMB_SIZE,
            src_hash: t.src_hash.clone(),
            is_placeholder: t.is_placeholder,
        });
    }
    Packed { blob, entries }
}

/// [u32 count][{u32 offset, u32 length, u8 format}]*count by album id.
pub fn cover_index(entries: &[PackedEntry]) -> Vec<u8> {
    let mut out = Vec::with_capacity(4 + entries.len() * 9);
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    for e in entries {
        out.extend_from_slice(&e.offset.to_le_bytes());
        out.extend_from_slice(&e.length.to_le_bytes());
        out.push(e.format.byte());
    }
    out
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Manifest<'a> {
    pub pipeline_version: u32,
    pub entries: &'a [PackedEntry],
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quantize_matches_ts() {
        assert_eq!(quantize_channel(255, 32, None), 248);
        assert_eq!(quantize_channel(4, 32, None), 8);
        assert_eq!(quantize_channel(3, 32, None), 0);
        assert_eq!(quantize_channel(255, 64, None), 252);
    }

    #[test]
    fn placeholder_hash_is_stable() {
        let a = generate(None, Config::default(), "a");
        let b = generate(Some(b"not an image"), Config::default(), "b");
        assert!(a.is_placeholder && b.is_placeholder);
        assert_eq!(a.src_hash, b.src_hash);
        assert_eq!(a.bytes.len(), (THUMB_SIZE * THUMB_SIZE * 2) as usize);
    }

    #[test]
    fn qoi_roundtrip_header() {
        let rgb = placeholder_rgb();
        let q = encode_qoi(&rgb, THUMB_SIZE, THUMB_SIZE);
        assert_eq!(&q[..4], b"qoif");
        assert_eq!(&q[q.len() - 8..], &[0, 0, 0, 0, 0, 0, 0, 1]);
    }
}
