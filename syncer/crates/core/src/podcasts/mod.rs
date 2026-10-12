//! Podcasts: a source folder mirrored to `<card>/Podcasts/` (see
//! [`mirror`]), the browser tree the firmware reads ([`tree`]) and the
//! listening state it writes ([`state`]).
//!
//! The card's podcasts are recorded in `openpod/podcasts.json`, apart from
//! the music manifest: music sync is additive, the podcast mirror deletes.
//! An episode is identified by its source file's [`crate::sync::content_hash`],
//! which also keys its listening state, so moving or renaming folders in the
//! source keeps what was listened to.

pub mod mirror;
pub mod state;
pub mod tree;

use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::index::thumbs;
use crate::manifest::{self, SourceRef};

pub const VERSION: u32 = 1;
pub const FILE: &str = "podcasts.json";
/// Card folder the podcasts are mirrored into.
pub const CARD_DIR: &str = "Podcasts";
pub const META_FILE: &str = "podcast_meta.bin";
pub const META_MAGIC: [u8; 4] = *b"OPPI";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PodcastManifest {
    pub version: u32,
    /// Bumped by every rebuild of the tables; written to `podcast_meta.bin`
    /// and `podcast_state.bin`.
    #[serde(default)]
    pub generation: u32,
    pub episodes: Vec<Episode>,
    /// How many of a show's latest episodes stay on the card, by
    /// [`show_key`]; a show not listed keeps them all. On the card so every
    /// computer mirrors the same.
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub keep: BTreeMap<String, u32>,
}

impl PodcastManifest {
    /// `show` keeps its latest `keep` episodes on the card (None: all).
    pub fn set_keep(&mut self, show: &str, keep: Option<u32>) {
        match keep {
            Some(n) => self.keep.insert(show.to_owned(), n.max(1)),
            None => self.keep.remove(show),
        };
    }
}

impl Default for PodcastManifest {
    fn default() -> Self {
        Self { version: VERSION, generation: 0, episodes: Vec::new(), keep: BTreeMap::new() }
    }
}

/// A show is the folder holding its episodes: its folder names from the
/// podcasts root, '/'-joined ("" for episodes at the root).
pub fn show_key(folders: &[String]) -> String {
    folders.join("/")
}

/// Sets how many latest episodes `show` keeps on the card (None: all), and
/// saves the manifest. The next sync applies it.
pub fn set_keep(card: &Path, show: &str, keep: Option<u32>) -> io::Result<PodcastManifest> {
    let mut m = load(card)?;
    m.set_keep(show, keep);
    save(card, &m)?;
    Ok(m)
}

/// Marks the episode at card path `path` played or not in the card's
/// `podcast_state.bin`, as the player's "Mark as played/unplayed" does.
pub fn set_played(card: &Path, path: &str, played: bool) -> io::Result<()> {
    let m = load(card)?;
    let key = m
        .episodes
        .iter()
        .find(|e| e.path == path)
        .map(Episode::key)
        .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, format!("{path}: not a podcast on the card")))?;
    let file = card.join(manifest::DIR).join(state::FILE);
    let mut b = fs::read(&file)?;
    if !state::set_played(&mut b, key, played) {
        return Err(io::Error::other("the player's listening state has no record of this episode"));
    }
    fs::write(&file, b)
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Episode {
    /// Card path, '/'-separated, under [`CARD_DIR`].
    pub path: String,
    /// Size of the file on the card.
    pub size: u64,
    /// Content hash of the source file: the episode's identity.
    pub hash: String,
    /// Folder names from the podcasts root down, as in the source.
    pub folders: Vec<String>,
    /// Raw title (tag, else the file name).
    pub title: String,
    /// Publication date, days since 1970-01-01 (0: unknown).
    #[serde(default)]
    pub date: u32,
    pub duration: f64,
    pub conversion: String,
    /// Source files it was mirrored from (one per computer).
    #[serde(default)]
    pub sources: Vec<SourceRef>,
    /// Unix seconds.
    pub added_at: u64,
}

