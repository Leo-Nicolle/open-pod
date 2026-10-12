//! Conversions. Lossless sources go through a streaming Rust pipeline
//! (symphonia decode -> downmix -> rubato resample -> TPDF dither -> flacenc,
//! LPC order 10, inside the FLAC subset), so a hi-res album never sits
//! decoded in memory. Formats symphonia lacks fall back to ffmpeg.

use std::fs::File;
use std::io::{Cursor, Read, Seek, SeekFrom};
use std::path::Path;
use std::process::Command;

use flacenc::component::BitRepr;
use flacenc::error::Verify;
use flacenc::error::SourceError;
use flacenc::source::{Fill, Source};
use lofty::config::WriteOptions;
use lofty::file::TaggedFileExt;
use lofty::ogg::tag::VorbisComments;
use lofty::tag::TagExt;
use rubato::audioadapter_buffers::direct::InterleavedSlice;
use rubato::{Fft, FixedSync, Indexing, Resampler};
use symphonia::core::codecs::audio::{AudioDecoder, AudioDecoderOptions};
use symphonia::core::errors::Error as SymError;
use symphonia::core::formats::probe::Hint;
use symphonia::core::formats::{FormatOptions, FormatReader, TrackType};
use symphonia::core::io::{MediaSource, MediaSourceStream};
use symphonia::core::meta::MetadataOptions;

use crate::policy::{Policy, Target};

pub type Error = Box<dyn std::error::Error + Send + Sync>;

const RESAMPLER_CHUNK: usize = 1024;

/// Small xorshift for TPDF dither (deterministic, so re-runs are identical).
struct Rng(u64);
impl Rng {
    fn unit(&mut self) -> f64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 >> 11) as f64 / (1u64 << 53) as f64
    }
}

/// Decoded, converted PCM, pulled block by block by the FLAC encoder.
struct PcmStream {
    format: Box<dyn FormatReader>,
    decoder: Box<dyn AudioDecoder>,
    track_id: u32,
    in_channels: usize,
    target: Target,
    /// Requantising to fewer bits than the source: dither.
    dither: bool,
    rng: Rng,
    decoded: Vec<f64>,
    resampler: Option<Fft<f64>>,
    rs_in: Vec<f64>,
    rs_out: Vec<f64>,
    /// Resampler latency still to drop from the output, in frames.
    delay_left: usize,
    in_frames: u64,
    out_frames: u64,
    pending: Vec<i32>,
    pending_pos: usize,
    finished: bool,
}

impl PcmStream {
    fn open(source: Box<dyn MediaSource>, name: &Path, target: Target) -> Result<Self, Error> {
        let mss = MediaSourceStream::new(source, Default::default());
        let mut hint = Hint::new();
        if let Some(ext) = name.extension() {
            hint.with_extension(&ext.to_string_lossy());
        }
        let format = symphonia::default::get_probe().probe(
            &hint,
            mss,
            FormatOptions::default(),
            MetadataOptions::default(),
        )?;
        let track = format.default_track(TrackType::Audio).ok_or("no audio track")?;
        let params = track.codec_params.as_ref().and_then(|p| p.audio()).ok_or("not an audio track")?.clone();
        let decoder = symphonia::default::get_codecs().make_audio_decoder(&params, &AudioDecoderOptions::default())?;
        let in_rate = params.sample_rate.ok_or("unknown sample rate")?;
        let in_channels = params.channels.as_ref().map_or(2, |c| c.count());
        let in_bits = params.bits_per_sample.unwrap_or(16);
        let out_channels = target.channels as usize;
        let resampler = (in_rate != target.rate)
            .then(|| {
                Fft::<f64>::new(in_rate as usize, target.rate as usize, RESAMPLER_CHUNK, out_channels, FixedSync::Input)
            })
            .transpose()?;
        let delay_left = resampler.as_ref().map_or(0, |r| r.output_delay());
        Ok(Self {
            track_id: track.id,
            format,
            decoder,
            in_channels,
            target,
            dither: in_bits > target.bits as u32,
            rng: Rng(0x9e37_79b9_7f4a_7c15),
            decoded: Vec::new(),
            resampler,
            rs_in: Vec::new(),
            rs_out: Vec::new(),
            delay_left,
            in_frames: 0,
            out_frames: 0,
            pending: Vec::new(),
            pending_pos: 0,
            finished: false,
        })
    }

