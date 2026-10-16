//! `index.pack` / `podcasts.pack`: every lookup table in one file, read by
//! `software/src/storage/PageStore.cpp`. The firmware doesn't copy it to
//! PSRAM at boot: it reads fixed-size pages of the file on demand and keeps
//! the recent ones in a PSRAM cache, so the library size is bounded by the
//! card, not by PSRAM.
//!
//! Layout (little-endian):
//! `"OPIX" | u16 version | u16 0 | u32 count | {u32 id, u32 offset, u32 length}*count`,
//! then the sections, each starting on a [`ALIGN`]-byte boundary. Section
//! ids are the constants in [`section`]; keep them in sync with
//! `software/src/storage/PackSections.h`.

pub const MAGIC: [u8; 4] = *b"OPIX";
pub const VERSION: u16 = 2;
pub const ALIGN: usize = 16;

pub const MUSIC_FILE: &str = "index.pack";
pub const PODCAST_FILE: &str = "podcasts.pack";

/// Section ids. Same numbers in `software/src/storage/PackSections.h`.
pub mod section {
    // Music (index.pack)
    pub const ARTIST_NAMES: u32 = 1;
    pub const ALBUM_NAMES: u32 = 2;
    pub const GENRE_NAMES: u32 = 3;
    pub const TRACK_NAMES: u32 = 4;
    pub const PATHS: u32 = 5;
    pub const DURATIONS: u32 = 6;
    pub const TRACK_TO_ALBUM: u32 = 7;
    pub const TRACK_TO_ARTIST: u32 = 8;
    pub const ALBUM_TO_ARTIST: u32 = 9;
    pub const ALBUM_TO_COVER: u32 = 10;
    pub const ARTIST_TO_ALBUMS: u32 = 11;
    pub const ARTIST_TO_TRACKS: u32 = 12;
    pub const ALBUM_TO_TRACKS: u32 = 13;
    pub const GENRE_TO_ALBUMS: u32 = 14;
    pub const GENRE_TO_TRACKS: u32 = 15;
    pub const GENRE_TO_ARTISTS: u32 = 16;
    pub const SEARCH_TRIE: u32 = 17;
    /// Letter-start tables (see `binary::letter_table`): `LETTERS + list * 4
    /// + letters`, list 0..4 = artists, albums, genres, tracks, letters 1..=3.
    pub const LETTERS: u32 = 32;

    // Podcasts (podcasts.pack)
    pub const POD_FOLDER_NAMES: u32 = 64;
    pub const POD_FOLDER_PARENT: u32 = 65;
    pub const POD_FOLDER_FLAGS: u32 = 66;
    pub const POD_FOLDER_SPAN: u32 = 67;
    pub const POD_FOLDER_CHILDREN: u32 = 68;
    pub const POD_FOLDER_EPISODES: u32 = 69;
    pub const POD_EPISODE_TITLES: u32 = 70;
    pub const POD_EPISODE_PATHS: u32 = 71;
    pub const POD_EPISODE_DURATIONS: u32 = 72;
    pub const POD_EPISODE_FOLDER: u32 = 73;
    pub const POD_EPISODE_DATE: u32 = 74;
    pub const POD_EPISODE_NUMBER: u32 = 75;
    pub const POD_FOLDER_TO_COVER: u32 = 76;

    pub const fn letters(list: u32, letters: u32) -> u32 {
        LETTERS + list * 4 + letters
    }
}

fn align(n: usize) -> usize {
    n.div_ceil(ALIGN) * ALIGN
}

/// Packs `sections` (id, bytes) in the given order.
pub fn pack(sections: &[(u32, Vec<u8>)]) -> Vec<u8> {
    let header = 12 + sections.len() * 12;
    let mut offset = align(header);
    let mut table = Vec::with_capacity(sections.len());
    for (id, bytes) in sections {
        table.push((*id, offset as u32, bytes.len() as u32));
        offset = align(offset + bytes.len());
    }
    let mut out = Vec::with_capacity(offset);
    out.extend_from_slice(&MAGIC);
    out.extend_from_slice(&VERSION.to_le_bytes());
    out.extend_from_slice(&0u16.to_le_bytes());
    out.extend_from_slice(&(sections.len() as u32).to_le_bytes());
    for (id, off, len) in &table {
        out.extend_from_slice(&id.to_le_bytes());
        out.extend_from_slice(&off.to_le_bytes());
        out.extend_from_slice(&len.to_le_bytes());
    }
    for ((_, bytes), (_, off, _)) in sections.iter().zip(&table) {
        out.resize(*off as usize, 0);
        out.extend_from_slice(bytes);
    }
    out.resize(align(out.len()), 0);
    out
}

/// The sections of a pack as (id, bytes), or None if it isn't one.
pub fn unpack(bytes: &[u8]) -> Option<Vec<(u32, &[u8])>> {
    let u32_at = |i: usize| bytes.get(i..i + 4).map(|b| u32::from_le_bytes(b.try_into().unwrap()));
    if bytes.get(0..4)? != MAGIC || u16::from_le_bytes(bytes.get(4..6)?.try_into().ok()?) != VERSION {
        return None;
    }
    let count = u32_at(8)? as usize;
    (0..count)
        .map(|i| {
            let e = 12 + i * 12;
            let (id, off, len) = (u32_at(e)?, u32_at(e + 4)? as usize, u32_at(e + 8)? as usize);
            Some((id, bytes.get(off..off + len)?))
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_and_alignment() {
        let sections = vec![(1, vec![1, 2, 3]), (17, vec![]), (5, (0..100u8).collect())];
        let p = pack(&sections);
        assert_eq!(p.len() % ALIGN, 0);
        let back = unpack(&p).unwrap();
        assert_eq!(back.len(), 3);
        for ((id, bytes), (bid, bbytes)) in sections.iter().zip(&back) {
            assert_eq!(id, bid);
            assert_eq!(bytes.as_slice(), *bbytes);
        }
        // Each section starts aligned.
        for i in 0..3 {
            let off = u32::from_le_bytes(p[12 + i * 12 + 4..12 + i * 12 + 8].try_into().unwrap());
            assert_eq!(off as usize % ALIGN, 0);
        }
    }

    #[test]
    fn rejects_other_files() {
        assert!(unpack(b"nope").is_none());
        let mut p = pack(&[(1, vec![1])]);
        p[4] = 1; // version 1
        assert!(unpack(&p).is_none());
    }
}