impl Episode {
    /// The state file's key: the hash's 64 bits.
    pub fn key(&self) -> u64 {
        u64::from_str_radix(&self.hash, 16).unwrap_or(0)
    }
}

pub fn path(card: &Path) -> PathBuf {
    card.join(manifest::DIR).join(FILE)
}

/// The card's podcasts; an empty manifest when it has none yet.
pub fn load(card: &Path) -> io::Result<PodcastManifest> {
    let data = match fs::read(path(card)) {
        Ok(d) => d,
        Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(PodcastManifest::default()),
        Err(e) => return Err(e),
    };
    let m: PodcastManifest = serde_json::from_slice(&data).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
    if m.version > VERSION {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("{FILE} version {} is newer than this tool ({VERSION}): update openpod", m.version),
        ));
    }
    Ok(m)
}

/// Writes `bytes` next to `target` then renames it over: a reader never
/// sees half a file.
fn write_atomic(target: &Path, bytes: &[u8]) -> io::Result<()> {
    if let Some(dir) = target.parent() {
        fs::create_dir_all(dir)?;
    }
    let tmp = PathBuf::from(format!("{}.tmp", target.display()));
    let mut f = fs::File::create(&tmp)?;
    io::Write::write_all(&mut f, bytes)?;
    f.sync_all()?;
    drop(f);
    fs::rename(&tmp, target)
}

pub fn save(card: &Path, m: &PodcastManifest) -> io::Result<()> {
    write_atomic(&path(card), &serde_json::to_vec(m).map_err(io::Error::other)?)
}

/// Library-relative excludes for the podcasts folder when it sits inside a
/// music library: podcasts aren't music.
pub fn library_excludes(libraries: &[PathBuf], podcasts: &Path) -> Vec<String> {
    let Ok(p) = fs::canonicalize(podcasts) else { return Vec::new() };
    libraries
        .iter()
        .filter_map(|l| p.strip_prefix(fs::canonicalize(l).ok()?).ok().map(|r| r.to_string_lossy().into_owned()))
        .filter(|r| !r.is_empty())
        .collect()
}

// --- dates ----------------------------------------------------------------------

/// Days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's
/// `days_from_civil`).
pub fn days_from_civil(y: i64, m: u32, d: u32) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let mp = (m as i64 + 9) % 12;
    let doy = (153 * mp + 2) / 5 + d as i64 - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

fn date_days(y: i64, m: u32, d: u32) -> Option<u32> {
    if !(1971..=2200).contains(&y) || !(1..=12).contains(&m) || !(1..=31).contains(&d) {
        return None;
    }
    u32::try_from(days_from_civil(y, m, d)).ok()
}

/// "2018-04-06", "2018-04-06T07:00:00", "06.04.2018", "06/04/2018" -> days.
/// A year alone isn't a date: it would sort a whole year as one day.
pub fn parse_date(s: &str) -> Option<u32> {
    let s = s.trim();
    let nums: Vec<&str> = s.split(|c: char| !c.is_ascii_digit()).filter(|p| !p.is_empty()).take(3).collect();
    if nums.len() < 3 {
        return None;
    }
    let n = |i: usize| nums[i].parse::<i64>().ok();
    if nums[0].len() == 4 {
        date_days(n(0)?, n(1)? as u32, n(2)? as u32)
    } else if nums[2].len() == 4 {
        date_days(n(2)?, n(1)? as u32, n(0)? as u32)
    } else {
        None
    }
}

