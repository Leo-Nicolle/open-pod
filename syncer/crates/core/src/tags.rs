//! Reading a source file: tags (with a fallback inferred from the folder
//! layout) and audio properties.

use std::fs;
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

use lofty::config::{ParseOptions, ParsingMode};
use lofty::file::{AudioFile, FileType, TaggedFile, TaggedFileExt};
use lofty::probe::Probe;
use lofty::tag::{ItemKey, Tag};
use serde::{Deserialize, Serialize};

use crate::audio::{self, AudioInfo, Codec};

/// Resolved tag values. Raw (unsanitised): the index sanitises them for
/// the ASCII-only firmware fonts, the card paths keep Unicode.
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct Tags {
    pub title: Option<String>,
    pub artist: Option<String>,
    pub album_artist: Option<String>,
    pub album: Option<String>,
    pub genre: Option<String>,
    #[serde(default)]
    pub track: u32,
    #[serde(default)]
    pub disc: u32,
    #[serde(default)]
    pub disc_total: u32,
    #[serde(default)]
    pub year: u32,
}

/// How much of title/artist/album came from real tags.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TagQuality {
    /// Title, artist and album all tagged.
    Tagged,
    /// At least one guessed from the folder layout.
    Inferred,
    /// At least one is still missing ("Unknown …" on the device).
    Unknown,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Probed {
    pub tags: Tags,
    pub quality: TagQuality,
    pub info: AudioInfo,
}

type BoxError = Box<dyn std::error::Error + Send + Sync>;

pub(crate) fn open(path: &Path, covers: bool) -> Result<TaggedFile, BoxError> {
    let mut f = std::io::BufReader::new(fs::File::open(path)?);
    open_reader(&mut f, path, covers)
}

/// Like [`open`], on any reader; `name` gives the extension used to pick
/// the parser (the content decides when it doesn't).
pub fn open_reader<R: Read + Seek>(r: &mut R, name: &Path, covers: bool) -> Result<TaggedFile, BoxError> {
    r.seek(SeekFrom::Start(0))?;
    let options = ParseOptions::new().parsing_mode(ParsingMode::Relaxed).read_cover_art(covers);
    let ext = name.extension().map(|e| e.to_string_lossy().to_lowercase());
    let probe = Probe::new(r).options(options);
    let probe = match ext.as_deref().and_then(FileType::from_ext) {
        Some(t) => probe.set_file_type(t),
        None => probe.guess_file_type()?,
    };
    Ok(probe.read()?)
}

/// Primary tag first, then the others (an MP3's ID3v1 behind its ID3v2).
pub(crate) fn tag_list(file: &TaggedFile) -> Vec<&Tag> {
    let primary = file.primary_tag();
    let mut tags: Vec<&Tag> = primary.into_iter().collect();
    tags.extend(file.tags().iter().filter(|t| Some(t.tag_type()) != primary.map(|p| p.tag_type())));
    tags
}

pub(crate) fn first_string(tags: &[&Tag], key: ItemKey) -> Option<String> {
    tags.iter().find_map(|t| t.get_string(key).map(str::trim).filter(|s| !s.is_empty()).map(str::to_owned))
}

/// All values of a multi-valued item from the first tag that has any.
pub(crate) fn all_strings(tags: &[&Tag], key: ItemKey) -> Option<String> {
    tags.iter().find_map(|t| {
        let v: Vec<&str> = t.get_strings(key).map(str::trim).filter(|s| !s.is_empty()).collect();
        (!v.is_empty()).then(|| v.join(", "))
    })
}

/// "3", "3/12" -> (3, 12)
fn number_pair(s: Option<String>) -> (u32, u32) {
    let Some(s) = s else { return (0, 0) };
    let mut it = s.split('/').map(|p| p.trim().parse().unwrap_or(0));
    (it.next().unwrap_or(0), it.next().unwrap_or(0))
}

/// ID3 genre references ("17", "(17)", "(17)Rock") resolve to their names.
pub(crate) fn resolve_genre(g: String) -> String {
    let t = g.trim();
    let digits = t.strip_prefix('(').and_then(|r| r.split_once(')')).map_or(t, |(n, _)| n);
    match digits.parse::<usize>() {
        Ok(n) if digits.chars().all(|c| c.is_ascii_digit()) => {
            lofty::id3::v1::GENRES.get(n).map_or(g.clone(), |s| (*s).to_owned())
        }
        _ => g,
    }
}

