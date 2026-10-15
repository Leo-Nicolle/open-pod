//! What to do with each source file so the VS1053b can play it.

use crate::audio::{self, AudioInfo, Codec, MAX_BITS, MAX_CHANNELS, MAX_LPC_ORDER, MAX_RATE};

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct Target {
    pub rate: u32,
    pub bits: u8,
    pub channels: u8,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Policy {
    /// Plays as is.
    Copy,
    /// Decode + (downmix/resample/requantise) + FLAC encode, in Rust: the
    /// fallback without ffmpeg (and the browser build).
    Flac(Target),
    /// The same with ffmpeg, ~6x faster than the Rust encoder; the only way
    /// for lossless formats symphonia can't decode (WavPack, APE).
    FfmpegFlac(Target),
    /// Lossy formats other than MP3: ffmpeg to MP3 V0.
    FfmpegMp3,
    Unsupported(String),
}

impl Policy {
    /// Extension of the file written to the card.
    pub fn output_ext<'a>(&self, source_ext: &'a str) -> &'a str {
        match self {
            Policy::Copy => source_ext,
            Policy::Flac(_) | Policy::FfmpegFlac(_) => "flac",
            Policy::FfmpegMp3 => "mp3",
            Policy::Unsupported(_) => source_ext,
        }
    }
}

fn within_limits(i: &AudioInfo) -> bool {
    i.sample_rate <= MAX_RATE && i.bits <= MAX_BITS && i.channels <= MAX_CHANNELS
}

/// Out of limits: 44.1 kHz family -> 44.1 kHz, others -> 48 kHz; 16-bit;
/// at most stereo (same output as the former sync/convert-flac.sh, except that it kept
/// 48 kHz for 88.2/176.4 kHz sources).
fn playable_target(i: &AudioInfo) -> Target {
    let rate = if i.sample_rate <= MAX_RATE {
        i.sample_rate
    } else if i.sample_rate % 44_100 == 0 {
        44_100
    } else {
        MAX_RATE
    };
    Target { rate, bits: 16, channels: i.channels.clamp(1, MAX_CHANNELS) }
}

fn same(i: &AudioInfo) -> Target {
    Target { rate: i.sample_rate, bits: i.bits.max(8), channels: i.channels }
}

/// Whether [`decide`] on this FLAC hangs on its unknown LPC order (copy if
/// it is <= [`MAX_LPC_ORDER`], else re-encode); out of limits it is
/// re-encoded anyway.
pub fn needs_lpc(i: &AudioInfo) -> bool {
    i.codec == Codec::Flac && i.lpc_order == audio::LPC_UNKNOWN && within_limits(i)
}

pub fn decide(i: &AudioInfo, have_ffmpeg: bool) -> Policy {
    if i.codec != Codec::OtherLossy && i.codec != Codec::Mp3 && i.sample_rate == 0 {
        return Policy::Unsupported("unreadable audio properties".into());
    }
    match i.codec {
        Codec::Mp3 => Policy::Copy,
        Codec::Flac if within_limits(i) && i.lpc_order <= MAX_LPC_ORDER => Policy::Copy,
        // Past the FLAC subset or the limits, or not FLAC: encoded to FLAC,
        // by ffmpeg when there.
        Codec::Flac | Codec::Wav | Codec::Aiff | Codec::Alac | Codec::OtherLossless if have_ffmpeg => {
            Policy::FfmpegFlac(if within_limits(i) { same(i) } else { playable_target(i) })
        }
        Codec::Flac | Codec::Wav | Codec::Aiff | Codec::Alac => {
            Policy::Flac(if within_limits(i) { same(i) } else { playable_target(i) })
        }
        Codec::OtherLossy if have_ffmpeg => Policy::FfmpegMp3,
        Codec::OtherLossless | Codec::OtherLossy => Policy::Unsupported("needs ffmpeg (not found on PATH)".into()),
    }
}