/// A date in a file name: `2026-10-05 - Title.m4a` (downloaders), or Radio
/// France's `10467-06.04.2018-ITEMA_….mp3`. The first valid
/// `YYYY?MM?DD` / `DD?MM?YYYY` (`?` one of `-._`) not inside a longer number.
pub fn date_in_name(name: &str) -> Option<u32> {
    let b = name.as_bytes();
    let digits = |s: &[u8]| s.iter().all(u8::is_ascii_digit);
    let sep = |c: u8| matches!(c, b'-' | b'.' | b'_');
    (0..b.len().saturating_sub(9)).find_map(|i| {
        let w = &b[i..i + 10];
        let bounded = (i == 0 || !b[i - 1].is_ascii_digit()) && b.get(i + 10).is_none_or(|c| !c.is_ascii_digit());
        let iso = digits(&w[..4]) && sep(w[4]) && digits(&w[5..7]) && sep(w[7]) && digits(&w[8..]);
        let dmy = digits(&w[..2]) && sep(w[2]) && digits(&w[3..5]) && sep(w[5]) && digits(&w[6..]);
        (bounded && (iso || dmy)).then(|| parse_date(std::str::from_utf8(w).ok()?)).flatten()
    })
}

/// Unix nanoseconds -> days.
pub fn days_of_mtime(mtime_ns: i128) -> u32 {
    (mtime_ns / 1_000_000_000 / 86_400).clamp(0, u32::MAX as i128) as u32
}

// --- tables --------------------------------------------------------------------

/// `podcast_meta.bin`: "OPPI" | u16 version | u16 0 | u32 episodes | u32 generation.
pub fn meta(episodes: usize, generation: u32) -> Vec<u8> {
    let mut out = Vec::with_capacity(16);
    out.extend_from_slice(&META_MAGIC);
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&0u16.to_le_bytes());
    out.extend_from_slice(&(episodes as u32).to_le_bytes());
    out.extend_from_slice(&generation.to_le_bytes());
    out
}

#[derive(Debug, Default)]
pub struct Summary {
    pub episodes: usize,
    pub folders: usize,
    /// Episodes whose listening state was carried over.
    pub kept_state: usize,
    pub warnings: Vec<String>,
}

/// Every podcast file for the card, as (file name, bytes), in write order:
/// the state is second to last and the meta file last, so a sync
/// interrupted in between leaves a generation mismatch the firmware
/// notices. Bumps `m.generation`. `old_state` is the card's current
/// `podcast_state.bin`; `art(folder)` gives a folder's cover bytes (a cover
/// file in its card directory, else its first episode's embedded art).
pub fn build_files<F>(
    m: &mut PodcastManifest,
    old_state: Option<&[u8]>,
    thumbs_cfg: thumbs::Config,
    art: F,
) -> (Vec<(String, Vec<u8>)>, Summary)
where
    F: Fn(&tree::Folder, Option<&Episode>) -> Option<Vec<u8>> + Sync,
{
    use rayon::prelude::*;

    let ins: Vec<tree::EpisodeIn> = m
        .episodes
        .iter()
        .map(|e| tree::EpisodeIn { folders: &e.folders, title: &e.title, date: e.date, duration: e.duration, path: &e.path })
        .collect();
    let t = tree::build(&ins);
    let mut files = t.files(&ins);

    // Covers: a folder without its own art shows its parent's.
    let own: Vec<Option<Vec<u8>>> = t
        .folders
        .par_iter()
        .map(|f| art(f, f.episodes.first().map(|&id| &m.episodes[t.order[id as usize]])))
        .collect();
    let mut source: Vec<Option<usize>> = vec![None; t.folders.len()];
    for (i, f) in t.folders.iter().enumerate() {
        source[i] = if own[i].is_some() {
            Some(i)
        } else if f.parent != tree::NO_PARENT {
            source[f.parent as usize]
        } else {
            None
        };
    }
    let thumbs: Vec<thumbs::Thumbnail> = t
        .folders
        .par_iter()
        .enumerate()
        .map(|(i, f)| thumbs::generate(source[i].and_then(|s| own[s].as_deref()), thumbs_cfg, &f.dir))
        .collect();
    let packed = thumbs::pack(&thumbs, thumbs_cfg.format);
    files.push(("podcast_thumbs.bin".into(), packed.blob));
    files.push(("podcast_folder_to_cover.bin".into(), thumbs::cover_index(&packed.entries)));

    let keys: Vec<u64> = t.order.iter().map(|&i| m.episodes[i].key()).collect();
    let old = old_state.and_then(state::parse).map(|(_, r)| r).unwrap_or_default();
    let records = state::remap(&old, &keys);
    m.generation = m.generation.wrapping_add(1);
    files.push((state::FILE.into(), state::encode(m.generation, &records)));
    files.push((META_FILE.into(), meta(keys.len(), m.generation)));

    let summary = Summary {
        episodes: keys.len(),
        folders: t.folders.len(),
        kept_state: records.iter().filter(|r| !r.is_blank()).count(),
        warnings: thumbs.iter().filter_map(|t| t.warning.clone()).collect(),
    };
    (files, summary)
}

