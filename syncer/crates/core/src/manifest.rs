//! `openpod/library.json`: every track on the card, where it came from, and
//! the tags the index is built from. Source of truth for sync, rm and index.

use std::collections::BTreeSet;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::audio::AudioInfo;
use crate::index::text::remove_accents;
use crate::tags::{TagQuality, Tags};

pub const VERSION: u32 = 1;
pub const DIR: &str = "openpod";
pub const FILE: &str = "library.json";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Manifest {
    pub version: u32,
    pub tracks: Vec<Entry>,
    /// Keys of tracks removed with `rm`: sync doesn't bring them back.
    #[serde(default)]
    pub removed: BTreeSet<String>,
    /// Library files of removed tracks, so sync skips them without reading.
    #[serde(default)]
    pub removed_sources: Vec<SourceRef>,
    /// What `removed` holds, album by album, for the "Removed" list and
    /// "bring back".
    #[serde(default)]
    pub removed_albums: Vec<RemovedAlbum>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub last_sync: Option<LastSync>,
}

impl Default for Manifest {
    fn default() -> Self {
        Self {
            version: VERSION,
            tracks: Vec::new(),
            removed: BTreeSet::new(),
            removed_sources: Vec::new(),
            removed_albums: Vec::new(),
            last_sync: None,
        }
    }
}

/// Tracks removed together (one album, or the part of it that matched).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemovedAlbum {
    /// [`album_id`] of the album.
    pub id: String,
    pub album: String,
    pub artist: String,
    /// Track keys added to [`Manifest::removed`].
    pub keys: Vec<String>,
    /// Library files added to [`Manifest::removed_sources`].
    pub sources: Vec<SourceRef>,
    /// Unix seconds.
    pub removed_at: u64,
    /// Computer the removal was made from.
    pub machine: String,
}

/// The last sync that wrote to the card.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LastSync {
    pub machine: String,
    /// Unix seconds.
    pub at: u64,
    /// Average card write speed, bytes/s (0 when nothing was written): the
    /// next "What's new" estimates its duration from it.
    pub write_speed: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Entry {
    /// Identity of the recording, see [`track_key`].
    pub key: String,
    /// Card path, '/'-separated, no leading slash.
    pub path: String,
    /// Size of the file on the card.
    pub size: u64,
    pub tags: Tags,
    pub tag_quality: TagQuality,
    /// Properties of the file on the card (after any conversion).
    pub audio: AudioInfo,
    /// Quality of the best source it was made from ([`AudioInfo::quality_rank`]).
    pub source_rank: u32,
    /// What was done to the source ("copied", "96000/24/2 -> 48000/16/2" …).
    pub conversion: String,
    pub sources: Vec<SourceRef>,
    /// Unix seconds.
    pub added_at: u64,
}

/// One library file this track was synced from.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SourceRef {
    pub machine: String,
    /// Absolute path on that machine.
    pub path: String,
    pub size: u64,
    /// Unix nanoseconds.
    pub mtime: i128,
    /// xxh3 of size + first and last 64 KiB, hex.
    pub hash: String,
}

/// Lower case, accents folded, apostrophes dropped, other punctuation as spaces, one leading article
/// removed. Unlike the search key it keeps non-Latin letters, so two
/// Japanese titles don't both normalise to "".
fn key_part(s: &str) -> String {
    let folded = remove_accents(&s.to_lowercase());
    let cleaned: String = folded
        .chars()
        .filter(|c| !matches!(c, '\'' | '’' | '`'))
        .map(|c| if c.is_alphanumeric() { c } else { ' ' })
        .collect();
    let words: Vec<&str> = cleaned.split_whitespace().collect();
    let skip = usize::from(
        words.len() > 1 && ["the", "a", "an", "le", "la", "les", "l", "un", "une", "des"].contains(&words[0]),
    );
    words[skip..].join(" ")
}

/// Recording identity: album artist (or artist), album, disc, track, title,
/// normalised so that case, accents, punctuation and a leading article don't
/// matter.
pub fn track_key(t: &Tags) -> String {
    let n = |s: Option<&str>| key_part(s.unwrap_or(""));
    format!(
        "{}|{}|{}|{}|{}",
        n(t.album_artist.as_deref().or(t.artist.as_deref())),
        n(t.album.as_deref()),
        t.disc.max(1),
        t.track,
        n(t.title.as_deref())
    )
}

/// Album identity in the UI: album artist (or artist) and album, normalised
/// like [`track_key`].
pub fn album_id(t: &Tags) -> String {
    let n = |s: Option<&str>| key_part(s.unwrap_or(""));
    format!("{}|{}", n(t.album_artist.as_deref().or(t.artist.as_deref())), n(t.album.as_deref()))
}

pub fn path(card: &Path) -> PathBuf {
    card.join(DIR).join(FILE)
}

pub fn exists(card: &Path) -> bool {
    path(card).is_file()
}

pub fn load(card: &Path) -> io::Result<Manifest> {
    let data = fs::read(path(card))?;
    let m: Manifest = serde_json::from_slice(&data).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
    if m.version > VERSION {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("library.json version {} is newer than this tool ({VERSION}): update openpod", m.version),
        ));
    }
    Ok(m)
}

/// Atomic: written next to the target, then renamed over it.
pub fn save(card: &Path, m: &Manifest) -> io::Result<()> {
    let target = path(card);
    fs::create_dir_all(target.parent().unwrap())?;
    let tmp = target.with_extension("json.tmp");
    let mut f = fs::File::create(&tmp)?;
    io::Write::write_all(&mut f, &serde_json::to_vec(m).map_err(io::Error::other)?)?;
    f.sync_all()?;
    drop(f);
    fs::rename(&tmp, &target)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn key_ignores_case_accents_articles() {
        let a = Tags {
            title: Some("Été".into()),
            artist: Some("The Band".into()),
            album: Some("Live".into()),
            track: 1,
            ..Default::default()
        };
        let b = Tags { title: Some("ete".into()), artist: Some("band".into()), album: Some("LIVE".into()), ..a.clone() };
        assert_eq!(track_key(&a), track_key(&b));
        let c = Tags { track: 2, ..a.clone() };
        assert_ne!(track_key(&a), track_key(&c));
        let j1 = Tags { title: Some("夜に駆ける".into()), ..a.clone() };
        let j2 = Tags { title: Some("群青".into()), ..a.clone() };
        assert_ne!(track_key(&j1), track_key(&j2));
        let p = Tags { title: Some("Dont Stop".into()), ..a.clone() };
        let q = Tags { title: Some("Don't stop!".into()), ..a };
        assert_eq!(track_key(&p), track_key(&q));
    }
}