/// Tags as the TS indexer read them (ID3v2.3 genre lists keep their first
/// entry). `file_data` is the raw file for MPEG streams.
pub(crate) fn read_tags(file: &TaggedFile, id3v23: bool) -> Tags {
    let tags = tag_list(file);
    let (track, _) = number_pair(first_string(&tags, ItemKey::TrackNumber));
    let (disc, mut disc_total) = number_pair(first_string(&tags, ItemKey::DiscNumber));
    if disc_total == 0 {
        disc_total = first_string(&tags, ItemKey::DiscTotal).and_then(|s| s.parse().ok()).unwrap_or(0);
    }
    Tags {
        title: first_string(&tags, ItemKey::TrackTitle),
        artist: all_strings(&tags, ItemKey::TrackArtist),
        album_artist: first_string(&tags, ItemKey::AlbumArtist),
        album: first_string(&tags, ItemKey::AlbumTitle),
        genre: first_string(&tags, ItemKey::Genre).and_then(|g| {
            let g = if id3v23 { g.split('/').map(str::trim).find(|s| !s.is_empty())?.to_owned() } else { g };
            Some(resolve_genre(g))
        }),
        track,
        disc,
        disc_total,
        year: first_string(&tags, ItemKey::Year)
            .or_else(|| first_string(&tags, ItemKey::RecordingDate))
            .and_then(|s| s.get(..4)?.parse().ok())
            .unwrap_or(0),
    }
}

/// A podcast episode's (title, date) tags. The date is the fullest one
/// the tags hold: the recording or release date, then the subtitle (TIT3,
/// where Radio France's old files say "Émission du 16.10.2017"), then the
/// year. Podcasts sort by it.
pub fn episode_tags_reader<R: Read + Seek>(r: &mut R, name: &Path) -> (Option<String>, Option<String>) {
    let Ok(file) = open_reader(r, name, false) else { return (None, None) };
    let tags = tag_list(&file);
    let date = [ItemKey::RecordingDate, ItemKey::ReleaseDate, ItemKey::OriginalReleaseDate, ItemKey::TrackSubtitle]
        .into_iter()
        .filter_map(|k| first_string(&tags, k))
        .find(|s| s.len() >= 8)
        .or_else(|| first_string(&tags, ItemKey::Year));
    (first_string(&tags, ItemKey::TrackTitle), date)
}

/// "07 Title", "07 - Title", "1-07 Title", "07. Title" -> (7, "Title")
fn split_track_prefix(stem: &str) -> (u32, &str) {
    let digits_end = stem.find(|c: char| !c.is_ascii_digit()).unwrap_or(stem.len());
    if digits_end == 0 || digits_end > 3 {
        return (0, stem);
    }
    let mut rest = &stem[digits_end..];
    let mut number = stem[..digits_end].parse().unwrap_or(0);
    // disc-track: "1-07"
    if let Some(r) = rest.strip_prefix('-') {
        let d2 = r.find(|c: char| !c.is_ascii_digit()).unwrap_or(r.len());
        if (1..=3).contains(&d2) {
            number = r[..d2].parse().unwrap_or(number);
            rest = &r[d2..];
        }
    }
    let title = rest.trim_start_matches(|c: char| c == ' ' || c == '-' || c == '.' || c == '_').trim();
    if title.is_empty() { (0, stem) } else { (number, title) }
}

/// Fills what the tags lack from an `Artist/Album/NN Title.ext` layout
/// (`rel` is the library-relative path).
pub fn infer_from_path(tags: &mut Tags, rel: &Path) -> TagQuality {
    let comps: Vec<String> = rel.components().map(|c| c.as_os_str().to_string_lossy().into_owned()).collect();
    let stem = rel.file_stem().map(|s| s.to_string_lossy().into_owned()).unwrap_or_default();
    let had_all = tags.title.is_some() && tags.artist.is_some() && tags.album.is_some();

    let (number, title) = split_track_prefix(&stem);
    if tags.title.is_none() {
        tags.title = Some(title.to_owned());
    }
    if tags.track == 0 {
        tags.track = number;
    }
    let n = comps.len();
    if tags.album.is_none() && n >= 2 {
        tags.album = Some(comps[n - 2].clone());
    }
    if tags.artist.is_none() && n >= 3 {
        tags.artist = tags.album_artist.clone().or_else(|| Some(comps[n - 3].clone()));
    }

    if had_all {
        TagQuality::Tagged
    } else if tags.artist.is_some() && tags.album.is_some() {
        TagQuality::Inferred
    } else {
        TagQuality::Unknown
    }
}

