//! MP3 duration: from a Xing/Info/VBRI header, else estimated from the
//! bitrate for CBR files (what lofty and ffprobe report, within a second),
//! else exact by walking every frame header.

const BITRATES_V1: [[u32; 16]; 3] = [
    [0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0], // Layer I
    [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0],    // Layer II
    [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0],     // Layer III
];
const BITRATES_V2: [[u32; 16]; 3] = [
    [0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0], // Layer I
    [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0],      // Layer II & III
    [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0],
];

struct Frame {
    len: usize,
    samples: u32,
    rate: u32,
    kbps: u32,
}

fn parse_header(h: &[u8]) -> Option<Frame> {
    if h.len() < 4 || h[0] != 0xff || h[1] & 0xe0 != 0xe0 {
        return None;
    }
    let version = (h[1] >> 3) & 3; // 0 = 2.5, 2 = 2, 3 = 1
    let layer = (h[1] >> 1) & 3; // 1 = III, 2 = II, 3 = I
    let br_idx = (h[2] >> 4) as usize;
    let sr_idx = ((h[2] >> 2) & 3) as usize;
    let pad = ((h[2] >> 1) & 1) as usize;
    if version == 1 || layer == 0 || br_idx == 0 || br_idx == 15 || sr_idx == 3 {
        return None;
    }
    let rate = [44100, 48000, 32000][sr_idx] >> match version {
        3 => 0,
        2 => 1,
        _ => 2,
    };
    let layer_ix = (3 - layer) as usize; // 0 = I, 1 = II, 2 = III
    let kbps = if version == 3 { BITRATES_V1[layer_ix][br_idx] } else { BITRATES_V2[layer_ix][br_idx] };
    let br = kbps as usize * 1000;
    let (len, samples) = match layer_ix {
        0 => ((12 * br / rate as usize + pad) * 4, 384),
        1 => (144 * br / rate as usize + pad, 1152),
        _ if version == 3 => (144 * br / rate as usize + pad, 1152),
        _ => (72 * br / rate as usize + pad, 576),
    };
    (len >= 4).then_some(Frame { len, samples, rate, kbps })
}

/// Size of a leading ID3v2 tag (0 when there is none); needs the first 10 bytes.
pub fn id3v2_len(d: &[u8]) -> usize {
    if d.len() < 10 || &d[..3] != b"ID3" {
        return 0;
    }
    let size = d[6..10].iter().fold(0usize, |acc, &b| (acc << 7) | (b & 0x7f) as usize);
    let footer = if d[5] & 0x10 != 0 { 10 } else { 0 };
    10 + size + footer
}

/// Frame count from a Xing/Info/VBRI header in the first frame.
fn header_frames(data: &[u8], pos: usize, f: &Frame) -> Option<u64> {
    let frame = &data[pos..(pos + f.len).min(data.len())];
    for tag in [&b"Xing"[..], b"Info"] {
        if let Some(at) = frame.windows(4).position(|w| w == tag) {
            let flags = frame.get(at + 4..at + 8).map(|b| u32::from_be_bytes(b.try_into().unwrap()));
            if flags.is_some_and(|fl| fl & 1 != 0) {
                let n = frame.get(at + 8..at + 12)?;
                return Some(u32::from_be_bytes(n.try_into().unwrap()) as u64);
            }
        }
    }
    let n = frame.get(36 + 14..36 + 18).filter(|_| frame.get(36..40) == Some(b"VBRI"))?;
    Some(u32::from_be_bytes(n.try_into().unwrap()) as u64)
}

/// Duration from the Xing/Info/VBRI header alone. `data` starts right after
/// the ID3v2 tag; None when the first frame has no such header (the whole
/// file is then needed, see [`duration`]).
pub fn header_duration(data: &[u8]) -> Option<f64> {
    let mut pos = 0;
    while pos + 4 <= data.len() {
        if let Some(f) = parse_header(&data[pos..]) {
            return header_frames(data, pos, &f).map(|n| (n * f.samples as u64) as f64 / f.rate as f64);
        }
        pos += 1;
    }
    None
}

