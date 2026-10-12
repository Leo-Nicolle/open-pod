//! Builds the id-based crawl index from a list of tracks, ported from
//! `readMetadata` / `sortIndexesAlphabetically` in `indexer/src/crawler.ts`.

use std::collections::{BTreeSet, HashMap};

use icu_collator::options::{CollatorOptions, Strength};
use icu_collator::{Collator, CollatorBorrowed, CollatorPreferences};

use super::text::{sanitize_name, split_artists};

/// One audio file, with its raw (unsanitised) tag values.
#[derive(Debug, Clone, Default)]
pub struct TrackRecord {
    /// Card path as written to `path_index.bin` (`<dir>/<file>`).
    pub card_path: String,
    pub file_name: String,
    pub title: Option<String>,
    pub artist: Option<String>,
    pub album_artist: Option<String>,
    pub album: Option<String>,
    pub genre: Option<String>,
    pub track_no: u32,
    pub year: u32,
    pub duration_secs: f64,
}

/// How tracks are grouped into albums.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum AlbumKey {
    /// Album name only (TS indexer behaviour): different artists' "Greatest
    /// Hits" merge into one album.
    Name,
    /// Album artist + album name. Without an album-artist tag the track's
    /// folder stands in, so a compilation in one folder stays one album.
    #[default]
    ArtistAlbum,
}

/// Dense, 0-based ids, already relabelled in alphabetical order.
///
/// `*_order` lists the (new) ids in *first-seen* order: the TS indexer
/// iterates its maps in insertion order when building the search trie, so the
/// trie layout depends on it.
#[derive(Debug, Default)]
pub struct CrawlIndex {
    pub tracks: Vec<String>,
    pub artists: Vec<String>,
    pub albums: Vec<String>,
    pub genres: Vec<String>,
    pub paths: Vec<String>,
    pub durations: Vec<f64>,
    pub track_order: Vec<u32>,
    pub artist_order: Vec<u32>,
    pub album_order: Vec<u32>,
    pub genre_order: Vec<u32>,
    pub artist_to_albums: Relation,
    pub album_to_tracks: Relation,
    pub artist_to_tracks: Relation,
    pub genre_to_albums: Relation,
    pub genre_to_tracks: Relation,
    pub genre_to_artists: Relation,
    pub track_to_album: Vec<u32>,
    /// Primary (first-listed) artist of each track.
    pub track_to_artist: Vec<u32>,
    /// Index into the input records of the track that supplies each album's
    /// cover art (its first track).
    pub album_art_track: Vec<usize>,
}

/// source id -> sorted, de-duplicated target ids. Sources with no targets are
/// absent (the binary format only lists sources that were ever inserted).
pub type Relation = std::collections::BTreeMap<u32, BTreeSet<u32>>;

struct Interner {
    by_key: HashMap<String, u32>,
    names: Vec<String>,
}

impl Interner {
    fn new() -> Self {
        Self { by_key: HashMap::new(), names: Vec::new() }
    }
    /// Returns (id, is_new).
    fn get_or_add(&mut self, key: &str, name: &str) -> (u32, bool) {
        if let Some(&id) = self.by_key.get(key) {
            return (id, false);
        }
        let id = self.names.len() as u32;
        self.by_key.insert(key.to_owned(), id);
        self.names.push(name.to_owned());
        (id, true)
    }
}

fn add(rel: &mut Relation, from: u32, to: u32) {
    rel.entry(from).or_default().insert(to);
}

fn collator() -> CollatorBorrowed<'static> {
    let mut options = CollatorOptions::default();
    options.strength = Some(Strength::Primary);
    Collator::try_new(CollatorPreferences::default(), options).expect("baked collation data")
}

/// old id -> new id, so that new ids follow alphabetical (ICU root, primary
/// strength = JS `localeCompare(…, {sensitivity: "base"})`) order. Ties keep
/// their original order (JS sort is stable).
fn alphabetical_remap(names: &[String], coll: &CollatorBorrowed) -> Vec<u32> {
    let mut sorted: Vec<u32> = (0..names.len() as u32).collect();
    sorted.sort_by(|&a, &b| coll.compare(&names[a as usize], &names[b as usize]));
    let mut remap = vec![0u32; names.len()];
    for (new_id, &old_id) in sorted.iter().enumerate() {
        remap[old_id as usize] = new_id as u32;
    }
    remap
}

