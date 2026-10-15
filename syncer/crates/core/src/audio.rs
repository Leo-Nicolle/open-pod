//! What a source file is, and what the VS1053b can play.

use std::io;

use serde::{Deserialize, Serialize};

/// Container/codec, as far as playback and conversion care.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Codec {
    Mp3,
    Flac,
    Wav,
    Aiff,
    Alac,
    /// WavPack, APE: lossless, decoded by ffmpeg only.
    OtherLossless,
    /// AAC, Vorbis, Opus, WMA, Musepack, Speex: re-encoded to MP3 by ffmpeg.
    OtherLossy,
}

impl Codec {
    pub fn is_lossless(self) -> bool {
        matches!(self, Codec::Flac | Codec::Wav | Codec::Aiff | Codec::Alac | Codec::OtherLossless)
    }

    /// Codecs the pure-Rust pipeline (symphonia) decodes.
    pub fn natively_decodable(self) -> bool {
        matches!(self, Codec::Flac | Codec::Wav | Codec::Aiff | Codec::Alac)
    }
}

/// Audio extensions picked up when scanning a library (lower case, no dot).
pub const SOURCE_EXTS: [&str; 15] =
    ["mp3", "flac", "fla", "wav", "aif", "aiff", "aifc", "m4a", "mp4", "aac", "ogg", "oga", "opus", "wma", "wv"];

