//! Additive sync: scan libraries, plan against the card's manifest, then
//! convert in parallel and write to the card from a single thread (SD write
//! speed is the bottleneck, and parallel writes thrash it).
//!
//! Nothing is ever removed from the card here: a track missing from a
//! library may have been added from another computer. Removal is `rm`.

use std::collections::{HashMap, VecDeque};
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
pub const PART_SUFFIX: &str = ".opentone-part";
pub(crate) const CHECKPOINT_EVERY: usize = 25;
pub(crate) const CHECKPOINT_SECS: u64 = 15;

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
    /// (entry, LPC order) found for adopted card files ([`apply_lpc`]),
    /// saved to the manifest so each is read once.
    pub lpc: Vec<(usize, u8)>,
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
    let probed: Vec<Probed> = mt.to_probe().par_iter().map(|f| tags::probe(&f.abs, &f.rel, false)).collect();
    let card_file = |rel: &str| {
        let meta = fs::metadata(opts.card.join(rel)).ok()?;
        Some((meta.len(), mtime_ns(&meta)))
    };
    let mut plan = finish(m, mt, probed, opts, &card_file);
    let checks = lpc_needed(m, &plan, opts);
    let orders: Vec<Option<u8>> = checks
        .par_iter()
        .map(|c| tags::lpc_order(&if c.on_card { opts.card.join(&c.path) } else { c.path.clone() }))
        .collect();
    apply_lpc(m, &mut plan, &checks, &orders, opts, &card_file);
    plan
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
    // An unknown LPC order waits for [`apply_lpc`].
    for (i, e) in m.tracks.iter().enumerate() {
        if e.conversion != "adopted" || replaced.contains(&i) || policy::needs_lpc(&e.audio) {
            continue;
        }
        plan.writes.extend(fix_write(i, e, e.audio, opts, card_file));
    }
    plan
}

/// The in-place conversion of adopted card file `e` (entry `i`) with
/// properties `info`, if the player needs one.
fn fix_write(
    i: usize,
    e: &Entry,
    info: AudioInfo,
    opts: &PlanOptions,
    card_file: &dyn Fn(&str) -> Option<(u64, i128)>,
) -> Option<Write> {
    let policy = policy::decide(&info, opts.have_ffmpeg);
    if matches!(policy, Policy::Copy | Policy::Unsupported(_)) {
        return None;
    }
    let (size, mtime) = card_file(&e.path)?;
    Some(Write {
        source: SourceFile { abs: opts.card.join(&e.path), rel: PathBuf::from(&e.path), size, mtime },
        probed: Probed { tags: e.tags.clone(), quality: e.tag_quality, info },
        key: e.key.clone(),
        estimated_size: policy::estimated_size(&policy, &info, size),
        policy,
        source_ref: None,
        also_from: Vec::new(),
        replaces: Some(i),
        reason: WriteReason::Fix,
    })
}

/// A FLAC whose LPC order [`finish`] left unknown ([`audio::LPC_UNKNOWN`]).
#[derive(Debug)]
pub struct LpcCheck {
    /// The file to read: a library path, or a card path when `on_card`.
    pub path: PathBuf,
    pub on_card: bool,
    target: LpcTarget,
}

#[derive(Debug, Clone, Copy)]
enum LpcTarget {
    Write(usize),
    Entry(usize),
}

/// The FLACs whose LPC order the plan hangs on: library files about to be
/// written (copy or re-encode), and adopted card files (fixed in place when
/// too high). Files attached to a track already on the card aren't read. An
/// adopted file is read through a library file of the same size attached
/// to it (the same encode, on a faster disk), else on the card.
pub fn lpc_needed(m: &Manifest, plan: &Plan, opts: &PlanOptions) -> Vec<LpcCheck> {
    let mut out = Vec::new();
    for (i, w) in plan.writes.iter().enumerate() {
        if w.source_ref.is_some() && policy::needs_lpc(&w.probed.info) {
            out.push(LpcCheck { path: w.source.abs.clone(), on_card: false, target: LpcTarget::Write(i) });
        }
    }
    let replaced: Vec<usize> = plan.writes.iter().filter_map(|w| w.replaces).collect();
    for (i, e) in m.tracks.iter().enumerate() {
        if e.conversion != "adopted" || replaced.contains(&i) || !policy::needs_lpc(&e.audio) {
            continue;
        }
        let same = |s: &&SourceRef| s.machine == opts.machine && s.size == e.size;
        let attached = plan.attach.iter().filter(|(t, _)| *t == i).map(|(_, s)| s);
        let path = match attached.chain(&e.sources).find(same) {
            Some(s) => LpcCheck { path: PathBuf::from(&s.path), on_card: false, target: LpcTarget::Entry(i) },
            None => LpcCheck { path: PathBuf::from(&e.path), on_card: true, target: LpcTarget::Entry(i) },
        };
        out.push(path);
    }
    out
}

