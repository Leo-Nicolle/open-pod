//! Lookup tables read by `software/src/storage/Music_lookup_load.cpp`,
//! ported from `indexer/src/music-indexes.ts`. All integers little-endian.

use super::model::{CrawlIndex, Relation};

fn put(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

/// [u32 count][u32 stringBytes][u32 id]*count [u32 offset]*(count+1)
/// [NUL-terminated UTF-8 strings]
pub fn string_index(strings: &[String]) -> Vec<u8> {
    let data_len: usize = strings.iter().map(|s| s.len() + 1).sum();
    let mut out = Vec::with_capacity(8 + strings.len() * 8 + 4 + data_len);
    put(&mut out, strings.len() as u32);
    put(&mut out, data_len as u32);
    for id in 0..strings.len() as u32 {
        put(&mut out, id);
    }
    let mut offset = 0u32;
    put(&mut out, 0);
    for s in strings {
        offset += s.len() as u32 + 1;
        put(&mut out, offset);
    }
    for s in strings {
        out.extend_from_slice(s.as_bytes());
        out.push(0);
    }
    out
}

/// [u32 sourceCount][u32 targetTotal][u32 sourceId]* [u32 targetCount]*
/// [u32 targetId]* — sources and each source's targets ascending.
pub fn relation(rel: &Relation) -> Vec<u8> {
    let total: usize = rel.values().map(|t| t.len()).sum();
    let mut out = Vec::with_capacity(8 + rel.len() * 8 + total * 4);
    put(&mut out, rel.len() as u32);
    put(&mut out, total as u32);
    for &src in rel.keys() {
        put(&mut out, src);
    }
    for targets in rel.values() {
        put(&mut out, targets.len() as u32);
    }
    for &t in rel.values().flatten() {
        put(&mut out, t);
    }
    out
}

/// [u32 count][u32 value]*count, indexed by dense id.
pub fn dense_u32(values: &[u32]) -> Vec<u8> {
    let mut out = Vec::with_capacity(4 + values.len() * 4);
    put(&mut out, values.len() as u32);
    for &v in values {
        put(&mut out, v);
    }
    out
}

/// [u32 count][u16 seconds]*count, indexed by track id.
pub fn durations(secs: &[f64]) -> Vec<u8> {
    let mut out = Vec::with_capacity(4 + secs.len() * 2);
    put(&mut out, secs.len() as u32);
    for &s in secs {
        // JS Math.round: half rounds up (towards +inf).
        let rounded = (s + 0.5).floor().clamp(0.0, 65535.0) as u16;
        out.extend_from_slice(&rounded.to_le_bytes());
    }
    out
}

/// Every lookup table, keyed by file stem (`<name>.bin`).
pub fn all(ix: &CrawlIndex) -> Vec<(&'static str, Vec<u8>)> {
    vec![
        ("genre_to_tracks", relation(&ix.genre_to_tracks)),
        ("genre_to_artists", relation(&ix.genre_to_artists)),
        ("genre_to_albums", relation(&ix.genre_to_albums)),
        ("artist_to_tracks", relation(&ix.artist_to_tracks)),
        ("artist_to_albums", relation(&ix.artist_to_albums)),
        ("album_to_tracks", relation(&ix.album_to_tracks)),
        ("artist_index", string_index(&ix.artists)),
        ("album_index", string_index(&ix.albums)),
        ("genre_index", string_index(&ix.genres)),
        ("track_index", string_index(&ix.tracks)),
        ("path_index", string_index(&ix.paths)),
        ("duration_index", durations(&ix.durations)),
        ("track_to_album", dense_u32(&ix.track_to_album)),
        ("track_to_artist", dense_u32(&ix.track_to_artist)),
        ("album_to_artist", dense_u32(&ix.album_to_artist())),
    ]
}