    /// Downmix `decoded` (in_channels, interleaved) to the output layout.
    /// More than two channels are assumed to be in WAV order (FL FR FC LFE
    /// BL BR SL SR); centre and surrounds fold in at -3 dB, LFE is dropped.
    fn downmix(&self, out: &mut Vec<f64>) {
        let (inc, outc) = (self.in_channels, self.target.channels as usize);
        if inc == outc {
            out.extend_from_slice(&self.decoded);
            return;
        }
        const K: f64 = std::f64::consts::FRAC_1_SQRT_2;
        for f in self.decoded.chunks_exact(inc) {
            match (inc, outc) {
                (1, 2) => out.extend_from_slice(&[f[0], f[0]]),
                (_, 1) => out.push(f.iter().sum::<f64>() / inc as f64),
                _ => {
                    let g = |i: usize| f.get(i).copied().unwrap_or(0.0);
                    let l = g(0) + K * g(2) + K * g(4) + K * g(6);
                    let r = g(1) + K * g(2) + K * g(5) + K * g(7);
                    let norm = 1.0 + K * (1 + inc.saturating_sub(4).min(4) / 2) as f64;
                    out.push(l / norm);
                    out.push(r / norm);
                }
            }
        }
    }

    fn quantize(&mut self, samples: &[f64]) {
        let scale = (1i64 << (self.target.bits - 1)) as f64;
        let (lo, hi) = (-scale, scale - 1.0);
        for &x in samples {
            let mut v = x * scale;
            if self.dither {
                v += self.rng.unit() - self.rng.unit();
            }
            self.pending.push(v.round().clamp(lo, hi) as i32);
        }
    }

    /// Feeds frames out of the resampler: drops its latency, and once the
    /// input has ended stops at `expected` frames.
    fn emit_resampled(&mut self, frames: usize, expected: Option<u64>) {
        let ch = self.target.channels as usize;
        let mut start = 0;
        if self.delay_left > 0 {
            start = self.delay_left.min(frames);
            self.delay_left -= start;
        }
        let mut end = frames;
        if let Some(exp) = expected {
            end = end.min(start + exp.saturating_sub(self.out_frames) as usize);
        }
        if end > start {
            self.out_frames += (end - start) as u64;
            let chunk = self.rs_out[start * ch..end * ch].to_vec();
            self.quantize(&chunk);
        }
    }

    fn run_resampler(&mut self, partial: Option<usize>, expected: Option<u64>) -> Result<(), Error> {
        let ch = self.target.channels as usize;
        let Some(rs) = self.resampler.as_mut() else { return Ok(()) };
        let need = rs.input_frames_next();
        let max_out = rs.output_frames_max();
        let have = self.rs_in.len() / ch;
        let take = partial.unwrap_or(need).min(have);
        let mut input = self.rs_in[..take * ch].to_vec();
        input.resize(need * ch, 0.0);
        self.rs_out.resize(max_out * ch, 0.0);
        let indexing = Indexing { partial_len: partial.map(|p| p.min(have)), ..Default::default() };
        let (_, produced) = rs.process_into_buffer(
            &InterleavedSlice::new(&input, ch, need)?,
            &mut InterleavedSlice::new_mut(&mut self.rs_out, ch, max_out)?,
            Some(&indexing),
        )?;
        self.rs_in.drain(..take * ch);
        self.emit_resampled(produced, expected);
        Ok(())
    }

