//! Card-side operations on the manifest: adopt an existing card, rebuild
//! the index, remove tracks, write the sync report.

use std::collections::BTreeMap;
use std::fmt::Write as _;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};


use crate::audio;
use crate::index::{self, AlbumKey, TrackRecord, crawl};
use crate::manifest::{self, Entry, Manifest, RemovedAlbum, track_key};
use crate::sync::{self, Outcome, Plan};
use crate::tags::{self, TagQuality};
use crate::view::{self, Skipped, SyncReport};

/// Builds a manifest from the audio already on a card (synced by
/// the former `sync/sync.sh` or by hand). The files stay where they are; they get no
/// source, so the first sync from each computer attaches its library files
/// to them by tags. `progress` is called as files are read.
pub fn adopt(card: &Path, progress: &(dyn Fn(AdoptProgress) + Sync)) -> io::Result<Manifest> {
    let mut files = Vec::new();
    let mut stack = vec![card.to_path_buf()];
    while let Some(dir) = stack.pop() {
        for e in fs::read_dir(&dir)?.flatten() {
            let name = e.file_name().to_string_lossy().into_owned();
            let path = e.path();
            // Podcasts/ is the podcast mirror's (podcasts.json), not music.
            let skipped_root = [manifest::DIR, "System Volume Information", crate::podcasts::CARD_DIR];
            if name.starts_with('.') || (dir == card && skipped_root.contains(&name.as_str())) {
                continue;
            }
            if e.file_type()?.is_dir() {
                stack.push(path);
            } else if path.extension().is_some_and(|x| {
                audio::SOURCE_EXTS.contains(&x.to_string_lossy().to_lowercase().as_str())
            }) {
                files.push(path);
            }
        }
    }
    files.sort();
    let now = sync::now_secs();
    let costs: Vec<u64> =
        files.iter().map(|p| tags::probe_cost(p, fs::metadata(p).map_or(0, |m| m.len()))).collect();
    let total = AdoptProgress { files: files.len(), cost: costs.iter().sum(), ..Default::default() };
    let (done, done_cost) = (AtomicUsize::new(0), AtomicU64::new(0));
    progress(total);
    // Workers take files in order (not rayon's split ranges): what an
    // interrupted run left in the page cache is all done first, and the
    // rest goes at the card's steady speed, which keeps the ETA honest.
    // The card reader is the bottleneck, so a few workers are enough.
    let next = AtomicUsize::new(0);
    let read = |path: &PathBuf| {
        let rel = path.strip_prefix(card).unwrap_or(path);
        let p = tags::probe(path, rel);
        let rel_str = rel.components().map(|c| c.as_os_str().to_string_lossy()).collect::<Vec<_>>().join("/");
        adopted_entry(rel_str, fs::metadata(path).map_or(0, |m| m.len()), p, now)
    };
    let mut slots: Vec<Option<Entry>> = (0..files.len()).map(|_| None).collect();
    let results = std::sync::Mutex::new(&mut slots);
    std::thread::scope(|s| {
        for _ in 0..ADOPT_WORKERS {
            s.spawn(|| {
                loop {
                    let i = next.fetch_add(1, Ordering::Relaxed);
                    let Some(path) = files.get(i) else { break };
                    let entry = read(path);
                    results.lock().unwrap()[i] = Some(entry);
                    progress(AdoptProgress {
                        files_done: done.fetch_add(1, Ordering::Relaxed) + 1,
                        cost_done: done_cost.fetch_add(costs[i], Ordering::Relaxed) + costs[i],
                        ..total
                    });
                }
            });
        }
    });
    let tracks = slots.into_iter().flatten().collect();
    Ok(Manifest { tracks, ..Default::default() })
}

/// The manifest entry of a file found on the card (`rel`: card path).
pub fn adopted_entry(rel: String, size: u64, p: tags::Probed, now: u64) -> Entry {
    Entry {
        key: track_key(&p.tags),
        path: rel,
        size,
        tags: p.tags,
        tag_quality: p.quality,
        audio: p.info,
        source_rank: p.info.quality_rank(),
        conversion: "adopted".into(),
        sources: Vec::new(),
        added_at: now,
    }
}

/// Threads reading the card in [`adopt`]: enough to overlap tag parsing
/// with I/O (a USB card reader gives the same ~20 MB/s to 1 or 14 threads).
const ADOPT_WORKERS: usize = 4;

/// Where [`adopt`] is. `cost` weighs each file by the bytes it needs read
/// (a FLAC far more than an MP3), for an even progress bar and ETA.
#[derive(Debug, Default, Clone, Copy)]
pub struct AdoptProgress {
    pub files_done: usize,
    pub files: usize,
    pub cost_done: u64,
    pub cost: u64,
}

