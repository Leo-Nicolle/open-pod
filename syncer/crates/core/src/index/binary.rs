//! Lookup tables read by `software/src/storage/Music_lookup.cpp`, one
//! section each of `index.pack` (see [`super::pack`]). All integers
//! little-endian. Ids are dense (0..count), so every table is indexed by id
//! directly: no id arrays and no searching on the device.

use super::model::{CrawlIndex, Relation};
use super::pack::section;

fn put(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

/// [u32 count][u32 stringBytes][u32 offset]*(count+1)
/// [NUL-terminated strings]: string `id` is at offsets[id].
pub fn string_index(strings: &[String]) -> Vec<u8> {
    let data_len: usize = strings.iter().map(|s| s.len() + 1).sum();
    let mut out = Vec::with_capacity(8 + strings.len() * 4 + 4 + data_len);
    put(&mut out, strings.len() as u32);
    put(&mut out, data_len as u32);
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

/// [u32 sourceCount][u32 targetTotal][u32 start]*(sourceCount+1)
/// [u32 targetId]*: source `id`'s targets are targets[start[id]..start[id+1]],
/// ascending. Every source id below `sources` has an entry (empty ones too).
pub fn relation(rel: &Relation, sources: usize) -> Vec<u8> {
    let sources = sources.max(rel.keys().next_back().map_or(0, |&k| k as usize + 1));
    let total: usize = rel.values().map(|t| t.len()).sum();
    let mut out = Vec::with_capacity(8 + (sources + 1) * 4 + total * 4);
    put(&mut out, sources as u32);
    put(&mut out, total as u32);
    let mut start = 0u32;
    put(&mut out, 0);
    for src in 0..sources as u32 {
        start += rel.get(&src).map_or(0, |t| t.len() as u32);
        put(&mut out, start);
    }
    for t in rel.values().flatten() {
        put(&mut out, *t);
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

/// The fast-scroll bucket of `name` at `letters` (1..=3) letters, as the
/// wheel's letter overlay shows it. Same rules as `fastScrollKey` in
/// `software/src/state/fast_scroll.cpp`: a name that doesn't start with a
/// letter is `#` (they all sort first); then letters uppercased and any
/// other character a space (it sorts before letters). Names are ASCII
/// already (`text::sanitize_name`).
pub fn scroll_key(name: &str, letters: usize) -> String {
    let b = name.as_bytes();
    if !b.first().is_some_and(|c| c.is_ascii_alphabetic()) {
        return "#".into();
    }
    b.iter()
        .take(letters)
        .map(|&c| if c.is_ascii_alphabetic() { c.to_ascii_uppercase() as char } else { ' ' })
        .collect()
}

/// Where each `letters`-letter bucket of an alphabetical list starts:
/// [u32 count]{u32 start, u8 key[4] (NUL-padded)}*count, in list order.
pub fn letter_table(names: &[String], letters: usize) -> Vec<u8> {
    let mut entries: Vec<(u32, String)> = Vec::new();
    for (i, name) in names.iter().enumerate() {
        let key = scroll_key(name, letters);
        if entries.last().is_none_or(|(_, k)| *k != key) {
            entries.push((i as u32, key));
        }
    }
    let mut out = Vec::with_capacity(4 + entries.len() * 8);
    put(&mut out, entries.len() as u32);
    for (start, key) in entries {
        put(&mut out, start);
        let mut k = [0u8; 4];
        k[..key.len()].copy_from_slice(key.as_bytes());
        out.extend_from_slice(&k);
    }
    out
}

/// Every lookup table, as `index.pack` sections (the search trie and the
/// cover index are added by the caller).
pub fn all(ix: &CrawlIndex) -> Vec<(u32, Vec<u8>)> {
    let (artists, albums, genres) = (ix.artists.len(), ix.albums.len(), ix.genres.len());
    let mut out = vec![
        (section::ARTIST_NAMES, string_index(&ix.artists)),
        (section::ALBUM_NAMES, string_index(&ix.albums)),
        (section::GENRE_NAMES, string_index(&ix.genres)),
        (section::TRACK_NAMES, string_index(&ix.tracks)),
        (section::PATHS, string_index(&ix.paths)),
        (section::DURATIONS, durations(&ix.durations)),
        (section::TRACK_TO_ALBUM, dense_u32(&ix.track_to_album)),
        (section::TRACK_TO_ARTIST, dense_u32(&ix.track_to_artist)),
        (section::ALBUM_TO_ARTIST, dense_u32(&ix.album_to_artist())),
        (section::ARTIST_TO_ALBUMS, relation(&ix.artist_to_albums, artists)),
        (section::ARTIST_TO_TRACKS, relation(&ix.artist_to_tracks, artists)),
        (section::ALBUM_TO_TRACKS, relation(&ix.album_to_tracks, albums)),
        (section::GENRE_TO_ALBUMS, relation(&ix.genre_to_albums, genres)),
        (section::GENRE_TO_TRACKS, relation(&ix.genre_to_tracks, genres)),
        (section::GENRE_TO_ARTISTS, relation(&ix.genre_to_artists, genres)),
    ];
    for (list, names) in [&ix.artists, &ix.albums, &ix.genres, &ix.tracks].into_iter().enumerate() {
        for letters in 1..=3u32 {
            out.push((section::letters(list as u32, letters), letter_table(names, letters as usize)));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn u32s(b: &[u8]) -> Vec<u32> {
        b.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect()
    }

    #[test]
    fn strings_are_indexed_by_id() {
        let b = string_index(&["ab".into(), "".into(), "c".into()]);
        let w = u32s(&b[..24]);
        assert_eq!(w, [3, 6, 0, 3, 4, 6]);
        assert_eq!(&b[24..], b"ab\0\0c\0");
    }

    #[test]
    fn relations_are_dense_with_prefix_starts() {
        let mut rel = Relation::new();
        rel.entry(1).or_default().extend([4, 2]);
        rel.entry(3).or_default().insert(7);
        // 5 sources, 0/2/4 empty; source 1 -> [2, 4], 3 -> [7].
        let w = u32s(&relation(&rel, 5));
        assert_eq!(w, [5, 3, 0, 0, 2, 2, 3, 3, 2, 4, 7]);
    }

    #[test]
    fn scroll_keys() {
        // Shared with software/test/test_fast_scroll.cpp.
        let cases = [
            ("abba", 1, "A"), ("abba", 2, "AB"), ("abba", 3, "ABB"),
            ("A Day", 3, "A D"), ("A-ha", 2, "A "), ("AC/DC", 3, "AC "),
            ("2Pac", 1, "#"), ("(What's", 3, "#"), ("", 2, "#"), ("Z", 3, "Z"),
        ];
        for (name, letters, key) in cases {
            assert_eq!(scroll_key(name, letters), key, "{name} at {letters}");
        }
    }

    #[test]
    fn letter_tables_mark_bucket_starts() {
        let names: Vec<String> =
            ["10cc", "(x)", "A Day", "Aardvark", "Abba", "abc", "Beck"].iter().map(|s| s.to_string()).collect();
        let parse = |b: Vec<u8>| -> Vec<(u32, String)> {
            let n = u32::from_le_bytes(b[0..4].try_into().unwrap()) as usize;
            (0..n)
                .map(|i| {
                    let e = &b[4 + i * 8..12 + i * 8];
                    let key = e[4..].iter().take_while(|&&c| c != 0).map(|&c| c as char).collect();
                    (u32::from_le_bytes(e[..4].try_into().unwrap()), key)
                })
                .collect()
        };
        let l1 = parse(letter_table(&names, 1));
        assert_eq!(l1, [(0, "#".into()), (2, "A".into()), (6, "B".into())]);
        let l2 = parse(letter_table(&names, 2));
        assert_eq!(l2, [(0, "#".into()), (2, "A ".into()), (3, "AA".into()), (4, "AB".into()), (6, "BE".into())]);
        let l3 = parse(letter_table(&names, 3));
        assert_eq!(l3[3], (4, "ABB".into()));
        assert_eq!(l3[4], (5, "ABC".into()));
    }
}