    /// Decodes one more packet (or finishes the stream).
    fn pump(&mut self) -> Result<(), Error> {
        let packet = match self.format.next_packet() {
            Ok(Some(p)) => p,
            Ok(None) => return self.finish(),
            Err(SymError::IoError(e)) if e.kind() == std::io::ErrorKind::UnexpectedEof => return self.finish(),
            Err(e) => return Err(e.into()),
        };
        if packet.track_id != self.track_id {
            return Ok(());
        }
        let buf = match self.decoder.decode(&packet) {
            Ok(b) => b,
            Err(SymError::DecodeError(_)) => return Ok(()), // skip a corrupt packet
            Err(e) => return Err(e.into()),
        };
        self.decoded.resize(buf.samples_interleaved(), 0.0);
        buf.copy_to_slice_interleaved(&mut self.decoded);
        self.in_frames += (self.decoded.len() / self.in_channels.max(1)) as u64;
        let mut mixed = Vec::with_capacity(self.decoded.len());
        self.downmix(&mut mixed);
        if self.resampler.is_none() {
            self.quantize(&mixed);
            return Ok(());
        }
        self.rs_in.extend_from_slice(&mixed);
        let ch = self.target.channels as usize;
        while self.rs_in.len() / ch >= self.resampler.as_ref().unwrap().input_frames_next() {
            self.run_resampler(None, None)?;
        }
        Ok(())
    }

    fn finish(&mut self) -> Result<(), Error> {
        if let Some(rs) = &self.resampler {
            let ratio = rs.resample_ratio();
            let expected = (self.in_frames as f64 * ratio).round() as u64;
            let ch = self.target.channels as usize;
            let left = self.rs_in.len() / ch;
            self.run_resampler(Some(left), Some(expected))?;
            // Flush the filter tail with silence until every frame is out.
            let mut guard = 0;
            while self.out_frames < expected && guard < 64 {
                self.run_resampler(Some(0), Some(expected))?;
                guard += 1;
            }
        }
        self.finished = true;
        Ok(())
    }
}

impl Source for PcmStream {
    fn channels(&self) -> usize {
        self.target.channels as usize
    }
    fn bits_per_sample(&self) -> usize {
        self.target.bits as usize
    }
    fn sample_rate(&self) -> usize {
        self.target.rate as usize
    }
    fn read_samples<F: Fill>(&mut self, block_size: usize, dest: &mut F) -> Result<usize, SourceError> {
        let ch = self.target.channels as usize;
        while self.pending.len() - self.pending_pos < block_size * ch && !self.finished {
            if let Err(e) = self.pump() {
                eprintln!("warning: decode error, stream truncated: {e}");
                self.finished = true;
            }
        }
        let avail = (self.pending.len() - self.pending_pos) / ch;
        let frames = avail.min(block_size);
        let slice = &self.pending[self.pending_pos..self.pending_pos + frames * ch];
        dest.fill_interleaved(slice)?;
        self.pending_pos += frames * ch;
        if self.pending_pos > 1 << 20 {
            self.pending.drain(..self.pending_pos);
            self.pending_pos = 0;
        }
        Ok(frames)
    }
}

/// A seekable reader as a symphonia source.
struct Seekable<R>(R, Option<u64>);

impl<R: Read> Read for Seekable<R> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.0.read(buf)
    }
}

impl<R: Seek> Seek for Seekable<R> {
    fn seek(&mut self, pos: SeekFrom) -> std::io::Result<u64> {
        self.0.seek(pos)
    }
}

impl<R: Read + Seek + Send + Sync> MediaSource for Seekable<R> {
    fn is_seekable(&self) -> bool {
        true
    }
    fn byte_len(&self) -> Option<u64> {
        self.1
    }
}

/// Encodes `src` to a FLAC at `dst` in the target format, tags included.
pub fn to_flac(src: &Path, dst: &Path, target: Target) -> Result<(), Error> {
    let file = File::open(src)?;
    let flac = to_flac_bytes(std::io::BufReader::new(file), src, target)?;
    std::fs::write(dst, flac)?;
    Ok(())
}