/// Where each folder's cover comes from, for a caller that reads the card
/// itself (the browser): (card directory, its first own episode's card
/// path). [`build_files`]'s `art` gets the same folders.
pub fn art_sources(m: &PodcastManifest) -> Vec<(String, Option<String>)> {
    let ins: Vec<tree::EpisodeIn> = m
        .episodes
        .iter()
        .map(|e| tree::EpisodeIn { folders: &e.folders, title: &e.title, date: e.date, duration: e.duration, path: &e.path })
        .collect();
    let t = tree::build(&ins);
    t.folders
        .iter()
        .map(|f| (f.dir.clone(), f.episodes.first().map(|&id| m.episodes[t.order[id as usize]].path.clone())))
        .collect()
}

/// A folder's cover on a mounted card: `cover.jpg` & co. in its directory,
/// else the first episode's embedded art.
fn card_art(card: &Path, f: &tree::Folder, first: Option<&Episode>) -> Option<Vec<u8>> {
    let dir = card.join(&f.dir);
    let names: Vec<String> =
        fs::read_dir(&dir).ok()?.flatten().map(|e| e.file_name().to_string_lossy().into_owned()).collect();
    if let Some(name) = crate::index::crawl::cover_file(&names) {
        return fs::read(dir.join(name)).ok();
    }
    let ep = card.join(&first?.path);
    fs::File::open(&ep).ok().and_then(|f| crate::index::crawl::embedded_art(&mut io::BufReader::new(f), &ep))
}

/// Rebuilds the podcast tables and remaps the listening state on the card,
/// then saves the manifest (its generation changed). A bad state file is
/// kept as `podcast_state.bin.bad` and replaced by a fresh one.
pub fn reindex(card: &Path, m: &mut PodcastManifest) -> io::Result<Summary> {
    let dir = card.join(manifest::DIR);
    fs::create_dir_all(&dir)?;
    let state_path = dir.join(state::FILE);
    let old = fs::read(&state_path).ok();
    if let Some(bytes) = &old {
        if state::parse(bytes).is_none() {
            let _ = fs::rename(&state_path, dir.join(format!("{}.bad", state::FILE)));
        }
    }
    let (files, summary) = build_files(m, old.as_deref(), thumbs::Config::default(), |f, first| card_art(card, f, first));
    save(card, m)?;
    for (name, bytes) in files {
        write_atomic(&dir.join(name), &bytes)?;
    }
    Ok(summary)
}

/// What the podcast mirror did, in the sync report.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Report {
    pub copied: usize,
    pub moved: usize,
    pub deleted: usize,
    pub bytes_written: u64,
    pub skipped: Vec<crate::view::Skipped>,
    pub failed: Vec<crate::view::Skipped>,
    /// Card totals after the sync.
    pub episodes: usize,
    pub shows: usize,
    /// Episodes with a listening state (started or played).
    pub listened: usize,
}

