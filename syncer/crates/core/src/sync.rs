//! Additive sync: scan libraries, plan against the card's manifest, then
//! convert in parallel and write to the card from a single thread (SD write
//! speed is the bottleneck, and parallel writes thrash it).
//!
//! Nothing is ever removed from the card here: a track missing from a
//! library may have been added from another computer. Removal is `rm`.

use std::collections::HashMap;
use std::fs;
use std::io::{self, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::mpsc;
use std::time::{Instant, UNIX_EPOCH};

use rayon::prelude::*;

use crate::audio::{self, AudioInfo, Codec};
use crate::convert;
use crate::manifest::{self, Entry, Manifest, SourceRef, track_key};
use crate::paths::{self, Taken};
use crate::policy::{self, Policy};
use crate::tags::{self, Probed};

/// Suffix of a file being written to the card; renamed when complete.
pub const PART_SUFFIX: &str = ".openpod-part";
const CHECKPOINT_EVERY: usize = 25;
const CHECKPOINT_SECS: u64 = 15;

#[derive(Debug, Clone)]
pub struct SourceFile {
    pub abs: PathBuf,
    /// Relative to its library root (feeds the tag fallback).
    pub rel: PathBuf,
    pub size: u64,
    pub mtime: i128,
}

pub fn now_secs() -> u64 {
    #[cfg(all(target_arch = "wasm32", target_os = "unknown"))]
    return (js_sys::Date::now() / 1000.0) as u64;
    #[cfg(not(all(target_arch = "wasm32", target_os = "unknown")))]
    std::time::SystemTime::now().duration_since(UNIX_EPOCH).map_or(0, |d| d.as_secs())
}

pub(crate) fn mtime_ns(meta: &fs::Metadata) -> i128 {
    meta.modified()
        .ok()
        .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
        .map_or(0, |d| d.as_nanos() as i128)
}

/// Audio files under each library, minus excluded library-relative paths
/// (an excluded folder excludes everything below it). Hidden entries are
/// skipped.
pub fn scan(libraries: &[PathBuf], excludes: &[String]) -> io::Result<Vec<SourceFile>> {
    let excludes: Vec<PathBuf> =
        excludes.iter().map(|e| PathBuf::from(e.trim_matches('/'))).filter(|e| !e.as_os_str().is_empty()).collect();
    let mut out = Vec::new();
    for lib in libraries {
        let lib = fs::canonicalize(lib)?;
        let mut stack = vec![lib.clone()];
        while let Some(dir) = stack.pop() {
            let entries = match fs::read_dir(&dir) {
                Ok(e) => e,
                Err(e) => {
                    eprintln!("warning: cannot read {}: {e}", dir.display());
                    continue;
                }
            };
            for entry in entries.flatten() {
                let name = entry.file_name();
                if name.to_string_lossy().starts_with('.') {
                    continue;
                }
                let path = entry.path();
                let rel = path.strip_prefix(&lib).unwrap_or(&path).to_path_buf();
                if excludes.iter().any(|e| rel.starts_with(e)) {
                    continue;
                }
                let Ok(meta) = fs::metadata(&path) else { continue };
                if meta.is_dir() {
                    stack.push(path);
                } else if meta.is_file()
                    && path
                        .extension()
                        .is_some_and(|e| audio::SOURCE_EXTS.contains(&e.to_string_lossy().to_lowercase().as_str()))
                {
                    out.push(SourceFile { abs: path, rel, size: meta.len(), mtime: mtime_ns(&meta) });
                }
            }
        }
    }
    out.sort_by(|a, b| a.abs.cmp(&b.abs));
    Ok(out)
}

/// xxh3 of the size, the first and the last 64 KiB: tells identical files
/// apart without reading whole albums.
pub fn content_hash(path: &Path, size: u64) -> io::Result<String> {
    content_hash_reader(&mut fs::File::open(path)?, size)
}

/// [`content_hash`] on any reader.
pub fn content_hash_reader<R: Read + Seek>(f: &mut R, size: u64) -> io::Result<String> {
    const SPAN: u64 = 64 * 1024;
    f.seek(SeekFrom::Start(0))?;
    let mut buf = Vec::with_capacity(2 * SPAN as usize + 8);
    buf.extend_from_slice(&size.to_le_bytes());
    (&mut *f).take(SPAN).read_to_end(&mut buf)?;
    if size > SPAN {
        f.seek(SeekFrom::Start(size.saturating_sub(SPAN).max(SPAN)))?;
        f.take(SPAN).read_to_end(&mut buf)?;
    }
    Ok(format!("{:016x}", xxhash_rust::xxh3::xxh3_64(&buf)))
}

fn source_ref(machine: &str, f: &SourceFile, hash: String) -> SourceRef {
    SourceRef { machine: machine.to_owned(), path: f.abs.to_string_lossy().into_owned(), size: f.size, mtime: f.mtime, hash }
}

/// A file to write to the card.
#[derive(Debug)]
pub struct Write {
    pub source: SourceFile,
    pub probed: Probed,
    pub key: String,
    pub policy: Policy,
    /// The library file written (None when fixing a file already on the card).
    pub source_ref: Option<SourceRef>,
    /// Other library files with the same recording (lower or equal quality).
    pub also_from: Vec<SourceRef>,
    pub estimated_size: u64,
    /// `Some(entry)`: replaces that manifest entry instead of adding one.
    pub replaces: Option<usize>,
    pub reason: WriteReason,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WriteReason {
    New,
    /// Better quality than the copy on the card.
    Upgrade,
    /// The library file this track came from changed (retagged…).
    SourceChanged,
    /// A file already on the card (adopted) that the player can't play
    /// well: converted in place.
    Fix,
}

#[derive(Debug, Default)]
pub struct Plan {
    pub writes: Vec<Write>,
    /// (entry, source): library files whose recording is already on the card.
    pub attach: Vec<(usize, SourceRef)>,
    pub unchanged: usize,
    pub unsupported: Vec<(PathBuf, String)>,
    pub unreadable: Vec<(PathBuf, String)>,
    /// Library files skipped because their track was removed with `rm`.
    pub removed: usize,
    /// Removed tracks allowed back (`include_removed`).
    pub restore: Vec<String>,
}

impl Plan {
    pub fn bytes_needed(&self) -> u64 {
        self.writes.iter().map(|w| w.estimated_size).sum()
    }
    pub fn is_empty(&self) -> bool {
        self.writes.is_empty() && self.attach.is_empty()
    }
}

pub struct PlanOptions {
    pub machine: String,
    pub have_ffmpeg: bool,
    pub card: PathBuf,
    /// Sync tracks that were removed with `rm` again.
    pub include_removed: bool,
}

/// Compares the scanned library files with the manifest, reading the files
/// that need it (hashes, tags) in parallel.
pub fn plan(m: &Manifest, files: Vec<SourceFile>, opts: &PlanOptions) -> Plan {
    let t = triage(m, files, opts);
    let hashes: Vec<io::Result<String>> = t.to_hash().par_iter().map(|f| content_hash(&f.abs, f.size)).collect();
    let mt = match_hashes(m, t, hashes, opts);
    let probed: Vec<Probed> = mt.to_probe().par_iter().map(|f| tags::probe(&f.abs, &f.rel)).collect();
    finish(m, mt, probed, opts, &|rel| {
        let meta = fs::metadata(opts.card.join(rel)).ok()?;
        Some((meta.len(), mtime_ns(&meta)))
    })
}

/// First planning step: files unchanged since the last sync are counted,
/// the others need their content hash ([`Triage::to_hash`]).
pub struct Triage {
    plan: Plan,
    /// (file, entry it was previously synced into, if its source changed)
    candidates: Vec<(SourceFile, Option<usize>)>,
}

impl Triage {
    /// Files whose [`content_hash`] [`match_hashes`] needs, in order.
    pub fn to_hash(&self) -> Vec<&SourceFile> {
        self.candidates.iter().map(|(f, _)| f).collect()
    }
}

pub fn triage(m: &Manifest, files: Vec<SourceFile>, opts: &PlanOptions) -> Triage {
    let mut by_source: HashMap<(&str, &str), (usize, &SourceRef)> = HashMap::new();
    for (i, e) in m.tracks.iter().enumerate() {
        for s in &e.sources {
            by_source.insert((&s.machine, &s.path), (i, s));
        }
    }
    let removed_sources: HashMap<(&str, &str), &SourceRef> =
        m.removed_sources.iter().map(|s| ((s.machine.as_str(), s.path.as_str()), s)).collect();

    let mut plan = Plan::default();
    let mut candidates = Vec::new();
    for f in files {
        let path = f.abs.to_string_lossy();
        if !opts.include_removed {
            if let Some(s) = removed_sources.get(&(opts.machine.as_str(), path.as_ref())) {
                if s.size == f.size && s.mtime == f.mtime {
                    plan.removed += 1;
                    continue;
                }
            }
        }
        match by_source.get(&(opts.machine.as_str(), path.as_ref())) {
            Some((_, s)) if s.size == f.size && s.mtime == f.mtime => plan.unchanged += 1,
            Some(&(entry, _)) => candidates.push((f, Some(entry))),
            None => candidates.push((f, None)),
        }
    }
    Triage { plan, candidates }
}

/// Second step: files whose bytes are already on the card are attached to
/// it; the others need their tags read ([`Matched::to_probe`]).
pub struct Matched {
    plan: Plan,
    to_probe: Vec<(SourceFile, Option<usize>, String)>,
}

impl Matched {
    /// Files [`finish`] needs [`tags::probe`]d, in order.
    pub fn to_probe(&self) -> Vec<&SourceFile> {
        self.to_probe.iter().map(|(f, _, _)| f).collect()
    }
}

/// `hashes` follows [`Triage::to_hash`].
pub fn match_hashes(m: &Manifest, t: Triage, hashes: Vec<io::Result<String>>, opts: &PlanOptions) -> Matched {
    let mut by_hash: HashMap<&str, usize> = HashMap::new();
    for (i, e) in m.tracks.iter().enumerate() {
        for s in &e.sources {
            by_hash.entry(&s.hash).or_insert(i);
        }
    }
    let Triage { mut plan, candidates } = t;
    let mut to_probe = Vec::new();
    for ((f, prev), hash) in candidates.into_iter().zip(hashes) {
        let hash = match hash {
            Ok(h) => h,
            Err(e) => {
                plan.unreadable.push((f.abs, e.to_string()));
                continue;
            }
        };
        match (prev, by_hash.get(hash.as_str())) {
            // Same bytes as a file already on the card (touched, moved,
            // copied, or synced from another computer).
            (_, Some(&entry)) => plan.attach.push((entry, source_ref(&opts.machine, &f, hash))),
            (prev, None) => to_probe.push((f, prev, hash)),
        }
    }
    Matched { plan, to_probe }
}

/// Last step: groups recordings, picks the best copy, decides conversions,
/// and converts adopted card files the player can't play well. `probed`
/// follows [`Matched::to_probe`]; `card_file(rel)` gives a card file's
/// (size, mtime).
pub fn finish(
    m: &Manifest,
    mt: Matched,
    probed: Vec<Probed>,
    opts: &PlanOptions,
    card_file: &dyn Fn(&str) -> Option<(u64, i128)>,
) -> Plan {
    let mut by_key: HashMap<&str, usize> = HashMap::new();
    for (i, e) in m.tracks.iter().enumerate() {
        by_key.entry(&e.key).or_insert(i);
    }
    let Matched { mut plan, to_probe } = mt;

    // Group by recording: the best copy is written, the others are recorded
    // as extra sources.
    let mut groups: HashMap<String, Vec<(SourceFile, Option<usize>, String, Probed)>> = HashMap::new();
    let mut order = Vec::new();
    for ((f, prev, hash), p) in to_probe.into_iter().zip(probed) {
        let key = track_key(&p.tags);
        if !groups.contains_key(&key) {
            order.push(key.clone());
        }
        groups.entry(key).or_default().push((f, prev, hash, p));
    }

    for key in order {
        let mut group = groups.remove(&key).unwrap();
        if m.removed.contains(&key) {
            if !opts.include_removed {
                plan.removed += group.len();
                continue;
            }
            plan.restore.push(key.clone());
        }
        group.sort_by_key(|(_, _, _, p)| std::cmp::Reverse(p.info.quality_rank()));
        let mut it = group.into_iter();
        let (f, prev, hash, p) = it.next().unwrap();
        let mut also_from: Vec<SourceRef> = Vec::new();
        for (of, _, oh, _) in it {
            also_from.push(source_ref(&opts.machine, &of, oh));
        }
        let policy = policy::decide(&p.info, opts.have_ffmpeg);
        if let Policy::Unsupported(reason) = &policy {
            plan.unsupported.push((f.abs, reason.clone()));
            continue;
        }
        let rank = p.info.quality_rank();
        let existing = by_key.get(key.as_str()).copied();
        let sref = Some(source_ref(&opts.machine, &f, hash));
        let (replaces, reason) = match (existing, prev) {
            // Retagged/re-encoded library file: rewrite its card copy, unless
            // its recording now matches another track already on the card.
            (None, Some(prev)) => (Some(prev), WriteReason::SourceChanged),
            (Some(e), Some(prev)) if e == prev => (Some(e), WriteReason::SourceChanged),
            (Some(e), _) if rank > m.tracks[e].source_rank => (Some(e), WriteReason::Upgrade),
            (Some(e), _) => {
                plan.attach.extend(sref.map(|s| (e, s)));
                plan.attach.extend(also_from.into_iter().map(|s| (e, s)));
                continue;
            }
            (None, None) => (None, WriteReason::New),
        };
        let estimated_size = policy::estimated_size(&policy, &p.info, f.size);
        plan.writes.push(Write {
            source: f,
            probed: p,
            key,
            policy,
            source_ref: sref,
            also_from,
            estimated_size,
            replaces,
            reason,
        });
    }

    // Files already on the card (adopted from a card filled by hand or by
    // sync.sh) that the player can't play well: convert them in place.
    let replaced: Vec<usize> = plan.writes.iter().filter_map(|w| w.replaces).collect();
    for (i, e) in m.tracks.iter().enumerate() {
        if e.conversion != "adopted" || replaced.contains(&i) {
            continue;
        }
        let policy = policy::decide(&e.audio, opts.have_ffmpeg);
        if matches!(policy, Policy::Copy | Policy::Unsupported(_)) {
            continue;
        }
        let Some((size, mtime)) = card_file(&e.path) else { continue };
        plan.writes.push(Write {
            source: SourceFile { abs: opts.card.join(&e.path), rel: PathBuf::from(&e.path), size, mtime },
            probed: Probed { tags: e.tags.clone(), quality: e.tag_quality, info: e.audio },
            key: e.key.clone(),
            estimated_size: policy::estimated_size(&policy, &e.audio, size),
            policy,
            source_ref: None,
            also_from: Vec::new(),
            replaces: Some(i),
            reason: WriteReason::Fix,
        });
    }
    plan
}

/// Progress reporting hooks for the UI.
pub trait Progress: Sync {
    fn start(&self, files: usize, bytes: u64);
    /// A file landed on the card (or failed). `bytes` is its share of the
    /// total given to `start` (its estimated size).
    fn advance(&self, bytes: u64, label: &str);
    fn warn(&self, msg: &str);
    /// Bytes of the file being written so far (`dest` is its card path).
    fn copying(&self, _dest: &str, _done: u64, _total: u64) {}
    /// Converter `worker` started on `src` (`what` describes the
    /// conversion), or finished (`what` None).
    fn converting(&self, _worker: usize, _src: &Path, _what: Option<&str>) {}
}

#[derive(Debug, Default)]
pub struct Outcome {
    pub written: Vec<String>,
    pub converted: Vec<(String, String)>,
    pub upgraded: usize,
    pub failed: Vec<(PathBuf, String)>,
    pub interrupted: bool,
    /// Bytes written to the card, and the time spent writing them (not
    /// waiting for conversions): the card's write speed.
    pub bytes: u64,
    pub write_secs: f64,
}

enum Ready {
    /// Copy straight from the library.
    Source(usize),
    /// Converted into a temp file.
    Converted(usize, PathBuf),
    Failed(usize, String),
}

/// Files kept next to the tracks and copied along with them.
const COVER_FILES: [&str; 4] = ["cover", "folder", "album", "front"];
const COVER_EXTS: [&str; 4] = ["jpg", "jpeg", "png", "webp"];

/// Copies the album folder's cover images; returns how many were written.
pub(crate) fn copy_cover_images(src_dir: &Path, card_dir: &Path) -> usize {
    let mut n = 0;
    let Ok(entries) = fs::read_dir(src_dir) else { return 0 };
    for e in entries.flatten() {
        let name = e.file_name().to_string_lossy().into_owned();
        let lower = name.to_lowercase();
        let is_cover = lower
            .rsplit_once('.')
            .is_some_and(|(stem, ext)| COVER_FILES.contains(&stem) && COVER_EXTS.contains(&ext));
        let dst = card_dir.join(&name);
        // Re-copied when the size differs: an empty or truncated cover is
        // what an unplug before the data reached the card leaves behind.
        let same = |src: &Path| fs::metadata(&dst).ok().map(|m| m.len()) == fs::metadata(src).ok().map(|m| m.len());
        if is_cover && !same(&e.path()) && write_file(&e.path(), &dst, &|_, _| {}).is_ok() {
            n += 1;
        }
    }
    n
}

/// Checks the cover images of every album this machine has a source for
/// (sizes only, so it's cheap) and re-copies missing or damaged ones.
/// Returns how many were written.
pub fn refresh_covers(card: &Path, m: &Manifest, machine: &str) -> usize {
    let mut dirs = std::collections::BTreeSet::new();
    for e in &m.tracks {
        if let Some(s) = e.sources.iter().find(|s| s.machine == machine) {
            if let (Some(src), Some(dst)) = (Path::new(&s.path).parent(), card.join(&e.path).parent()) {
                dirs.insert((src.to_path_buf(), dst.to_path_buf()));
            }
        }
    }
    dirs.iter().filter(|(_, dst)| dst.is_dir()).map(|(src, dst)| copy_cover_images(src, dst)).sum()
}

/// Writes `src` to `card/rel` through a `.openpod-part` file, and waits
/// until it is on the card: otherwise the OS buffers gigabytes in RAM, the
/// progress shows memory speed instead of the card's, and pulling the card
/// after an interrupted sync loses files the manifest says are there.
pub(crate) fn write_to_card(src: &Path, card: &Path, rel: &str, on_bytes: &dyn Fn(u64, u64)) -> io::Result<u64> {
    let dst = card.join(rel);
    if let Some(dir) = dst.parent() {
        fs::create_dir_all(dir)?;
    }
    write_file(src, &dst, on_bytes)
}

/// Data written between flushes, so per-file progress follows the card.
const FLUSH_EVERY: u64 = 8 * 1024 * 1024;

/// Copies through a `.openpod-part` file, synced before the rename.
/// `on_bytes(done, total)` follows the bytes that reached the card.
fn write_file(src: &Path, dst: &Path, on_bytes: &dyn Fn(u64, u64)) -> io::Result<u64> {
    use std::io::Write as _;
    let part = PathBuf::from(format!("{}{PART_SUFFIX}", dst.display()));
    let mut input = fs::File::open(src)?;
    let total = input.metadata()?.len();
    let mut out = fs::File::create(&part)?;
    let mut buf = vec![0u8; 1024 * 1024];
    let (mut done, mut flushed) = (0u64, 0u64);
    loop {
        let n = input.read(&mut buf)?;
        if n == 0 {
            break;
        }
        out.write_all(&buf[..n])?;
        done += n as u64;
        if done - flushed >= FLUSH_EVERY {
            out.sync_data()?;
            flushed = done;
            on_bytes(done, total);
        }
    }
    out.sync_all()?;
    on_bytes(done, total);
    drop(out);
    fs::rename(&part, dst)?;
    Ok(done)
}

/// Deletes `.openpod-part` leftovers of an interrupted sync.
pub fn remove_partial_files(card: &Path) -> usize {
    let mut removed = 0;
    let mut stack = vec![card.to_path_buf()];
    while let Some(dir) = stack.pop() {
        let Ok(entries) = fs::read_dir(&dir) else { continue };
        for e in entries.flatten() {
            let p = e.path();
            if e.file_type().is_ok_and(|t| t.is_dir()) {
                stack.push(p);
            } else if p.to_string_lossy().ends_with(PART_SUFFIX) && fs::remove_file(&p).is_ok() {
                removed += 1;
            }
        }
    }
    removed
}

/// Audio properties of the file written to the card.
fn output_info(policy: &Policy, src: &AudioInfo, size: u64) -> AudioInfo {
    let bitrate = |d: f64| if d > 0.0 { (size as f64 * 8.0 / d / 1000.0) as u32 } else { 0 };
    match policy {
        Policy::Flac(t) | Policy::FfmpegFlac(t) => AudioInfo {
            codec: Codec::Flac,
            sample_rate: t.rate,
            bits: t.bits,
            channels: t.channels,
            bitrate: bitrate(src.duration),
            duration: src.duration,
            lpc_order: 10,
        },
        Policy::FfmpegMp3 => AudioInfo {
            codec: Codec::Mp3,
            sample_rate: src.sample_rate.min(audio::MAX_RATE),
            bits: 0,
            channels: src.channels.min(2),
            bitrate: bitrate(src.duration),
            duration: src.duration,
            lpc_order: 0,
        },
        _ => *src,
    }
}

/// A library file belongs to one card track: forget it elsewhere (and in
/// the removed list, if it was brought back).
fn detach(m: &mut Manifest, s: &SourceRef) {
    let same = |o: &SourceRef| o.machine == s.machine && o.path == s.path;
    for e in &mut m.tracks {
        e.sources.retain(|o| !same(o));
    }
    m.removed_sources.retain(|o| !same(o));
}

/// Applies the plan's links (library files whose recording is already on
/// the card) and restores; returns the files left to write.
pub fn apply_links(m: &mut Manifest, plan: Plan) -> Vec<Write> {
    for (entry, s) in plan.attach {
        detach(m, &s);
        m.tracks[entry].sources.push(s);
    }
    for key in &plan.restore {
        m.removed.remove(key);
    }
    plan.writes
}

/// Card path of each write, decided up front so names are deterministic.
pub fn card_paths(m: &Manifest, writes: &[Write]) -> Vec<String> {
    let mut taken = Taken::default();
    for e in &m.tracks {
        taken.insert(&e.path);
    }
    let mut dests = Vec::with_capacity(writes.len());
    for w in writes {
        let ext = w.source.abs.extension().map(|e| e.to_string_lossy().to_lowercase()).unwrap_or_default();
        let wanted = paths::card_path(&w.probed.tags, w.policy.output_ext(&ext));
        let current = w.replaces.map(|e| m.tracks[e].path.clone());
        let dest = match current {
            // Fixing a card file: same place, new extension if needed.
            Some(cur) if w.reason == WriteReason::Fix => {
                let fixed = match cur.rsplit_once('.') {
                    Some((stem, _)) => format!("{stem}.{}", w.policy.output_ext(&ext)),
                    None => cur.clone(),
                };
                if fixed.eq_ignore_ascii_case(&cur) { cur } else { taken.claim(&fixed) }
            }
            // Keep a replaced track's path when it is still the one we'd pick.
            Some(cur) if cur.eq_ignore_ascii_case(&wanted) => cur,
            _ => taken.claim(&wanted),
        };
        dests.push(dest);
    }
    dests
}

/// Records a file written to the card at `dest` (`size` bytes) in the
/// manifest and in `out`. Returns the card path of the copy it replaced
/// when that one lived elsewhere: the caller deletes it.
pub fn record_write(m: &mut Manifest, w: &Write, dest: &str, size: u64, out: &mut Outcome) -> Option<String> {
    let conversion = convert::describe(&w.policy, &w.probed.info);
    if w.policy != Policy::Copy {
        out.converted.push((dest.to_owned(), conversion.clone()));
    }
    let mut sources: Vec<SourceRef> = w.source_ref.iter().cloned().collect();
    sources.extend(w.also_from.iter().cloned());
    for s in &sources {
        detach(m, s);
    }
    let entry = Entry {
        key: w.key.clone(),
        path: dest.to_owned(),
        size,
        tags: w.probed.tags.clone(),
        tag_quality: w.probed.quality,
        audio: output_info(&w.policy, &w.probed.info, size),
        source_rank: if w.reason == WriteReason::Fix {
            m.tracks[w.replaces.unwrap()].source_rank
        } else {
            w.probed.info.quality_rank()
        },
        conversion,
        sources,
        added_at: now_secs(),
    };
    out.written.push(dest.to_owned());
    out.bytes += size;
    out.upgraded += usize::from(w.reason == WriteReason::Upgrade);
    match w.replaces {
        Some(e) => {
            let old = std::mem::replace(&mut m.tracks[e], entry);
            // Keep the other computers' sources of the old copy.
            let keep: Vec<SourceRef> = old
                .sources
                .into_iter()
                .filter(|s| !m.tracks[e].sources.iter().any(|n| n.machine == s.machine && n.path == s.path))
                .collect();
            m.tracks[e].sources.extend(keep);
            (!old.path.eq_ignore_ascii_case(dest)).then_some(old.path)
        }
        None => {
            m.tracks.push(entry);
            None
        }
    }
}

/// Runs the plan. `m` is updated in place and checkpointed to the card as
/// files land, so an interrupted run (`stop` set) loses nothing.
pub fn execute(
    card: &Path,
    m: &mut Manifest,
    plan: Plan,
    jobs: usize,
    stop: &AtomicBool,
    progress: &dyn Progress,
) -> io::Result<Outcome> {
    let mut out = Outcome::default();
    let writes = apply_links(m, plan);
    manifest::save(card, m)?;
    if writes.is_empty() {
        return Ok(out);
    }
    let tmp = std::env::temp_dir().join(format!("openpod-{}", std::process::id()));
    fs::create_dir_all(&tmp)?;
    let dests = card_paths(m, &writes);

    progress.start(writes.len(), writes.iter().map(|w| w.estimated_size).sum());
    let next = AtomicUsize::new(0);
    let (tx, rx) = mpsc::sync_channel::<Ready>(jobs * 2);
    let result = std::thread::scope(|scope| -> io::Result<()> {
        for worker in 0..jobs {
            let tx = tx.clone();
            let (writes, next, tmp) = (&writes, &next, &tmp);
            scope.spawn(move || {
                loop {
                    if stop.load(Ordering::Relaxed) {
                        break;
                    }
                    let i = next.fetch_add(1, Ordering::Relaxed);
                    let Some(w) = writes.get(i) else { break };
                    let msg = if w.policy == Policy::Copy {
                        Ready::Source(i)
                    } else {
                        let ext = w.policy.output_ext("flac");
                        let dst = tmp.join(format!("{i}.{ext}"));
                        progress.converting(worker, &w.source.abs, Some(&convert::describe(&w.policy, &w.probed.info)));
                        let r = convert::run(&w.policy, &w.source.abs, &dst);
                        progress.converting(worker, &w.source.abs, None);
                        match r {
                            Ok(()) => Ready::Converted(i, dst),
                            Err(e) => {
                                let _ = fs::remove_file(&dst);
                                Ready::Failed(i, e.to_string())
                            }
                        }
                    };
                    if tx.send(msg).is_err() {
                        break;
                    }
                }
            });
        }
        drop(tx);

        let mut since_save = 0;
        let mut last_save = Instant::now();
        for msg in rx {
            let (i, file, temp) = match msg {
                Ready::Source(i) => (i, writes[i].source.abs.clone(), false),
                Ready::Converted(i, p) => (i, p, true),
                Ready::Failed(i, e) => {
                    progress.warn(&format!("{}: {e}", writes[i].source.abs.display()));
                    out.failed.push((writes[i].source.abs.clone(), e));
                    progress.advance(writes[i].estimated_size, "");
                    continue;
                }
            };
            if stop.load(Ordering::Relaxed) {
                if temp {
                    let _ = fs::remove_file(&file);
                }
                continue;
            }
            let w = &writes[i];
            let dest = &dests[i];
            let t0 = Instant::now();
            let written = write_to_card(&file, card, dest, &|done, total| progress.copying(dest, done, total));
            out.write_secs += t0.elapsed().as_secs_f64();
            if temp {
                let _ = fs::remove_file(&file);
            }
            let size = match written {
                Ok(s) => s,
                Err(e) => {
                    progress.warn(&format!("writing {dest}: {e}"));
                    out.failed.push((w.source.abs.clone(), e.to_string()));
                    progress.advance(w.estimated_size, "");
                    if e.kind() == io::ErrorKind::StorageFull {
                        stop.store(true, Ordering::Relaxed);
                    }
                    continue;
                }
            };
            if let (Some(src_dir), Some(card_dir)) = (w.source.abs.parent(), card.join(dest).parent()) {
                let _ = copy_cover_images(src_dir, card_dir);
            }

            if let Some(old) = record_write(m, w, dest, size, &mut out) {
                let _ = fs::remove_file(card.join(&old));
                crate::library::remove_empty_dirs(card, &old);
            }
            progress.advance(w.estimated_size, dest);

            since_save += 1;
            if since_save >= CHECKPOINT_EVERY || last_save.elapsed().as_secs() >= CHECKPOINT_SECS {
                manifest::save(card, m)?;
                since_save = 0;
                last_save = Instant::now();
            }
        }
        Ok(())
    });
    let _ = fs::remove_dir_all(&tmp);
    manifest::save(card, m)?;
    result?;
    out.interrupted = stop.load(Ordering::Relaxed);
    Ok(out)
}