/// Completes the plan with the `orders` read for `checks` (None: unreadable,
/// left unknown and asked again next time): decides the writes' policies
/// and adds the fixes the adopted files need.
pub fn apply_lpc(
    m: &Manifest,
    plan: &mut Plan,
    checks: &[LpcCheck],
    orders: &[Option<u8>],
    opts: &PlanOptions,
    card_file: &dyn Fn(&str) -> Option<(u64, i128)>,
) {
    for (c, o) in checks.iter().zip(orders) {
        let Some(o) = *o else { continue };
        match c.target {
            LpcTarget::Write(i) => {
                let w = &mut plan.writes[i];
                w.probed.info.lpc_order = o;
                w.policy = policy::decide(&w.probed.info, opts.have_ffmpeg);
                w.estimated_size = policy::estimated_size(&w.policy, &w.probed.info, w.source.size);
            }
            LpcTarget::Entry(i) => {
                plan.lpc.push((i, o));
                let e = &m.tracks[i];
                let info = AudioInfo { lpc_order: o, ..e.audio };
                plan.writes.extend(fix_write(i, e, info, opts, card_file));
            }
        }
    }
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
    /// The files ready to be written next, in order (card paths).
    fn queued(&self, _dests: &[&str]) {}
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
        if is_cover && !same(&e.path()) && start_file(&e.path(), dst.clone(), &|_, _| {}).and_then(Pending::finish).is_ok() {
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

/// A file whose bytes are handed to the OS but not yet on the card.
pub(crate) struct Pending {
    out: fs::File,
    part: PathBuf,
    dst: PathBuf,
    size: u64,
}

impl Pending {
    /// Waits until the file is on the card, then gives it its name.
    pub(crate) fn finish(self) -> io::Result<u64> {
        self.out.sync_all()?;
        drop(self.out);
        fs::rename(&self.part, &self.dst)?;
        Ok(self.size)
    }
}

/// Copies `src` to `card/rel` through a `.opentone-part` file, named by
/// [`Pending::finish`] once it is on the card: otherwise the OS buffers
/// gigabytes in RAM, the progress shows memory speed instead of the card's,
/// and pulling the card after an interrupted sync loses files the manifest
/// says are there.
/// It isn't synced along the way (a sync every few MB stalls the card
/// between them, ~6%), so `on_bytes(done, total)` follows the bytes handed
/// to the OS: a file's bar runs ahead, then [`Pending::finish`] waits.
pub(crate) fn start_write(src: &Path, card: &Path, rel: &str, on_bytes: &dyn Fn(u64, u64)) -> io::Result<Pending> {
    let dst = card.join(rel);
    if let Some(dir) = dst.parent() {
        fs::create_dir_all(dir)?;
    }
    start_file(src, dst, on_bytes)
}

/// [`start_write`] to `dst`, whose folder exists.
fn start_file(src: &Path, dst: PathBuf, on_bytes: &dyn Fn(u64, u64)) -> io::Result<Pending> {
    use std::io::Write as _;
    let part = PathBuf::from(format!("{}{PART_SUFFIX}", dst.display()));
    let mut input = fs::File::open(src)?;
    let total = input.metadata()?.len();
    let mut out = fs::File::create(&part)?;
    let mut buf = vec![0u8; 1024 * 1024];
    let mut done = 0u64;
    loop {
        let n = input.read(&mut buf)?;
        if n == 0 {
            break;
        }
        out.write_all(&buf[..n])?;
        done += n as u64;
        on_bytes(done, total);
    }
    Ok(Pending { out, part, dst, size: done })
}

/// Total time covered by `spans` (overlaps counted once).
fn busy_secs(mut spans: Vec<(Instant, Instant)>) -> f64 {
    spans.sort_by_key(|s| s.0);
    let mut total = 0.0;
    let mut cur: Option<(Instant, Instant)> = None;
    for (a, b) in spans {
        cur = match cur {
            Some((ca, cb)) if a <= cb => Some((ca, cb.max(b))),
            Some((ca, cb)) => {
                total += (cb - ca).as_secs_f64();
                Some((a, b))
            }
            None => Some((a, b)),
        };
    }
    total + cur.map_or(0.0, |(a, b)| (b - a).as_secs_f64())
}

/// Deletes `.opentone-part` leftovers of an interrupted sync.
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
    for (entry, o) in plan.lpc {
        m.tracks[entry].audio.lpc_order = o;
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

/// One file for [`pipeline`]: converted first unless `policy` is Copy.
pub(crate) struct Job<'a> {
    pub src: &'a Path,
    pub policy: &'a Policy,
    pub info: &'a AudioInfo,
    /// Card path.
    pub dest: &'a str,
}

/// Why a [`pipeline`] file didn't land.
pub(crate) enum Failure {
    Convert(String),
    Write(io::Error),
}

impl Failure {
    /// Warns about it; returns the message for the outcome.
    pub(crate) fn report(self, progress: &dyn Progress, src: &Path, dest: &str) -> String {
        let (what, e) = match self {
            Failure::Convert(e) => (src.display().to_string(), e),
            Failure::Write(e) => (format!("writing {dest}"), e.to_string()),
        };
        progress.warn(&format!("{what}: {e}"));
        e
    }
}

/// Converts and writes `items` to the card, keeping the card busy: a feeder
/// queues the copies at once while `workers` threads convert the others,
/// and the calling thread writes the next ready file while a finisher syncs
/// and renames the previous one. Converted files wait in `tmp`, at most
/// `2 * workers` ahead. `landed(i, result)` runs on the calling thread once
/// `items[i]` has its name on the card (or failed). Returns the seconds the
/// card was busy, for its write speed.
pub(crate) fn pipeline(
    card: &Path,
    items: &[Job],
    workers: usize,
    tmp: &Path,
    stop: &AtomicBool,
    progress: &dyn Progress,
    landed: &mut dyn FnMut(usize, Result<u64, Failure>) -> io::Result<()>,
) -> io::Result<f64> {
    let (copies, conversions): (Vec<usize>, Vec<usize>) = (0..items.len()).partition(|&i| *items[i].policy == Policy::Copy);
    let cap = workers.max(1) * 2;
    let next = AtomicUsize::new(0);
    let (tx, rx) = mpsc::sync_channel::<Ready>(cap);
    let mut spans: Vec<(Instant, Instant)> = Vec::new();
    std::thread::scope(|scope| -> io::Result<()> {
        let feeder = tx.clone();
        let copies = &copies;
        scope.spawn(move || {
            for &i in copies {
                if stop.load(Ordering::Relaxed) || feeder.send(Ready::Source(i)).is_err() {
                    break;
                }
            }
        });
        for worker in 0..workers.min(conversions.len()) {
            let (tx, next, conversions) = (tx.clone(), &next, &conversions);
            scope.spawn(move || {
                while !stop.load(Ordering::Relaxed) {
                    let Some(&i) = conversions.get(next.fetch_add(1, Ordering::Relaxed)) else { break };
                    let it = &items[i];
                    let dst = tmp.join(format!("{i}.{}", it.policy.output_ext("flac")));
                    progress.converting(worker, it.src, Some(&convert::describe(it.policy, it.info)));
                    let r = convert::run(it.policy, it.src, &dst);
                    progress.converting(worker, it.src, None);
                    let msg = match r {
                        Ok(()) => Ready::Converted(i, dst),
                        Err(e) => {
                            let _ = fs::remove_file(&dst);
                            Ready::Failed(i, e.to_string())
                        }
                    };
                    if tx.send(msg).is_err() {
                        break;
                    }
                }
            });
        }
        drop(tx);

        // The finisher syncs and renames a written file while the next one
        // is written (rendezvous: one at a time), so the card never idles
        // between files. A file is reported only once it has its name.
        let (finish_tx, finish_rx) = mpsc::sync_channel::<(usize, Option<PathBuf>, Pending)>(0);
        let (done_tx, done_rx) = mpsc::channel::<(usize, io::Result<u64>, (Instant, Instant))>();
        scope.spawn(move || {
            for (i, temp, p) in finish_rx {
                let t0 = Instant::now();
                let r = p.finish();
                let span = (t0, Instant::now());
                if let Some(t) = temp {
                    let _ = fs::remove_file(t);
                }
                if done_tx.send((i, r, span)).is_err() {
                    break;
                }
            }
        });
        // A full card stops the run.
        let mut deliver = |i: usize, r: Result<u64, Failure>| {
            if matches!(&r, Err(Failure::Write(e)) if e.kind() == io::ErrorKind::StorageFull) {
                stop.store(true, Ordering::Relaxed);
            }
            landed(i, r)
        };
        // Ready files, held here so the UI can show what comes next.
        let mut queue: VecDeque<Ready> = VecDeque::new();
        let mut open = true;
        loop {
            while open && queue.len() < cap {
                match rx.try_recv() {
                    Ok(msg) => queue.push_back(msg),
                    Err(mpsc::TryRecvError::Empty) => break,
                    Err(mpsc::TryRecvError::Disconnected) => open = false,
                }
            }
            let msg = match queue.pop_front() {
                Some(msg) => msg,
                None if open => match rx.recv() {
                    Ok(msg) => msg,
                    Err(_) => break,
                },
                None => break,
            };
            let ready = |r: &Ready| match r {
                Ready::Source(i) | Ready::Converted(i, _) => Some(items[*i].dest),
                Ready::Failed(..) => None,
            };
            progress.queued(&queue.iter().filter_map(ready).collect::<Vec<_>>());
            let (i, file, temp) = match msg {
                Ready::Source(i) => (i, items[i].src.to_path_buf(), false),
                Ready::Converted(i, p) => (i, p, true),
                Ready::Failed(i, e) => {
                    deliver(i, Err(Failure::Convert(e)))?;
                    continue;
                }
            };
            if stop.load(Ordering::Relaxed) {
                if temp {
                    let _ = fs::remove_file(&file);
                }
                continue;
            }
            let dest = items[i].dest;
            let t0 = Instant::now();
            match start_write(&file, card, dest, &|done, total| progress.copying(dest, done, total)) {
                // Waits for the finisher to take it: the previous file is then on the card.
                Ok(p) => {
                    let _ = finish_tx.send((i, temp.then_some(file), p));
                }
                Err(e) => {
                    if temp {
                        let _ = fs::remove_file(&file);
                    }
                    deliver(i, Err(Failure::Write(e)))?;
                }
            }
            spans.push((t0, Instant::now()));
            while let Ok((i, r, span)) = done_rx.try_recv() {
                spans.push(span);
                deliver(i, r.map_err(Failure::Write))?;
            }
        }
        drop(finish_tx);
        for (i, r, span) in done_rx {
            spans.push(span);
            deliver(i, r.map_err(Failure::Write))?;
        }
        Ok(())
    })?;
    Ok(busy_secs(spans))
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
    let tmp = std::env::temp_dir().join(format!("opentone-{}", std::process::id()));
    fs::create_dir_all(&tmp)?;
    let dests = card_paths(m, &writes);

    progress.start(writes.len(), writes.iter().map(|w| w.estimated_size).sum());
    let items: Vec<Job> = writes
        .iter()
        .zip(&dests)
        .map(|(w, dest)| Job { src: &w.source.abs, policy: &w.policy, info: &w.probed.info, dest })
        .collect();
    let mut since_save = 0;
    let mut last_save = Instant::now();
    let result = pipeline(card, &items, jobs, &tmp, stop, progress, &mut |i, r| {
        let (w, dest) = (&writes[i], &dests[i]);
        let size = match r {
            Ok(size) => size,
            Err(f) => {
                let e = f.report(progress, &w.source.abs, dest);
                out.failed.push((w.source.abs.clone(), e));
                progress.advance(w.estimated_size, "");
                return Ok(());
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
        Ok(())
    });
    let _ = fs::remove_dir_all(&tmp);
    manifest::save(card, m)?;
    out.write_secs = result?;
    out.interrupted = stop.load(Ordering::Relaxed);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::library::adopted_entry;
    use crate::tags::{TagQuality, Tags};

    fn flac(lpc_order: u8) -> AudioInfo {
        AudioInfo { codec: Codec::Flac, sample_rate: 44_100, bits: 16, channels: 2, bitrate: 900, duration: 200.0, lpc_order }
    }

    fn adopted(rel: &str, size: u64) -> Entry {
        let p = Probed { tags: Tags::default(), quality: TagQuality::Unknown, info: flac(audio::LPC_UNKNOWN) };
        adopted_entry(rel.into(), size, p, 0)
    }

    fn source(path: &str, size: u64) -> SourceRef {
        SourceRef { machine: "pc".into(), path: path.into(), size, mtime: 0, hash: String::new() }
    }

    #[test]
    fn adopted_flac_lpc_read_where_cheapest() {
        let m = Manifest { tracks: vec![adopted("a.flac", 100), adopted("b.flac", 100)], ..Default::default() };
        let opts = PlanOptions { machine: "pc".into(), have_ffmpeg: false, card: "card".into(), include_removed: false };
        // a has its library file attached (same size); b has a different-size one.
        let mut plan = Plan { attach: vec![(0, source("/lib/a.flac", 100)), (1, source("/lib/b.flac", 99))], ..Default::default() };
        let card_file = |_: &str| Some((100, 0));
        let checks = lpc_needed(&m, &plan, &opts);
        let read: Vec<_> = checks.iter().map(|c| (c.path.to_str().unwrap(), c.on_card)).collect();
        assert_eq!(read, [("/lib/a.flac", false), ("b.flac", true)]);
        apply_lpc(&m, &mut plan, &checks, &[Some(8), Some(32)], &opts, &card_file);
        assert_eq!(plan.lpc, [(0, 8), (1, 32)]);
        // Only b, past the FLAC subset, is converted in place.
        assert_eq!(plan.writes.len(), 1);
        assert_eq!((plan.writes[0].replaces, plan.writes[0].reason), (Some(1), WriteReason::Fix));
    }
}
