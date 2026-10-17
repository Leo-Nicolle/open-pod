//! Port of the TS indexer (`indexer/`): builds the `opentone/index.pack`
//! (every lookup table and the search trie, see [`pack`]) and `thumbs.bin`
//! the firmware reads.

pub mod binary;
pub mod crawl;
pub mod model;
pub mod mp3;
pub mod pack;
pub mod synthetic;
pub mod text;
pub mod thumbs;
pub mod trie;

use std::fs;
use std::path::Path;

use rayon::prelude::*;

pub use model::{AlbumKey, CrawlIndex, TrackRecord};

#[derive(Debug, Clone, Copy, Default)]
pub struct Options {
    pub album_key: AlbumKey,
    pub thumbs: thumbs::Config,
}

#[derive(Debug, Default)]
pub struct Summary {
    pub tracks: usize,
    pub artists: usize,
    pub albums: usize,
    pub genres: usize,
    pub placeholders: usize,
    /// Covers that couldn't be read.
    pub warnings: Vec<String>,
}

/// Writes every index file for `records` into `out_dir`. `art_source(i)`
/// returns the cover bytes for `records[i]` (called once per album, in
/// parallel).
pub fn write_index<F>(records: &[TrackRecord], out_dir: &Path, opts: Options, art_source: F) -> std::io::Result<Summary>
where
    F: Fn(usize) -> Option<Vec<u8>> + Sync,
{
    let (files, summary) = build_index(records, opts, art_source)?;
    for (name, bytes) in files {
        write_fresh(&out_dir.join(name), &bytes)?;
    }
    remove_legacy_files(out_dir);
    Ok(summary)
}

/// The per-table `.bin` files that `index.pack` replaced. The firmware no
/// longer reads them; removed so a card doesn't carry two indexes.
pub const LEGACY_FILES: [&str; 17] = [
    "music_index.bin", "artist_index.bin", "album_index.bin", "genre_index.bin", "track_index.bin",
    "path_index.bin", "duration_index.bin", "track_to_album.bin", "track_to_artist.bin",
    "album_to_artist.bin", "album_to_cover.bin", "artist_to_albums.bin", "artist_to_tracks.bin",
    "album_to_tracks.bin", "genre_to_albums.bin", "genre_to_tracks.bin", "genre_to_artists.bin",
];

/// Writes a new file next to `path`, then renames it over it. Rewriting in
/// place reuses the old file's clusters first and spills the rest elsewhere:
/// a fragmented `index.pack`, which the firmware reads much slower (every
/// seek walks the FAT). Written whole while the old one still exists, the
/// new one usually gets one contiguous run.
fn write_fresh(path: &Path, bytes: &[u8]) -> std::io::Result<()> {
    let tmp = path.with_extension("tmp");
    fs::write(&tmp, bytes)?;
    fs::rename(&tmp, path)
}

fn remove_legacy_files(dir: &Path) {
    for name in LEGACY_FILES {
        let _ = fs::remove_file(dir.join(name));
    }
}

/// The index files as (file name, bytes), for [`write_index`] or a caller
/// that writes them itself (the browser build).
pub fn build_index<F>(records: &[TrackRecord], opts: Options, art_source: F) -> std::io::Result<(Vec<(String, Vec<u8>)>, Summary)>
where
    F: Fn(usize) -> Option<Vec<u8>> + Sync,
{
    let ix = model::build(records, opts.album_key);

    let thumbnails: Vec<thumbs::Thumbnail> = ix
        .album_art_track
        .par_iter()
        .map(|&t| thumbs::generate(art_source(t).as_deref(), opts.thumbs, &records[t].card_path))
        .collect();
    let packed = thumbs::pack(&thumbnails, opts.thumbs.format);

    let mut sections = binary::all(&ix);
    sections.push((pack::section::ALBUM_TO_COVER, thumbs::cover_index(&packed.entries)));
    sections.push((pack::section::SEARCH_TRIE, trie::export(&ix)));

    let mut files: Vec<(String, Vec<u8>)> = Vec::new();
    files.push(("thumbs.bin".into(), packed.blob.clone()));
    let manifest = thumbs::Manifest { pipeline_version: thumbs::PIPELINE_VERSION, entries: &packed.entries };
    files.push(("thumbs.manifest.json".into(), serde_json::to_string_pretty(&manifest)?.into_bytes()));
    files.push((pack::MUSIC_FILE.into(), pack::pack(&sections)));

    let summary = Summary {
        tracks: ix.tracks.len(),
        artists: ix.artists.len(),
        albums: ix.albums.len(),
        genres: ix.genres.len(),
        placeholders: thumbnails.iter().filter(|t| t.is_placeholder).count(),
        warnings: thumbnails.iter().filter_map(|t| t.warning.clone()).collect(),
    };
    Ok((files, summary))
}

/// Crawls a music folder laid out as it will be on the card and indexes it
/// (the TS `generate.ts` flow).
pub fn index_folder(music_root: &Path, out_dir: &Path, opts: Options) -> std::io::Result<Summary> {
    let files = crawl::walk(music_root)?;
    let records = crawl::read_records(music_root, &files);
    write_index(&records, out_dir, opts, |i| crawl::album_art_source(&files[i]))
}
