//! Where a track goes on the card: `<Album artist>/<Album>/<NN Title>.<ext>`,
//! the same layout as a beets library, with FAT/exFAT-safe names.

use std::collections::HashSet;

use crate::tags::Tags;

/// Longest path component kept (exFAT allows 255 UTF-16 units; leave room
/// for " (2)" and the extension).
const MAX_COMPONENT: usize = 120;

/// Replaces characters FAT/exFAT reject (and path separators) with "_",
/// drops control characters, trims spaces and trailing dots.
pub fn sanitize_component(s: &str) -> String {
    let mut out: String = s
        .chars()
        .filter(|c| !c.is_control())
        .map(|c| if matches!(c, '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|') { '_' } else { c })
        .collect();
    out = out.split_whitespace().collect::<Vec<_>>().join(" ");
    if out.chars().count() > MAX_COMPONENT {
        out = out.chars().take(MAX_COMPONENT).collect::<String>().trim_end().to_owned();
    }
    let out = out.trim_end_matches(['.', ' ']).trim_start_matches(' ').to_owned();
    if out.is_empty() || out == "." || out == ".." { "_".into() } else { out }
}

/// Canonical card path (no leading slash) for a track.
pub fn card_path(tags: &Tags, ext: &str) -> String {
    let owner = tags.album_artist.as_deref().or(tags.artist.as_deref()).unwrap_or("Unknown Artist");
    let album = tags.album.as_deref().unwrap_or("Unknown Album");
    let title = tags.title.as_deref().unwrap_or("Unknown Title");
    let prefix = match (tags.track, tags.disc_total > 1 || tags.disc > 1) {
        (0, _) => String::new(),
        (t, true) => format!("{}-{t:02} ", tags.disc.max(1)),
        (t, false) => format!("{t:02} "),
    };
    format!(
        "{}/{}/{}",
        sanitize_component(owner),
        sanitize_component(album),
        sanitize_component(&format!("{prefix}{title}.{ext}"))
    )
}

/// Paths already taken on the card, compared case-insensitively (exFAT and
/// FAT32 are case-insensitive).
#[derive(Default)]
pub struct Taken(HashSet<String>);

impl Taken {
    pub fn insert(&mut self, path: &str) -> bool {
        self.0.insert(path.to_lowercase())
    }
    pub fn remove(&mut self, path: &str) {
        self.0.remove(&path.to_lowercase());
    }
    pub fn contains(&self, path: &str) -> bool {
        self.0.contains(&path.to_lowercase())
    }

    /// `path`, or `name (2).ext`, `name (3).ext`… if it's taken; reserves it.
    pub fn claim(&mut self, path: &str) -> String {
        if self.insert(path) {
            return path.to_owned();
        }
        let (stem, ext) = path.rsplit_once('.').unwrap_or((path, ""));
        for n in 2.. {
            let candidate = format!("{stem} ({n}).{ext}");
            if self.insert(&candidate) {
                return candidate;
            }
        }
        unreachable!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn components() {
        assert_eq!(sanitize_component("Tu veux me ?"), "Tu veux me _");
        assert_eq!(sanitize_component("AC/DC"), "AC_DC");
        assert_eq!(sanitize_component(" Vol. 2..."), "Vol. 2");
        assert_eq!(sanitize_component(".."), "_");
        assert_eq!(sanitize_component("Françoise"), "Françoise");
    }

    #[test]
    fn layout() {
        let t = Tags {
            title: Some("Sad girl".into()),
            artist: Some("Yoa feat. X".into()),
            album_artist: Some("Yoa".into()),
            album: Some("La Favorite".into()),
            track: 2,
            ..Default::default()
        };
        assert_eq!(card_path(&t, "flac"), "Yoa/La Favorite/02 Sad girl.flac");
        let t = Tags { disc: 2, disc_total: 2, ..t };
        assert_eq!(card_path(&t, "flac"), "Yoa/La Favorite/2-02 Sad girl.flac");
    }

    #[test]
    fn claim_dedupes_case_insensitively() {
        let mut taken = Taken::default();
        assert_eq!(taken.claim("A/B/x.mp3"), "A/B/x.mp3");
        assert_eq!(taken.claim("a/b/X.mp3"), "a/b/X (2).mp3");
    }
}