impl Report {
    pub fn new(plan: &mirror::MirrorPlan, out: &mirror::Outcome, m: &PodcastManifest, index: Option<&Summary>) -> Self {
        let skipped = |v: &[(PathBuf, String)]| -> Vec<crate::view::Skipped> {
            v.iter().map(|(p, r)| crate::view::Skipped { path: p.to_string_lossy().into_owned(), reason: r.clone() }).collect()
        };
        let mut unsupported = skipped(&plan.unsupported);
        unsupported.extend(skipped(&plan.unreadable));
        let shows: std::collections::HashSet<&[String]> = m.episodes.iter().map(|e| e.folders.as_slice()).collect();
        Report {
            copied: out.copied.len(),
            moved: out.moved,
            deleted: out.deleted,
            bytes_written: out.bytes,
            skipped: unsupported,
            failed: skipped(&out.failed),
            episodes: m.episodes.len(),
            shows: shows.len(),
            listened: index.map_or(0, |s| s.kept_state),
        }
    }

    /// The podcasts section of `sync-report.txt`.
    pub fn text(&self) -> String {
        let mut r = format!(
            "## Podcasts\n  copied {}, moved {}, deleted {}, failed {}, skipped {}; {} episodes in {} folders on the card\n",
            self.copied,
            self.moved,
            self.deleted,
            self.failed.len(),
            self.skipped.len(),
            self.episodes,
            self.shows
        );
        for s in self.failed.iter().chain(&self.skipped) {
            r.push_str(&format!("  {} — {}\n", s.path, s.reason));
        }
        r
    }
}

/// After [`mirror::execute`]: rebuilds the tables when the card's podcasts
/// changed (or have no tables yet), then adds the podcasts to the sync
/// reports [`crate::library::finish_sync`] wrote.
pub fn finish(card: &Path, m: &mut PodcastManifest, plan: &mirror::MirrorPlan, out: &mirror::Outcome) -> io::Result<(Report, Option<Summary>)> {
    let index = if out.changed() || !index_exists(card) { Some(reindex(card, m)?) } else { None };
    let report = Report::new(plan, out, m, index.as_ref());
    let dir = card.join(manifest::DIR);
    if let Some(mut r) = crate::view::load_report(card) {
        r.podcasts = Some(report.clone());
        fs::write(dir.join(crate::view::REPORT_FILE), serde_json::to_vec_pretty(&r).map_err(io::Error::other)?)?;
    }
    let text = dir.join("sync-report.txt");
    if let Ok(mut t) = fs::read_to_string(&text) {
        t.push_str(&report.text());
        fs::write(&text, t)?;
    }
    Ok((report, index))
}

/// Whether the card has podcast tables at all.
pub fn index_exists(card: &Path) -> bool {
    card.join(manifest::DIR).join(META_FILE).is_file()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dates() {
        assert_eq!(days_from_civil(1970, 1, 1), 0);
        assert_eq!(days_from_civil(2026, 10, 12), 20_738);
        assert_eq!(parse_date("2018-04-06"), Some(days_from_civil(2018, 4, 6) as u32));
        assert_eq!(parse_date("2018-04-06T07:00:00Z"), parse_date("2018-04-06"));
        assert_eq!(parse_date("06.04.2018"), parse_date("2018-04-06"));
        assert_eq!(parse_date("2018"), None);
        assert_eq!(parse_date("13.13.2018"), None);
        assert_eq!(date_in_name("10467-06.04.2018-ITEMA_21639733-1.mp3"), parse_date("2018-04-06"));
        assert_eq!(date_in_name("Les chemins 4_4.mp3"), None);
        assert_eq!(date_in_name("2026-10-05 - _Phèdre_ de Platon.m4a"), parse_date("2026-10-05"));
        assert_eq!(date_in_name("Show 2026_10_05.mp3"), parse_date("2026-10-05"));
        assert_eq!(date_in_name("id 12026-10-05.mp3"), None);
        assert_eq!(date_in_name("2026-13-05 x.mp3"), None);
    }
}