/// Loads the card's manifest, adopting the card's content when there is none.
/// Returns (manifest, adopted).
pub fn load_or_adopt(card: &Path, progress: &(dyn Fn(AdoptProgress) + Sync)) -> io::Result<(Manifest, bool)> {
    if manifest::exists(card) {
        return Ok((manifest::load(card)?, false));
    }
    let m = adopt(card, progress)?;
    manifest::save(card, &m)?;
    Ok((m, true))
}

/// Rebuilds `<card>/openpod/*.bin` from the manifest (no tag re-reading;
/// covers come from the card files).
pub fn reindex(card: &Path, m: &Manifest, album_key: AlbumKey) -> io::Result<index::Summary> {
    let records = index_records(m);
    let out = card.join(manifest::DIR);
    fs::create_dir_all(&out)?;
    let opts = index::Options { album_key, ..Default::default() };
    index::write_index(&records, &out, opts, |i| crawl::album_art_source(&card.join(&m.tracks[i].path)))
}

/// The index's input: one record per manifest track, in manifest order.
pub fn index_records(m: &Manifest) -> Vec<TrackRecord> {
    m
        .tracks
        .iter()
        .map(|e| TrackRecord {
            card_path: e.path.clone(),
            file_name: e.path.rsplit('/').next().unwrap_or(&e.path).to_owned(),
            title: e.tags.title.clone(),
            artist: e.tags.artist.clone(),
            album_artist: e.tags.album_artist.clone(),
            album: e.tags.album.clone(),
            genre: e.tags.genre.clone(),
            track_no: e.tags.track,
            year: e.tags.year,
            duration_secs: e.audio.duration,
        })
        .collect()
}

/// Removes now-empty folders above a deleted card file (and cover images
/// left alone in them).
pub fn remove_empty_dirs(card: &Path, rel: &str) {
    let mut dir = card.join(rel);
    while dir.pop() && dir != card && dir.starts_with(card) {
        let Ok(entries) = fs::read_dir(&dir) else { return };
        let names: Vec<PathBuf> = entries.flatten().map(|e| e.path()).collect();
        let only_art = names.iter().all(|p| {
            p.is_file()
                && p.extension().is_some_and(|x| {
                    ["jpg", "jpeg", "png", "webp"].contains(&x.to_string_lossy().to_lowercase().as_str())
                })
        });
        if !only_art {
            return;
        }
        for p in names {
            let _ = fs::remove_file(p);
        }
        if fs::remove_dir(&dir).is_err() {
            return;
        }
    }
}

/// Which tracks `rm` / `ls` select. Matching is case-insensitive; artist
/// matches the track artist or the album artist.
#[derive(Debug, Default, Clone)]
pub struct Filter {
    pub artist: Option<String>,
    pub album: Option<String>,
    /// Card path prefix.
    pub path: Option<String>,
    /// Exact track keys.
    pub keys: Option<std::collections::HashSet<String>>,
}

impl Filter {
    pub fn is_empty(&self) -> bool {
        self.artist.is_none() && self.album.is_none() && self.path.is_none() && self.keys.is_none()
    }

    pub fn matches(&self, e: &Entry) -> bool {
        let eq = |want: &Option<String>, got: &[Option<&str>]| {
            want.as_ref().is_none_or(|w| got.iter().flatten().any(|g| g.eq_ignore_ascii_case(w)))
        };
        eq(&self.artist, &[e.tags.artist.as_deref(), e.tags.album_artist.as_deref()])
            && eq(&self.album, &[e.tags.album.as_deref()])
            && self.keys.as_ref().is_none_or(|k| k.contains(&e.key))
            && self.path.as_ref().is_none_or(|p| {
                let p = p.trim_matches('/').to_lowercase();
                let path = e.path.to_lowercase();
                path == p || path.starts_with(&format!("{p}/"))
            })
    }
}

/// Deletes the selected tracks from the card and the manifest, and records
/// them (album by album) so syncs from any computer skip them. `machine` is
/// the computer doing it. Returns the removed entries.
pub fn remove(card: &Path, m: &mut Manifest, filter: &Filter, machine: &str) -> io::Result<Vec<Entry>> {
    let removed = forget(m, filter, machine);
    for e in &removed {
        match fs::remove_file(card.join(&e.path)) {
            Ok(()) => {}
            Err(err) if err.kind() == io::ErrorKind::NotFound => {}
            Err(err) => return Err(err),
        }
        remove_empty_dirs(card, &e.path);
    }
    manifest::save(card, m)?;
    Ok(removed)
}