fn codec_of(file: &TaggedFile, path: &Path) -> Codec {
    match file.file_type() {
        FileType::Mpeg => Codec::Mp3,
        FileType::Flac => Codec::Flac,
        FileType::Wav => Codec::Wav,
        FileType::Aiff => Codec::Aiff,
        // lofty only reports a bit depth for ALAC inside MP4.
        FileType::Mp4 if file.properties().bit_depth().is_some() => Codec::Alac,
        FileType::Ape | FileType::WavPack => Codec::OtherLossless,
        FileType::Mp4 | FileType::Aac | FileType::Vorbis | FileType::Opus | FileType::Speex | FileType::Mpc => {
            Codec::OtherLossy
        }
        _ => path.extension().and_then(|e| audio::codec_from_ext(&e.to_string_lossy())).unwrap_or(Codec::OtherLossy),
    }
}

/// (ID3v2.3 tag, duration) of an MP3. Reads the head only: the duration
/// comes from a Xing/VBRI header or, for CBR, the bitrate. Only VBR files
/// without a header are read in full, which matters on slow SD cards.
pub(crate) fn mp3_info(path: &Path) -> (bool, Option<f64>) {
    match fs::File::open(path) {
        Ok(f) => mp3_info_reader(&mut std::io::BufReader::new(f)),
        Err(_) => (false, None),
    }
}

pub(crate) fn mp3_info_reader<R: Read + Seek>(f: &mut R) -> (bool, Option<f64>) {
    use crate::index::mp3;
    if f.seek(SeekFrom::Start(0)).is_err() {
        return (false, None);
    }
    let mut head = [0u8; 10];
    let n = f.read(&mut head).unwrap_or(0);
    let v23 = n >= 4 && head.starts_with(b"ID3") && head[3] == 3;
    let mut frame = Vec::new();
    let tag_len = mp3::id3v2_len(&head[..n]) as u64;
    if f.seek(SeekFrom::Start(tag_len)).is_ok() {
        let _ = (&mut *f).take(16 * 1024).read_to_end(&mut frame);
    }
    if let Some(d) = mp3::header_duration(&frame) {
        return (v23, Some(d));
    }
    // A CBR podcast episode is tens of MB: its bitrate gives the length.
    if let Some(d) = f.seek(SeekFrom::End(0)).ok().and_then(|size| mp3::cbr_duration(&frame, size.saturating_sub(tag_len))) {
        return (v23, Some(d));
    }
    let mut all = Vec::new();
    let full = f.seek(SeekFrom::Start(0)).is_ok() && f.read_to_end(&mut all).is_ok();
    (v23, if full { mp3::duration(&all) } else { None })
}

/// A FLAC's LPC order alone (what [`probe`] skips without `lpc`); None
/// when the file can't be read.
pub fn lpc_order(path: &Path) -> Option<u8> {
    let f = fs::File::open(path).ok()?;
    lpc_order_reader(&mut std::io::BufReader::with_capacity(64 * 1024, f))
}

/// [`lpc_order`] on any reader.
pub fn lpc_order_reader<R: Read + Seek>(r: &mut R) -> Option<u8> {
    flac_lpc_order(r).ok()
}

/// Highest LPC order in the first [`audio::LPC_SCAN_BYTES`] of audio,
/// skipping the metadata blocks (cover art can be megabytes).
fn flac_lpc_order<R: Read + Seek>(f: &mut R) -> std::io::Result<u8> {
    f.seek(SeekFrom::Start(0))?;
    let Some(at) = audio::flac_audio_offset(f)? else { return Ok(0) };
    f.seek(SeekFrom::Start(at))?;
    let mut frames = Vec::new();
    f.take(audio::LPC_SCAN_BYTES).read_to_end(&mut frames)?;
    Ok(audio::flac_frames_max_lpc_order(&frames, usize::MAX))
}

/// Rough bytes [`probe`] reads from a file of this size, to weigh progress:
/// FLACs get the LPC scan (when `lpc`), other formats little more than their tags.
pub fn probe_cost(path: &Path, size: u64, lpc: bool) -> u64 {
    let ext = path.extension().map(|e| e.to_string_lossy().to_lowercase()).unwrap_or_default();
    let cost = if ext == "flac" && lpc { audio::LPC_SCAN_BYTES } else { 256 * 1024 };
    size.min(cost)
}

