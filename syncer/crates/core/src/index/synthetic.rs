//! Synthetic libraries for the firmware test bench (`software/bench/`,
//! `openpod bench-dataset`): a large, seeded, reproducible set of
//! [`TrackRecord`]s whose files don't exist. The firmware only opens a track's
//! file when it plays it, so the index alone exercises browsing and search.
//!
//! The names mix what real libraries throw at the indexer: "The …" articles,
//! accents and non-latin scripts, very long titles, "feat." artists and
//! compilations.

use super::model::TrackRecord;

/// Bumped whenever the output for a given seed changes, so bench reports
/// made from different datasets aren't compared.
pub const GENERATOR_VERSION: u32 = 1;

/// SplitMix64: tiny, seedable, and stable across platforms and releases
/// (unlike `rand`'s generators), which keeps a seed's dataset the same.
struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        z ^ (z >> 31)
    }
    /// Uniform in `lo..=hi`.
    fn range(&mut self, lo: u32, hi: u32) -> u32 {
        lo + (self.next() % u64::from(hi - lo + 1)) as u32
    }
    fn chance(&mut self, percent: u32) -> bool {
        self.range(0, 99) < percent
    }
    fn pick<'a>(&mut self, items: &[&'a str]) -> &'a str {
        items[(self.next() % items.len() as u64) as usize]
    }
}

const SYLLABLES: &[&str] = &[
    "ka", "lo", "mi", "ra", "ten", "su", "vel", "dor", "an", "is", "mon", "ri", "zu", "pa", "gle", "tor", "ne", "sha",
    "bri", "co", "ul", "fen", "ya", "ost", "lu", "mar", "qui", "ex", "hal", "do", "ve", "nym",
];
const WORDS: &[&str] = &[
    "Night", "River", "Glass", "Echo", "Summer", "Static", "Paper", "Ghost", "Golden", "Velvet", "Electric", "Silent",
    "Broken", "Blue", "Wild", "Heart", "Machine", "Ocean", "Midnight", "Fire", "Dream", "City", "Love", "Shadow",
    "Light", "Rain", "Road", "Song", "Northern", "Lost", "Young", "Stone", "Garden", "Signal", "Mirror", "Winter",
];
/// Non-ASCII words: accents fold to ASCII for search, the rest is drawn
/// with the fallback glyph.
const UNICODE_WORDS: &[&str] = &[
    "Été", "Fiançailles", "Mañana", "Ørsted", "Über", "Ça", "Größe", "Café", "Noël", "Søren", "Добро", "東京", "Ελλάδα",
    "Zürich", "Œuvre", "Ñandú",
];
const GENRES: &[&str] = &[
    "Rock", "Pop", "Jazz", "Electronic", "Hip-Hop", "Classical", "Folk", "Metal", "Soul", "Funk", "Reggae", "Blues",
    "Ambient", "Techno", "House", "Punk", "Indie", "R&B", "Country", "Disco", "Drum & Bass", "Dub", "Gospel", "Grunge",
    "Latin", "Lo-Fi", "New Wave", "Opera", "Post-Rock", "Shoegaze", "Ska", "Soundtrack", "Swing", "Trance", "Trip-Hop",
    "World", "Chanson", "Afrobeat", "Bossa Nova", "Synthpop",
];

fn capitalize(s: &str) -> String {
    let mut c = s.chars();
    c.next().map(|f| f.to_uppercase().chain(c).collect()).unwrap_or_default()
}

/// A made-up word, e.g. "Kalomi".
fn coined(rng: &mut Rng) -> String {
    let n = rng.range(2, 3);
    capitalize(&(0..n).map(|_| rng.pick(SYLLABLES)).collect::<String>())
}

fn word(rng: &mut Rng) -> String {
    match rng.range(0, 9) {
        0..=4 => rng.pick(WORDS).to_string(),
        5..=8 => coined(rng),
        _ => rng.pick(UNICODE_WORDS).to_string(),
    }
}

fn phrase(rng: &mut Rng, min: u32, max: u32) -> String {
    let n = rng.range(min, max);
    let mut s = (0..n).map(|_| word(rng)).collect::<Vec<_>>().join(" ");
    if rng.chance(10) {
        s = format!("The {s}");
    }
    if rng.chance(4) {
        // Longer than any row: exercises truncation and the 64-byte names.
        s = format!("{s} ({}, {} and the {} Sessions)", phrase_plain(rng), phrase_plain(rng), word(rng));
    }
    s
}

fn phrase_plain(rng: &mut Rng) -> String {
    (0..rng.range(2, 4)).map(|_| word(rng)).collect::<Vec<_>>().join(" ")
}