/// The manifest side of [`remove`]: drops the selected tracks and records
/// them as removed. The caller deletes their files.
pub fn forget(m: &mut Manifest, filter: &Filter, machine: &str) -> Vec<Entry> {
    let (removed, kept): (Vec<Entry>, Vec<Entry>) = m.tracks.drain(..).partition(|e| filter.matches(e));
    m.tracks = kept;
    let now = sync::now_secs();
    for e in &removed {
        m.removed.insert(e.key.clone());
        m.removed_sources.extend(e.sources.iter().cloned());
        let id = manifest::album_id(&e.tags);
        let i = match m.removed_albums.iter().position(|r| r.id == id) {
            Some(i) => i,
            None => {
                m.removed_albums.push(RemovedAlbum {
                    id,
                    album: e.tags.album.clone().unwrap_or_else(|| "Unknown Album".into()),
                    artist: e
                        .tags
                        .album_artist
                        .clone()
                        .or_else(|| e.tags.artist.clone())
                        .unwrap_or_else(|| "Unknown Artist".into()),
                    keys: Vec::new(),
                    sources: Vec::new(),
                    removed_at: now,
                    machine: machine.to_owned(),
                });
                m.removed_albums.len() - 1
            }
        };
        let r = &mut m.removed_albums[i];
        r.removed_at = now;
        r.machine = machine.to_owned();
        if !r.keys.contains(&e.key) {
            r.keys.push(e.key.clone());
        }
        r.sources.extend(e.sources.iter().cloned());
    }
    removed
}

/// The [`Filter`] selecting whole albums ([`manifest::album_id`]s).
pub fn album_filter(m: &Manifest, ids: &[String]) -> Filter {
    let ids: std::collections::HashSet<&str> = ids.iter().map(String::as_str).collect();
    let keys =
        m.tracks.iter().filter(|e| ids.contains(manifest::album_id(&e.tags).as_str())).map(|e| e.key.clone()).collect();
    Filter { keys: Some(keys), ..Default::default() }
}

/// Removes the given albums from the card ([`manifest::album_id`]s).
pub fn remove_albums(card: &Path, m: &mut Manifest, ids: &[String], machine: &str) -> io::Result<Vec<Entry>> {
    let filter = album_filter(m, ids);
    remove(card, m, &filter, machine)
}

/// Forgets a removal: the next sync copies the album again.
pub fn bring_back(card: &Path, m: &mut Manifest, id: &str) -> io::Result<bool> {
    let found = unforget(m, id);
    if found {
        manifest::save(card, m)?;
    }
    Ok(found)
}

/// The manifest side of [`bring_back`].
pub fn unforget(m: &mut Manifest, id: &str) -> bool {
    let Some(i) = m.removed_albums.iter().position(|r| r.id == id) else { return false };
    let r = m.removed_albums.remove(i);
    for k in &r.keys {
        m.removed.remove(k);
    }
    m.removed_sources.retain(|s| !r.sources.iter().any(|o| o.machine == s.machine && o.path == s.path));
    true
}

/// After [`sync::execute`]: re-copies damaged covers, rebuilds the index if
/// anything changed, writes both reports and remembers the card's write
/// speed. `seconds` is the sync's duration.
pub fn finish_sync(
    card: &Path,
    m: &mut Manifest,
    machine: &str,
    album_key: AlbumKey,
    plan: &PlanInfo,
    out: &Outcome,
    seconds: u64,
) -> io::Result<Finished> {
    let covers = if out.interrupted { 0 } else { sync::refresh_covers(card, m, machine) };
    let index_missing = !card.join(manifest::DIR).join("music_index.bin").is_file();
    let index = if out.written.is_empty() && covers == 0 && !index_missing {
        None
    } else {
        Some(reindex(card, m, album_key)?)
    };
    if note_sync(m, machine, out) {
        manifest::save(card, m)?;
    }
    let report = report(machine, m, plan, out, seconds, index.as_ref());
    let dir = card.join(manifest::DIR);
    fs::write(dir.join(view::REPORT_FILE), serde_json::to_vec_pretty(&report).map_err(io::Error::other)?)?;
    let text_report = dir.join("sync-report.txt");
    fs::write(&text_report, text_report_of(machine, m, plan, out))?;
    Ok(Finished { covers, index, report, text_report })
}

#[derive(Debug)]
pub struct Finished {
    /// Cover images re-copied.
    pub covers: usize,
    /// None when the index didn't need rebuilding.
    pub index: Option<index::Summary>,
    pub report: SyncReport,
    pub text_report: PathBuf,
}

/// Remembers this sync (when it wrote anything) and the card's measured
/// write speed. Returns whether the manifest changed.
pub fn note_sync(m: &mut Manifest, machine: &str, out: &Outcome) -> bool {
    if out.written.is_empty() {
        return false;
    }
    let write_speed = if out.write_secs > 1.0 { (out.bytes as f64 / out.write_secs) as u64 } else { 0 };
    let previous = m.last_sync.as_ref().map_or(0, |s| s.write_speed);
    m.last_sync = Some(manifest::LastSync {
        machine: machine.to_owned(),
        at: sync::now_secs(),
        // A sync too short to measure keeps the previous figure.
        write_speed: if write_speed > 0 { write_speed } else { previous },
    });
    true
}

