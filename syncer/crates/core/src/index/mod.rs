//! Port of the TS indexer (`indexer/`): builds the `openpod/*.bin` files the
//! firmware reads.

pub mod binary;
pub mod crawl;
pub mod model;
pub mod mp3;
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
        fs::write(out_dir.join(name), bytes)?;
    }
    Ok(summary)
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

    let mut files: Vec<(String, Vec<u8>)> = Vec::new();
    files.push(("thumbs.bin".into(), packed.blob.clone()));
    files.push(("album_to_cover.bin".into(), thumbs::cover_index(&packed.entries)));
    let manifest = thumbs::Manifest { pipeline_version: thumbs::PIPELINE_VERSION, entries: &packed.entries };
    files.push(("thumbs.manifest.json".into(), serde_json::to_string_pretty(&manifest)?.into_bytes()));
    files.push(("music_index.bin".into(), trie::export(&ix)));
    for (name, bytes) in binary::all(&ix) {
        files.push((format!("{name}.bin"), bytes));
    }

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