/// Frames walked by [`cbr_duration`] before trusting a constant bitrate.
const CBR_FRAMES: usize = 8;

/// Duration of a CBR stream of `audio_len` bytes from its bitrate, when the
/// first frames of `data` (starting right after the ID3v2 tag) all share
/// one bitrate. Off by at most a trailing tag's worth (an ID3v1 tag is 8 ms
/// at 128 kbps), fine for mm:ss. None when they differ: a VBR stream
/// without a header, which needs [`duration`].
pub fn cbr_duration(data: &[u8], audio_len: u64) -> Option<f64> {
    let mut pos = (0..data.len()).find(|&i| parse_header(&data[i..]).is_some())?;
    let first = parse_header(&data[pos..])?;
    for _ in 0..CBR_FRAMES {
        let f = parse_header(data.get(pos..)?)?;
        if f.kbps != first.kbps || f.rate != first.rate {
            return None;
        }
        pos += f.len;
    }
    Some(audio_len as f64 * 8.0 / (first.kbps as f64 * 1000.0))
}

/// Duration in seconds, or None when no frames are found.
pub fn duration(data: &[u8]) -> Option<f64> {
    let mut pos = id3v2_len(data);
    let mut samples = 0u64;
    let mut rate = 0u32;
    let mut first = true;
    while pos + 4 <= data.len() {
        let Some(f) = parse_header(&data[pos..]) else {
            if data[pos..].starts_with(b"TAG") || data[pos..].starts_with(b"APETAGEX") {
                break;
            }
            pos += 1; // junk between frames: resync
            continue;
        };
        if first {
            first = false;
            rate = f.rate;
            if let Some(n) = header_frames(data, pos, &f) {
                return Some((n * f.samples as u64) as f64 / rate as f64);
            }
        }
        samples += f.samples as u64;
        pos += f.len;
    }
    (rate > 0).then(|| samples as f64 / rate as f64)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// MPEG-1 Layer III, 128 kbps, 44.1 kHz, no padding: 417-byte frames.
    fn cbr(frames: usize) -> Vec<u8> {
        let mut frame = vec![0u8; 417];
        frame[..4].copy_from_slice(&[0xff, 0xfb, 0x90, 0x00]);
        let mut d = b"ID3\x03\x00\x00\x00\x00\x00\x05xxxxx".to_vec();
        for _ in 0..frames {
            d.extend_from_slice(&frame);
        }
        d.extend_from_slice(b"TAG");
        d.resize(d.len() + 125, 0);
        d
    }

    #[test]
    fn counts_frames() {
        let secs = duration(&cbr(1000)).unwrap();
        assert!((secs - 1000.0 * 1152.0 / 44100.0).abs() < 1e-9);
    }

    #[test]
    fn estimates_cbr() {
        let d = cbr(1000);
        let secs = cbr_duration(&d[15..], 1000 * 417).unwrap();
        assert!((secs - 1000.0 * 1152.0 / 44100.0).abs() < 0.1);
        let mut vbr = d[15..].to_vec();
        vbr[417 * 3 + 2] = 0xa0; // 4th frame at 160 kbps
        assert_eq!(cbr_duration(&vbr, 1000 * 417), None);
    }

    #[test]
    fn uses_xing_frame_count() {
        let mut d = cbr(3);
        let first = 15; // after the 15-byte ID3v2 tag
        d[first + 36..first + 40].copy_from_slice(b"Xing");
        d[first + 40..first + 44].copy_from_slice(&1u32.to_be_bytes());
        d[first + 44..first + 48].copy_from_slice(&5000u32.to_be_bytes());
        let secs = duration(&d).unwrap();
        assert!((secs - 5000.0 * 1152.0 / 44100.0).abs() < 1e-9);
    }
}