/// Rough size of the output, for the free-space check.
pub fn estimated_size(p: &Policy, i: &AudioInfo, source_size: u64) -> u64 {
    match p {
        Policy::Copy | Policy::Unsupported(_) => source_size,
        Policy::Flac(t) | Policy::FfmpegFlac(t) => {
            // Lossless sizes scale with the PCM rate; FLAC compresses ~60%.
            let pcm = |r: u32, b: u8, c: u8| r as f64 * b as f64 * c as f64;
            let src_pcm = pcm(i.sample_rate.max(1), i.bits.max(8), i.channels.max(1));
            let ratio = pcm(t.rate, t.bits, t.channels) / src_pcm;
            let base = if i.codec == Codec::Flac { source_size as f64 } else { source_size as f64 * 0.6 };
            (base * ratio.min(1.0)) as u64
        }
        Policy::FfmpegMp3 => (i.duration * 245_000.0 / 8.0) as u64,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn info(codec: Codec, rate: u32, bits: u8, ch: u8, lpc: u8) -> AudioInfo {
        AudioInfo { codec, sample_rate: rate, bits, channels: ch, bitrate: 0, duration: 1.0, lpc_order: lpc }
    }

    #[test]
    fn table() {
        use Codec::*;
        let t = |rate, bits, channels| Target { rate, bits, channels };
        assert_eq!(decide(&info(Mp3, 44100, 0, 2, 0), false), Policy::Copy);
        assert_eq!(decide(&info(Flac, 44100, 16, 2, 8), false), Policy::Copy);
        assert_eq!(decide(&info(Flac, 48000, 24, 2, 12), false), Policy::Copy);
        // LPC past the subset: lossless re-encode.
        assert_eq!(decide(&info(Flac, 44100, 16, 2, 32), false), Policy::Flac(t(44100, 16, 2)));
        // Hi-res.
        assert_eq!(decide(&info(Flac, 96000, 24, 2, 8), false), Policy::Flac(t(48000, 16, 2)));
        assert_eq!(decide(&info(Flac, 88200, 24, 2, 8), false), Policy::Flac(t(44100, 16, 2)));
        assert_eq!(decide(&info(Flac, 192000, 24, 2, 8), false), Policy::Flac(t(48000, 16, 2)));
        assert_eq!(decide(&info(Flac, 44100, 32, 2, 8), false), Policy::Flac(t(44100, 16, 2)));
        assert_eq!(decide(&info(Flac, 48000, 24, 6, 8), false), Policy::Flac(t(48000, 16, 2)));
        // Uncompressed / ALAC always become FLAC.
        assert_eq!(decide(&info(Wav, 44100, 16, 2, 0), false), Policy::Flac(t(44100, 16, 2)));
        assert_eq!(decide(&info(Alac, 96000, 24, 2, 0), false), Policy::Flac(t(48000, 16, 2)));
        // ffmpeg-only formats.
        assert_eq!(decide(&info(OtherLossy, 44100, 0, 2, 0), true), Policy::FfmpegMp3);
        assert!(matches!(decide(&info(OtherLossy, 44100, 0, 2, 0), false), Policy::Unsupported(_)));
        assert_eq!(decide(&info(OtherLossless, 44100, 16, 2, 0), true), Policy::FfmpegFlac(t(44100, 16, 2)));
        // ffmpeg, when there, encodes every FLAC.
        assert_eq!(decide(&info(Flac, 44100, 16, 2, 8), true), Policy::Copy);
        assert_eq!(decide(&info(Flac, 44100, 16, 2, 32), true), Policy::FfmpegFlac(t(44100, 16, 2)));
        assert_eq!(decide(&info(Flac, 96000, 24, 2, 8), true), Policy::FfmpegFlac(t(48000, 16, 2)));
        assert_eq!(decide(&info(Wav, 44100, 16, 2, 0), true), Policy::FfmpegFlac(t(44100, 16, 2)));
    }
}
