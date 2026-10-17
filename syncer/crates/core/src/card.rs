//! What a mounted card holds, before any sync: the states the app's setup
//! screen shows and the CLI checks.

use std::fs;
use std::io;
use std::path::Path;

use serde::Serialize;

use crate::audio;
use crate::manifest::{self, LastSync};

#[derive(Debug, Clone, Serialize)]
#[serde(tag = "kind", rename_all = "camelCase")]
pub enum CardState {
    /// Nothing at that path, or an empty folder that isn't a mounted volume.
    Missing,
    /// Mounted but not writable (lock switch, permissions).
    Locked { reason: String },
    /// No opentone manifest and no music: the first sync sets it up.
    Empty,
    /// Music but no manifest: it has to be read once (`library::adopt`).
    Unread { files: usize, bytes: u64 },
    #[serde(rename_all = "camelCase")]
    Ready { tracks: usize, albums: usize, last_sync: Option<LastSync> },
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardInfo {
    pub path: String,
    /// Volume label, else the folder name.
    pub name: String,
    pub capacity: u64,
    pub free: u64,
    pub state: CardState,
}

/// Free bytes on the card's volume (0 when unknown).
pub fn free_space(card: &Path) -> u64 {
    fs4::available_space(card).unwrap_or(0)
}

/// Total bytes of the card's volume (0 when unknown).
pub fn capacity(card: &Path) -> u64 {
    fs4::total_space(card).unwrap_or(0)
}

/// Tries to create and delete a file at the card's root.
pub fn check_writable(card: &Path) -> io::Result<()> {
    let probe = card.join(".opentone-write-test");
    fs::write(&probe, b"")?;
    fs::remove_file(probe)
}

/// Audio files outside `opentone/` (count, bytes): what an unread card holds.
fn count_audio(card: &Path) -> (usize, u64) {
    let (mut n, mut bytes) = (0, 0);
    let mut stack = vec![card.to_path_buf()];
    while let Some(dir) = stack.pop() {
        let Ok(entries) = fs::read_dir(&dir) else { continue };
        for e in entries.flatten() {
            let name = e.file_name();
            let name = name.to_string_lossy();
            if name.starts_with('.') || (dir == card && (name == manifest::DIR || name == "System Volume Information")) {
                continue;
            }
            let Ok(t) = e.file_type() else { continue };
            if t.is_dir() {
                stack.push(e.path());
            } else if e
                .path()
                .extension()
                .is_some_and(|x| audio::SOURCE_EXTS.contains(&x.to_string_lossy().to_lowercase().as_str()))
            {
                n += 1;
                bytes += e.metadata().map_or(0, |m| m.len());
            }
        }
    }
    (n, bytes)
}

/// Inspects a card. `mounted` says the path is a mounted volume (detected
/// by the caller), so an empty folder there is a blank card rather than a
/// mount point with nothing mounted.
pub fn inspect(card: &Path, name: Option<String>, mounted: bool) -> CardInfo {
    let name = name.filter(|n| !n.is_empty()).unwrap_or_else(|| {
        card.file_name().map_or_else(|| card.to_string_lossy().into_owned(), |n| n.to_string_lossy().to_uppercase())
    });
    let mut info = CardInfo {
        path: card.to_string_lossy().into_owned(),
        name,
        capacity: capacity(card),
        free: free_space(card),
        state: CardState::Missing,
    };
    let empty = fs::read_dir(card).map(|mut d| d.next().is_none());
    info.state = match empty {
        Err(_) => CardState::Missing,
        Ok(true) if !mounted => CardState::Missing,
        _ => match check_writable(card) {
            Err(e) => CardState::Locked { reason: e.to_string() },
            Ok(()) if manifest::exists(card) => match manifest::load(card) {
                Ok(m) => {
                    let albums: std::collections::HashSet<String> =
                        m.tracks.iter().map(|e| manifest::album_id(&e.tags)).collect();
                    CardState::Ready { tracks: m.tracks.len(), albums: albums.len(), last_sync: m.last_sync }
                }
                Err(e) => CardState::Locked { reason: format!("opentone/library.json can't be read: {e}") },
            },
            Ok(()) => match count_audio(card) {
                (0, _) => CardState::Empty,
                (files, bytes) => CardState::Unread { files, bytes },
            },
        },
    };
    info
}