fn permute<T: Clone + Default>(items: &[T], remap: &[u32]) -> Vec<T> {
    let mut out = vec![T::default(); items.len()];
    for (old, item) in items.iter().enumerate() {
        out[remap[old] as usize] = item.clone();
    }
    out
}

fn remap_relation(rel: &Relation, src: &[u32], dst: &[u32]) -> Relation {
    rel.iter()
        .map(|(&from, tos)| {
            (src[from as usize], tos.iter().map(|&t| dst[t as usize]).collect())
        })
        .collect()
}

pub fn build(records: &[TrackRecord], album_key: AlbumKey) -> CrawlIndex {
    let mut artists = Interner::new();
    let mut albums = Interner::new();
    let mut genres = Interner::new();
    let mut tracks = Vec::with_capacity(records.len());
    let mut paths = Vec::with_capacity(records.len());
    let mut durations = Vec::with_capacity(records.len());
    let mut track_to_album = Vec::with_capacity(records.len());
    let mut track_to_artist = Vec::with_capacity(records.len());
    let mut album_art_track = Vec::new();
    let mut ix = CrawlIndex::default();

    for (i, rec) in records.iter().enumerate() {
        let title = sanitize_name(rec.title.as_deref().filter(|s| !s.is_empty()).unwrap_or(&rec.file_name));
        let mut artist_names = split_artists(rec.artist.as_deref());
        if artist_names.is_empty() {
            artist_names.push("Unknown Artist".into());
        }
        let album = sanitize_name(rec.album.as_deref().filter(|s| !s.is_empty()).unwrap_or("Unknown Album"));
        let genre = sanitize_name(rec.genre.as_deref().filter(|s| !s.is_empty()).unwrap_or("Unknown Genre"));

        let artist_ids: Vec<u32> =
            artist_names.iter().map(|a| artists.get_or_add(a, a).0).collect();

        let key = match album_key {
            AlbumKey::Name => album.clone(),
            AlbumKey::ArtistAlbum => {
                let owner = rec
                    .album_artist
                    .as_deref()
                    .map(sanitize_name)
                    .filter(|s| !s.is_empty())
                    .unwrap_or_else(|| {
                        let dir = rec.card_path.rsplit_once('/').map_or("", |(d, _)| d);
                        format!("dir:{dir}")
                    });
                format!("{}\u{1f}{}", owner.to_lowercase(), album.to_lowercase())
            }
        };
        let (album_id, new_album) = albums.get_or_add(&key, &album);
        if new_album {
            album_art_track.push(i);
        }
        let (genre_id, _) = genres.get_or_add(&genre, &genre);

        let track_id = tracks.len() as u32;
        tracks.push(title);
        paths.push(rec.card_path.clone());
        durations.push(rec.duration_secs);
        track_to_album.push(album_id);
        track_to_artist.push(artist_ids[0]);

        add(&mut ix.album_to_tracks, album_id, track_id);
        add(&mut ix.genre_to_tracks, genre_id, track_id);
        add(&mut ix.genre_to_albums, genre_id, album_id);
        for &artist_id in &artist_ids {
            add(&mut ix.artist_to_tracks, artist_id, track_id);
            add(&mut ix.artist_to_albums, artist_id, album_id);
            add(&mut ix.genre_to_artists, genre_id, artist_id);
        }
    }

    let coll = collator();
    let track_r = alphabetical_remap(&tracks, &coll);
    let artist_r = alphabetical_remap(&artists.names, &coll);
    let album_r = alphabetical_remap(&albums.names, &coll);
    let genre_r = alphabetical_remap(&genres.names, &coll);

    CrawlIndex {
        track_order: track_r.clone(),
        artist_order: artist_r.clone(),
        album_order: album_r.clone(),
        genre_order: genre_r.clone(),
        tracks: permute(&tracks, &track_r),
        artists: permute(&artists.names, &artist_r),
        albums: permute(&albums.names, &album_r),
        genres: permute(&genres.names, &genre_r),
        paths: permute(&paths, &track_r),
        durations: permute(&durations, &track_r),
        track_to_album: permute(
            &track_to_album.iter().map(|&a| album_r[a as usize]).collect::<Vec<_>>(),
            &track_r,
        ),
        track_to_artist: permute(
            &track_to_artist.iter().map(|&a| artist_r[a as usize]).collect::<Vec<_>>(),
            &track_r,
        ),
        album_art_track: permute(&album_art_track, &album_r),
        artist_to_albums: remap_relation(&ix.artist_to_albums, &artist_r, &album_r),
        album_to_tracks: remap_relation(&ix.album_to_tracks, &album_r, &track_r),
        artist_to_tracks: remap_relation(&ix.artist_to_tracks, &artist_r, &track_r),
        genre_to_albums: remap_relation(&ix.genre_to_albums, &genre_r, &album_r),
        genre_to_tracks: remap_relation(&ix.genre_to_tracks, &genre_r, &track_r),
        genre_to_artists: remap_relation(&ix.genre_to_artists, &genre_r, &artist_r),
    }
}