/// Reads tags and audio properties. `rel` (library-relative path) feeds the
/// tag fallback. Files lofty can't parse (WMA) get an extension-based codec,
/// path-inferred tags and zeroed properties.
///
/// `lpc` scans a FLAC's first [`audio::LPC_SCAN_BYTES`] of audio for its
/// LPC order (`info.lpc_order`, which decides copy vs re-encode in
/// [`crate::policy::decide`]). It is most of the cost of probing a FLAC
/// (~2/3 of reading a card), so reading the card and planning skip it: the
/// FLAC gets [`audio::LPC_UNKNOWN`], and [`crate::sync::lpc_needed`] lists
/// the few files whose order the plan needs, read with [`lpc_order`].
pub fn probe(path: &Path, rel: &Path, lpc: bool) -> Probed {
    match fs::File::open(path) {
        // 64 KiB: lofty seeks block to block through a FLAC's metadata, and
        // each seek past the buffer is a round trip to the card.
        Ok(f) => probe_reader(&mut std::io::BufReader::with_capacity(64 * 1024, f), path, rel, lpc),
        Err(_) => probe_failed(path, rel),
    }
}

fn blank_info(name: &Path) -> AudioInfo {
    AudioInfo {
        codec: name.extension().and_then(|e| audio::codec_from_ext(&e.to_string_lossy())).unwrap_or(Codec::OtherLossy),
        sample_rate: 0,
        bits: 0,
        channels: 0,
        bitrate: 0,
        duration: 0.0,
        lpc_order: 0,
    }
}

fn probe_failed(name: &Path, rel: &Path) -> Probed {
    let mut tags = Tags::default();
    let quality = infer_from_path(&mut tags, rel);
    Probed { tags, quality, info: blank_info(name) }
}

/// [`probe`] on any reader; `name` gives the extension.
pub fn probe_reader<R: Read + Seek>(r: &mut R, name: &Path, rel: &Path, lpc: bool) -> Probed {
    let mut info = blank_info(name);
    let mut tags = Tags::default();
    if let Ok(file) = open_reader(r, name, false) {
        let p = file.properties();
        info.codec = codec_of(&file, name);
        info.sample_rate = p.sample_rate().unwrap_or(0);
        info.bits = if info.codec.is_lossless() { p.bit_depth().unwrap_or(16) } else { 0 };
        info.channels = p.channels().unwrap_or(2);
        info.bitrate = p.audio_bitrate().or(p.overall_bitrate()).unwrap_or(0);
        info.duration = p.duration().as_secs_f64();
        let mut id3v23 = false;
        if info.codec == Codec::Mp3 {
            let (v23, d) = mp3_info_reader(r);
            id3v23 = v23;
            if let Some(d) = d {
                info.duration = d;
            }
        }
        if info.codec == Codec::Flac {
            info.lpc_order = if lpc { flac_lpc_order(r).unwrap_or(0) } else { audio::LPC_UNKNOWN };
        }
        tags = read_tags(&file, id3v23);
    }
    let quality = infer_from_path(&mut tags, rel);
    Probed { tags, quality, info }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn track_prefix() {
        assert_eq!(split_track_prefix("07 Title"), (7, "Title"));
        assert_eq!(split_track_prefix("07 - Title"), (7, "Title"));
        assert_eq!(split_track_prefix("1-07 Title"), (7, "Title"));
        assert_eq!(split_track_prefix("2001"), (0, "2001"));
        assert_eq!(split_track_prefix("Title"), (0, "Title"));
    }

    #[test]
    fn inference() {
        let mut t = Tags::default();
        let q = infer_from_path(&mut t, Path::new("Yoa/La Favorite/02 Sad girl.flac"));
        assert_eq!(q, TagQuality::Inferred);
        assert_eq!(t.artist.as_deref(), Some("Yoa"));
        assert_eq!(t.album.as_deref(), Some("La Favorite"));
        assert_eq!(t.title.as_deref(), Some("Sad girl"));
        assert_eq!(t.track, 2);

        let mut t = Tags::default();
        assert_eq!(infer_from_path(&mut t, Path::new("loose.mp3")), TagQuality::Unknown);
        assert_eq!(t.title.as_deref(), Some("loose"));

        let mut t = Tags { title: Some("x".into()), artist: Some("a".into()), album: Some("b".into()), ..Default::default() };
        assert_eq!(infer_from_path(&mut t, Path::new("q/w/e.mp3")), TagQuality::Tagged);
    }
}