/// Card folders whose tracks have tags of quality `q`.
fn weak_folders(m: &Manifest, q: TagQuality) -> Vec<String> {
    let mut albums: Vec<String> = m
        .tracks
        .iter()
        .filter(|e| e.tag_quality == q)
        .map(|e| e.path.rsplit_once('/').map_or(e.path.clone(), |(d, _)| d.to_owned()))
        .collect();
    albums.sort();
    albums.dedup();
    albums
}

pub fn report(
    machine: &str,
    m: &Manifest,
    plan: &PlanInfo,
    out: &Outcome,
    seconds: u64,
    index: Option<&index::Summary>,
) -> SyncReport {
    let mut kinds: BTreeMap<String, usize> = BTreeMap::new();
    for (_, desc) in &out.converted {
        if let Some(k) = view::conversion_kind(desc) {
            *kinds.entry(k).or_default() += 1;
        }
    }
    let mut conversions: Vec<(String, usize)> = kinds.into_iter().collect();
    conversions.sort_by(|a, b| b.1.cmp(&a.1));
    let skipped = |v: &[(PathBuf, String)]| {
        v.iter().map(|(p, r)| Skipped { path: p.to_string_lossy().into_owned(), reason: r.clone() }).collect()
    };
    let mut tags_guessed = weak_folders(m, TagQuality::Inferred);
    tags_guessed.extend(weak_folders(m, TagQuality::Unknown));
    tags_guessed.sort();
    tags_guessed.dedup();
    let albums: std::collections::HashSet<String> = m.tracks.iter().map(|e| manifest::album_id(&e.tags)).collect();
    SyncReport {
        machine: machine.to_owned(),
        at: sync::now_secs(),
        seconds,
        interrupted: out.interrupted,
        written: out.written.len(),
        bytes_written: out.bytes,
        upgraded: out.upgraded,
        conversions,
        converted: out.converted.len(),
        skipped: skipped(&plan.unsupported),
        failed: skipped(&out.failed),
        tags_guessed,
        warnings: index.map(|s| s.warnings.clone()).unwrap_or_default(),
        card_tracks: m.tracks.len(),
        card_albums: albums.len(),
        podcasts: None,
    }
}

/// `openpod/sync-report.txt`: what this sync did and what needs attention.
pub fn text_report_of(machine: &str, m: &Manifest, plan_info: &PlanInfo, out: &Outcome) -> String {
    let mut r = String::new();
    let _ = writeln!(r, "OpenPod sync report — machine '{machine}', unix time {}", sync::now_secs());
    let _ = writeln!(
        r,
        "written {}, converted {}, attached {}, unchanged {}, skipped as removed {}, failed {}, unsupported {}{}\n",
        out.written.len(),
        out.converted.len(),
        plan_info.attached,
        plan_info.unchanged,
        plan_info.removed,
        out.failed.len(),
        plan_info.unsupported.len(),
        if out.interrupted { " (INTERRUPTED: run sync again to finish)" } else { "" }
    );
    let section = |r: &mut String, title: &str, lines: Vec<String>| {
        if !lines.is_empty() {
            let _ = writeln!(r, "## {title} ({})", lines.len());
            for l in lines {
                let _ = writeln!(r, "  {l}");
            }
            r.push('\n');
        }
    };
    section(&mut r, "Failed", out.failed.iter().map(|(p, e)| format!("{} — {e}", p.display())).collect());
    section(&mut r, "Unsupported", plan_info.unsupported.iter().map(|(p, e)| format!("{} — {e}", p.display())).collect());
    section(&mut r, "Converted", out.converted.iter().map(|(p, c)| format!("{p} — {c}")).collect());
    section(&mut r, "Folders with tags guessed from the folder layout (whole card)", weak_folders(m, TagQuality::Inferred));
    section(&mut r, "Folders with missing tags, shown as Unknown (whole card)", weak_folders(m, TagQuality::Unknown));
    r
}

/// The parts of a [`Plan`] the report needs (the plan itself is consumed by
/// `execute`).
#[derive(Debug, Default, Clone)]
pub struct PlanInfo {
    pub attached: usize,
    pub unchanged: usize,
    pub removed: usize,
    pub unsupported: Vec<(PathBuf, String)>,
}

impl From<&Plan> for PlanInfo {
    fn from(p: &Plan) -> Self {
        let mut unsupported = p.unsupported.clone();
        unsupported.extend(p.unreadable.iter().cloned());
        Self { attached: p.attach.len(), unchanged: p.unchanged, removed: p.removed, unsupported }
    }
}