fn artist_name(rng: &mut Rng) -> String {
    match rng.range(0, 9) {
        0..=3 => format!("{} {}", coined(rng), coined(rng)), // "Kalomi Tenvel"
        4..=6 => phrase(rng, 1, 3),                          // a band
        7 => format!("The {}s", rng.pick(WORDS)),
        8 => format!("DJ {}", coined(rng)),
        _ => format!("{} & {}", coined(rng), coined(rng)),
    }
}

fn safe_path(s: &str) -> String {
    s.chars().map(|c| if "/\\:*?\"<>|".contains(c) { '_' } else { c }).collect()
}

/// Shape of a synthetic library, derived from the track count.
#[derive(Debug, Clone, Copy)]
pub struct Shape {
    pub tracks: u32,
    pub artists: u32,
}

impl Shape {
    /// About 13 tracks per album and 2 albums per artist.
    pub fn for_tracks(tracks: u32) -> Self {
        Shape { tracks, artists: (tracks / 26).max(1) }
    }
}

/// `shape.tracks` seeded tracks, grouped in albums of 6-20 tracks. Albums
/// are listed track by track, in album order.
pub fn records(shape: Shape, seed: u64) -> Vec<TrackRecord> {
    let mut rng = Rng(seed);
    let artists: Vec<String> = (0..shape.artists).map(|_| artist_name(&mut rng)).collect();
    let mut out = Vec::with_capacity(shape.tracks as usize);
    let mut album_no = 0u32;
    while (out.len() as u32) < shape.tracks {
        album_no += 1;
        let left = shape.tracks - out.len() as u32;
        let count = rng.range(6, 20).min(left);
        // Squared: a few artists get many albums, most get one or two.
        let r = (rng.next() % 1_000_000) as f64 / 1_000_000.0;
        let artist = &artists[((r * r) * artists.len() as f64) as usize];
        let compilation = rng.chance(5);
        let album_artist = if compilation { "Various Artists".to_string() } else { artist.clone() };
        let album = phrase(&mut rng, 1, 4);
        let genre = rng.pick(GENRES).to_string();
        let year = rng.range(1960, 2025);
        let dir = format!("{}/{} ({album_no})", safe_path(&album_artist), safe_path(&album));
        for n in 1..=count {
            let mut track_artist = if compilation {
                artists[(rng.next() % artists.len() as u64) as usize].clone()
            } else {
                artist.clone()
            };
            if rng.chance(3) {
                let other = &artists[(rng.next() % artists.len() as u64) as usize];
                track_artist = format!("{track_artist} feat. {other}");
            }
            let title = phrase(&mut rng, 1, 5);
            let file_name = format!("{n:02} {}.mp3", safe_path(&title));
            out.push(TrackRecord {
                card_path: format!("{dir}/{file_name}"),
                file_name,
                title: Some(title),
                artist: Some(track_artist),
                album_artist: Some(album_artist.clone()),
                album: Some(album.clone()),
                genre: Some(genre.clone()),
                track_no: n,
                year,
                duration_secs: f64::from(rng.range(90, 540)),
            });
        }
    }
    out
}

/// A 300×300 PNG gradient seeded by `key`, standing in for album art.
pub fn cover_png(key: u64) -> Vec<u8> {
    let mut rng = Rng(key);
    let (a, b) = ([rng.range(0, 255), rng.range(0, 255), rng.range(0, 255)], [
        rng.range(0, 255),
        rng.range(0, 255),
        rng.range(0, 255),
    ]);
    let img = image::RgbImage::from_fn(300, 300, |x, y| {
        let t = (x + y) as f32 / 598.0;
        let mix = |i: usize| (a[i] as f32 * (1.0 - t) + b[i] as f32 * t) as u8;
        image::Rgb([mix(0), mix(1), mix(2)])
    });
    let mut bytes = Vec::new();
    img.write_to(&mut std::io::Cursor::new(&mut bytes), image::ImageFormat::Png).expect("png encode");
    bytes
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn same_seed_same_library() {
        let a = records(Shape::for_tracks(500), 7);
        let b = records(Shape::for_tracks(500), 7);
        assert_eq!(a.len(), 500);
        assert!(a.iter().zip(&b).all(|(x, y)| x.card_path == y.card_path && x.title == y.title));
        let c = records(Shape::for_tracks(500), 8);
        assert!(a.iter().zip(&c).any(|(x, y)| x.title != y.title));
    }

    #[test]
    fn indexes() {
        let recs = records(Shape::for_tracks(2000), 1);
        let ix = super::super::model::build(&recs, Default::default());
        assert_eq!(ix.tracks.len(), 2000);
        assert!(ix.albums.len() > 2000 / 20 && ix.albums.len() < 2000 / 6 + 1);
        assert!(ix.artists.len() > 10);
        assert!(ix.genres.len() > 10);
    }

    #[test]
    fn cover_decodes() {
        let png = cover_png(3);
        assert!(image::load_from_memory(&png).is_ok());
    }
}