/// [`to_flac`] in memory, from any reader (`name` gives the extension):
/// the browser build writes the result itself.
pub fn to_flac_bytes<R: Read + Seek + Send + Sync + 'static>(mut src: R, name: &Path, target: Target) -> Result<Vec<u8>, Error> {
    // Tags and pictures first, then the same reader feeds the decoder.
    let tags: Option<VorbisComments> = crate::tags::open_reader(&mut src, name, true)
        .ok()
        .and_then(|f| f.primary_tag().or_else(|| f.first_tag()).cloned())
        .map(Into::into);
    let len = src.seek(SeekFrom::End(0)).ok();
    src.seek(SeekFrom::Start(0))?;
    let stream = PcmStream::open(Box::new(Seekable(src, len)), name, target)?;
    let mut config = flacenc::config::Encoder::default();
    config.multithread = false; // files are converted in parallel already
    let config = config.into_verified().map_err(|(_, e)| format!("flac config: {e:?}"))?;
    let encoded = flacenc::encode_with_fixed_block_size(&config, stream, config.block_size)
        .map_err(|e| format!("flac encode: {e:?}"))?;
    let mut sink = flacenc::bitsink::ByteSink::new();
    encoded.write(&mut sink).map_err(|e| format!("flac write: {e:?}"))?;
    let mut out = Cursor::new(sink.as_slice().to_vec());
    if let Some(vc) = tags {
        vc.save_to(&mut out, WriteOptions::default())?;
    }
    Ok(out.into_inner())
}

pub fn ffmpeg_available() -> bool {
    Command::new("ffmpeg").arg("-version").output().is_ok_and(|o| o.status.success())
}

fn ffmpeg(src: &Path, dst: &Path, codec_args: &[String]) -> Result<(), Error> {
    let mut cmd = Command::new("ffmpeg");
    cmd.args(["-nostdin", "-y", "-loglevel", "error", "-i"]).arg(src);
    cmd.args(["-map", "0:a:0", "-map", "0:v:0?", "-map_metadata", "0", "-c:v", "copy"]);
    cmd.args(["-disposition:v:0", "attached_pic"]).args(codec_args).arg(dst);
    let out = cmd.output()?;
    if !out.status.success() {
        return Err(format!("ffmpeg: {}", String::from_utf8_lossy(&out.stderr).trim()).into());
    }
    Ok(())
}

/// Runs the conversion `policy` asks for, writing the result to `dst`.
pub fn run(policy: &Policy, src: &Path, dst: &Path) -> Result<(), Error> {
    match policy {
        Policy::Copy => {
            std::fs::copy(src, dst)?;
            Ok(())
        }
        Policy::Flac(t) => to_flac(src, dst, *t),
        Policy::FfmpegFlac(t) => ffmpeg(
            src,
            dst,
            &[
                "-c:a".into(),
                "flac".into(),
                "-compression_level".into(),
                "8".into(),
                "-ar".into(),
                t.rate.to_string(),
                "-ac".into(),
                t.channels.to_string(),
                "-sample_fmt".into(),
                if t.bits <= 16 { "s16".into() } else { "s32".into() },
            ],
        ),
        Policy::FfmpegMp3 => ffmpeg(
            src,
            dst,
            &["-c:a".into(), "libmp3lame".into(), "-q:a".into(), "0".into(), "-id3v2_version".into(), "3".into()],
        ),
        Policy::Unsupported(reason) => Err(reason.clone().into()),
    }
}

/// Human-readable record of a conversion, for the manifest and report.
pub fn describe(policy: &Policy, src: &crate::audio::AudioInfo) -> String {
    let fmt = |r: u32, b: u8, c: u8| format!("{r}/{b}/{c}");
    match policy {
        Policy::Copy => "copied".into(),
        Policy::Flac(t) | Policy::FfmpegFlac(t)
            if src.codec == crate::audio::Codec::Flac
                && (t.rate, t.bits, t.channels) == (src.sample_rate, src.bits, src.channels) =>
        {
            format!("lpc order {} -> <= 12 (lossless re-encode)", src.lpc_order)
        }
        Policy::Flac(t) | Policy::FfmpegFlac(t) => format!(
            "{:?} {} -> flac {}",
            src.codec,
            fmt(src.sample_rate, src.bits, src.channels),
            fmt(t.rate, t.bits, t.channels)
        )
        .to_lowercase(),
        Policy::FfmpegMp3 => format!("{:?} {} kbps -> mp3 v0", src.codec, src.bitrate).to_lowercase(),
        Policy::Unsupported(r) => format!("skipped: {r}"),
    }
}