/// Best guess from the extension, refined by [`AudioInfo`] once probed.
pub fn codec_from_ext(ext: &str) -> Option<Codec> {
    Some(match ext.to_ascii_lowercase().as_str() {
        "mp3" => Codec::Mp3,
        "flac" | "fla" => Codec::Flac,
        "wav" => Codec::Wav,
        "aif" | "aiff" | "aifc" => Codec::Aiff,
        "wv" | "ape" => Codec::OtherLossless,
        "m4a" | "mp4" | "aac" | "ogg" | "oga" | "opus" | "wma" => Codec::OtherLossy,
        _ => return None,
    })
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct AudioInfo {
    pub codec: Codec,
    pub sample_rate: u32,
    /// Bits per sample for lossless sources, 0 for lossy ones.
    pub bits: u8,
    pub channels: u8,
    /// kbps (average for VBR).
    pub bitrate: u32,
    pub duration: f64,
    /// Highest LPC/fixed predictor order in a FLAC stream (0 otherwise).
    #[serde(default)]
    pub lpc_order: u8,
}

impl AudioInfo {
    /// Ordering used to decide whether a new copy of a track replaces the one
    /// on the card: any lossless beats any lossy, then bitrate.
    pub fn quality_rank(&self) -> u32 {
        if self.codec.is_lossless() { 1_000_000 } else { self.bitrate }
    }
}

/// VS1053b FLAC plugin limits (datasheets/vs1053.pdf, "Supported FLAC
/// Formats"): 48 kHz, 24-bit, stereo. Past LPC order 12 (outside the FLAC
/// streamable subset) it can't decode in real time and stutters.
pub const MAX_RATE: u32 = 48_000;
pub const MAX_BITS: u8 = 24;
pub const MAX_CHANNELS: u8 = 2;
pub const MAX_LPC_ORDER: u8 = 12;
/// `AudioInfo::lpc_order` of a FLAC probed without the LPC scan (see
/// `tags::probe`): resolved by `sync::lpc_needed` when a plan needs it.
pub const LPC_UNKNOWN: u8 = u8::MAX;

/// Highest predictor order in the first `max_frames` FLAC frames, from frame
/// and subframe headers only (port of the former `sync/flac-max-lpc-order.py`). A frame
/// header must pass its CRC-8 and carry the next frame number, since a sync
/// code + valid CRC also shows up by chance inside compressed audio. Returns
/// 0 when the stream can't be parsed.
pub fn flac_max_lpc_order(d: &[u8], max_frames: usize) -> u8 {
    match flac_audio_offset(&mut io::Cursor::new(d)) {
        Ok(Some(at)) if (at as usize) < d.len() => flac_frames_max_lpc_order(&d[at as usize..], max_frames),
        _ => 0,
    }
}

/// Offset of the first FLAC frame, after the (possibly huge, cover art)
/// metadata blocks, which are seeked over rather than read. None when this
/// isn't a FLAC stream.
pub fn flac_audio_offset(r: &mut (impl io::Read + io::Seek)) -> io::Result<Option<u64>> {
    let mut h = [0u8; 10];
    r.read_exact(&mut h[..4])?;
    let mut pos = 0u64;
    if &h[..3] == b"ID3" {
        r.read_exact(&mut h[4..10])?;
        pos = 10 + h[6..10].iter().fold(0u64, |a, &b| (a << 7) | (b & 0x7f) as u64);
        r.seek(io::SeekFrom::Start(pos))?;
        r.read_exact(&mut h[..4])?;
    }
    if &h[..4] != b"fLaC" {
        return Ok(None);
    }
    pos += 4;
    loop {
        if r.read_exact(&mut h[..4]).is_err() {
            return Ok(None);
        }
        pos += 4 + ((h[1] as u64) << 16 | (h[2] as u64) << 8 | h[3] as u64);
        if h[0] & 0x80 != 0 {
            return Ok(Some(pos));
        }
        r.seek(io::SeekFrom::Start(pos))?;
    }
}

/// [`flac_max_lpc_order`] on data starting at the first frame.
pub fn flac_frames_max_lpc_order(d: &[u8], max_frames: usize) -> u8 {
    fn crc8(data: &[u8]) -> u8 {
        let mut c = 0u8;
        for &x in data {
            c ^= x;
            for _ in 0..8 {
                c = if c & 0x80 != 0 { (c << 1) ^ 0x07 } else { c << 1 };
            }
        }
        c
    }

    let mut k = 0;
    let mut best = 0u8;
    let mut frames = 0;
    let mut expected = 0u64;
    while frames < max_frames {
        let Some(off) = d[k..].windows(2).position(|w| w == [0xff, 0xf8]) else { break };
        k += off;
        if k + 16 > d.len() {
            break;
        }
        let block_code = d[k + 2] >> 4;
        let rate_code = d[k + 2] & 0x0f;
        let mut q = k + 4;
        // UTF-8 coded frame number: count the leading ones of the first byte.
        let first = d[q];
        let mut extra = 0u32;
        let mut mask = 0x80u8;
        while first & mask != 0 && extra < 7 {
            extra += 1;
            mask >>= 1;
        }
        let nbytes = 1 + if extra > 1 { extra as usize - 1 } else { 0 };
        let mut number: i64 = if extra != 1 { (first & (0x7f >> extra)) as i64 } else { -1 };
        for &b in &d[q + 1..(q + nbytes).min(d.len())] {
            number = (number << 6) | (b & 0x3f) as i64;
        }
        q += nbytes;
        q += match block_code {
            6 => 1,
            7 => 2,
            _ => 0,
        };
        q += match rate_code {
            12 => 1,
            13 | 14 => 2,
            _ => 0,
        };
        if q + 1 >= d.len() || crc8(&d[k..q]) != d[q] || number != expected as i64 {
            k += 1; // false sync inside audio data
            continue;
        }
        expected += 1;
        let subframe_type = (d[q + 1] >> 1) & 0x3f;
        if subframe_type >= 32 {
            best = best.max((subframe_type & 31) + 1); // LPC
        } else if (8..16).contains(&subframe_type) {
            best = best.max(subframe_type & 7); // fixed
        }
        frames += 1;
        k = q + 1;
    }
    best
}

/// Bytes of FLAC audio (after the metadata) scanned for the LPC order,
/// ~12 s at CD quality. On ~/Music (1121 FLACs, 128 over order 12) 2 MB
/// misses none, 1 MB misses 2; encoders reach their maximum order early,
/// but not in quiet intros.
pub const LPC_SCAN_BYTES: u64 = 2 * 1024 * 1024;