impl CrawlIndex {
    /// Each album's artist: the primary artist credited on most of its
    /// tracks, ties to the lowest artist id. `u32::MAX` (TS: -1) never
    /// happens since every album has at least one track.
    pub fn album_to_artist(&self) -> Vec<u32> {
        let mut counts: Vec<HashMap<u32, u32>> = vec![HashMap::new(); self.albums.len()];
        for (t, &album) in self.track_to_album.iter().enumerate() {
            *counts[album as usize].entry(self.track_to_artist[t]).or_default() += 1;
        }
        counts
            .iter()
            .map(|per| {
                per.iter()
                    .max_by(|a, b| a.1.cmp(b.1).then_with(|| b.0.cmp(a.0)))
                    .map_or(u32::MAX, |(&artist, _)| artist)
            })
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rec(path: &str, title: &str, artist: &str, album: &str, album_artist: Option<&str>) -> TrackRecord {
        TrackRecord {
            card_path: path.into(),
            file_name: path.rsplit('/').next().unwrap().into(),
            title: Some(title.into()),
            artist: Some(artist.into()),
            album_artist: album_artist.map(Into::into),
            album: Some(album.into()),
            ..Default::default()
        }
    }

    #[test]
    fn album_key_separates_same_named_albums() {
        let records = [
            rec("/Queen/GH/1.mp3", "Bohemian", "Queen", "Greatest Hits", Some("Queen")),
            rec("/ABBA/GH/1.mp3", "Waterloo", "ABBA", "Greatest Hits", Some("ABBA")),
            // No album artist: one folder = one album, whatever the track artists.
            rec("/Comp/Hits/1.mp3", "A", "X", "Hits", None),
            rec("/Comp/Hits/2.mp3", "B", "Y", "Hits", None),
        ];
        assert_eq!(build(&records, AlbumKey::Name).albums, ["Greatest Hits", "Hits"]);
        let ix = build(&records, AlbumKey::ArtistAlbum);
        assert_eq!(ix.albums, ["Greatest Hits", "Greatest Hits", "Hits"]);
        assert_eq!(ix.album_to_artist().len(), 3);
    }

    #[test]
    fn ids_are_alphabetical_and_order_is_first_seen() {
        let records = [
            rec("/b.mp3", "beta", "Zed", "Z", None),
            rec("/a.mp3", "Alpha", "amy", "Y", None),
        ];
        let ix = build(&records, AlbumKey::Name);
        assert_eq!(ix.tracks, ["Alpha", "beta"]);
        assert_eq!(ix.paths, ["/a.mp3", "/b.mp3"]);
        assert_eq!(ix.track_order, [1, 0]);
        assert_eq!(ix.artists, ["amy", "Zed"]);
    }
}
