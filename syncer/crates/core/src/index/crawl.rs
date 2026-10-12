//! Directory crawl + tag reading, ported from `crawl` / `readMetadata` /
//! `resolveAlbumArtSource` in the TS indexer.

use std::fs;
use std::io::{Read, Seek};
use std::path::{Path, PathBuf};

use lofty::file::{AudioFile, TaggedFileExt};
use rayon::prelude::*;

use super::model::TrackRecord;
use crate::tags;

pub const AUDIO_EXTS: [&str; 6] = [".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac"];

/// Node's `path.extname`: from the last dot, "" for dotfiles without one.
fn extname(name: &str) -> &str {
    match name.rfind('.') {
        Some(p) if p > 0 => &name[p..],
        _ => "",
    }
}

/// Audio files under `root`, in the TS crawler's order: a LIFO stack fed by
/// byte-sorted `readdir` (libuv's scandir sorts with strcmp), i.e. a
/// depth-first walk visiting entries in reverse name order. Track and album
/// ids before the alphabetical relabel, and so the search trie layout,
/// depend on this order.
pub fn walk(root: &Path) -> std::io::Result<Vec<PathBuf>> {
    let mut stack = vec![root.to_path_buf()];
    let mut files = Vec::new();
    while let Some(current) = stack.pop() {
        let Ok(meta) = fs::metadata(&current) else { continue };
        if meta.is_dir() {
            let mut entries: Vec<_> =
                fs::read_dir(&current)?.filter_map(|e| e.ok()).map(|e| e.file_name()).collect();
            entries.sort_by(|a, b| a.as_encoded_bytes().cmp(b.as_encoded_bytes()));
            stack.extend(entries.into_iter().map(|n| current.join(n)));
        } else if meta.is_file() {
            let name = current.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
            if AUDIO_EXTS.contains(&extname(&name).to_lowercase().as_str()) {
                files.push(current);
            }
        }
    }
    Ok(files)
}

/// Reads one file's tags; an unreadable file still yields a record (title
/// falls back to the file name, everything else to "Unknown …").
pub fn read_record(root: &Path, path: &Path) -> TrackRecord {
    let file_name = path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    let rel_dir = path
        .parent()
        .and_then(|p| p.strip_prefix(root).ok())
        .map(|p| p.components().map(|c| c.as_os_str().to_string_lossy()).collect::<Vec<_>>().join("/"))
        .unwrap_or_default();
    let mut rec = TrackRecord { card_path: format!("{rel_dir}/{file_name}"), file_name, ..Default::default() };
    match tags::open(path, false) {
        Ok(file) => {
            let (id3v23, mp3_duration) = if file.file_type() == lofty::file::FileType::Mpeg {
                tags::mp3_info(path)
            } else {
                (false, None)
            };
            let t = tags::read_tags(&file, id3v23);
            rec.title = t.title;
            rec.artist = t.artist;
            rec.album_artist = t.album_artist;
            rec.album = t.album;
            rec.genre = t.genre;
            rec.track_no = t.track;
            rec.year = t.year;
            rec.duration_secs = file.properties().duration().as_secs_f64();
            if let Some(d) = mp3_duration {
                rec.duration_secs = d;
            }
        }
        Err(e) => eprintln!("warning: cannot read tags of {}: {e}", path.display()),
    }
    rec
}

/// Reads every file in parallel, keeping the input order.
pub fn read_records(root: &Path, files: &[PathBuf]) -> Vec<TrackRecord> {
    files.par_iter().map(|p| read_record(root, p)).collect()
}

const COVER_BASENAMES: [&str; 4] = ["cover", "folder", "album", "front"];
const COVER_EXTS: [&str; 4] = [".jpg", ".jpeg", ".png", ".webp"];

/// Embedded picture first, then cover/folder/album/front.{jpg,jpeg,png,webp}
/// (case-insensitive) next to the track.
pub fn album_art_source(track: &Path) -> Option<Vec<u8>> {
    if let Some(art) = fs::File::open(track).ok().and_then(|f| embedded_art(&mut std::io::BufReader::new(f), track)) {
        return Some(art);
    }
    let folder = track.parent()?;
    let names: Vec<String> =
        fs::read_dir(folder).ok()?.filter_map(|e| e.ok()).map(|e| e.file_name().to_string_lossy().into_owned()).collect();
    fs::read(folder.join(cover_file(&names)?)).ok()
}

/// The track's embedded picture (music-metadata's common.picture[0]: first
/// picture of the primary tag).
pub fn embedded_art<R: Read + Seek>(r: &mut R, name: &Path) -> Option<Vec<u8>> {
    let file = tags::open_reader(r, name, true).ok()?;
    let p = tags::tag_list(&file).into_iter().flat_map(|t| t.pictures()).next()?;
    (!p.data().is_empty()).then(|| p.data().to_vec())
}

/// Which of a folder's files is its cover image, if any:
/// cover/folder/album/front.{jpg,jpeg,png,webp}, case-insensitive.
pub fn cover_file(names: &[String]) -> Option<&String> {
    let mut names: Vec<&String> = names.iter().collect();
    names.sort();
    for base in COVER_BASENAMES {
        for ext in COVER_EXTS {
            let want = format!("{base}{ext}");
            if let Some(name) = names.iter().find(|n| n.to_lowercase() == want) {
                return Some(name);
            }
        }
    }
    None
}
