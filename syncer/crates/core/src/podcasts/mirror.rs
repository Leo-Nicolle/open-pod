//! Mirrors the podcasts folder to `<card>/Podcasts/`: new episodes are
//! copied (converted like music when the player needs it), episodes gone
//! from the source are deleted, and episodes that only moved (same content
//! hash, another path) are renamed on the card instead of copied again.
//!
//! Planning runs in steps so the browser build can do the file reading
//! itself: [`triage`] -> hash [`Triage::to_hash`] -> [`match_hashes`] ->
//! [`probe`] [`Matched::to_probe`] -> [`finish`]. [`plan`] chains them.

use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{self, Read, Seek};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Instant;

use rayon::prelude::*;

use super::{CARD_DIR, Episode, PodcastManifest, date_in_name, days_of_mtime, parse_date};
use crate::convert;
use crate::manifest::SourceRef;
use crate::paths::{Taken, sanitize_component};
use crate::policy::{self, Policy};
use crate::sync::{self, Progress, SourceFile, now_secs};
use crate::tags::{self, Probed};

pub struct Options {
    pub machine: String,
    pub have_ffmpeg: bool,
}

fn source_ref(machine: &str, f: &SourceFile, hash: &str) -> SourceRef {
    SourceRef { machine: machine.to_owned(), path: f.abs.to_string_lossy().into_owned(), size: f.size, mtime: f.mtime, hash: hash.to_owned() }
}

/// Folder names between the podcasts root and the file.
fn folders_of(rel: &Path) -> Vec<String> {
    rel.parent()
        .map(|p| p.components().map(|c| c.as_os_str().to_string_lossy().into_owned()).collect())
        .unwrap_or_default()
}

/// Card path for a source file: `Podcasts/<same folders>/<same name>.<ext>`,
/// FAT-safe. Long names are cut before the extension, not through it.
pub fn card_path(rel: &Path, ext: &str) -> String {
    let mut parts = vec![CARD_DIR.to_owned()];
    parts.extend(folders_of(rel).iter().map(|f| sanitize_component(f)));
    let stem = rel.file_stem().map(|s| s.to_string_lossy().into_owned()).unwrap_or_default();
    parts.push(format!("{}.{ext}", sanitize_component(&stem)));
    parts.join("/")
}

fn ext_of(path: &str) -> &str {
    path.rsplit_once('.').map_or("", |(_, e)| e)
}

/// First step: files unchanged since the last sync keep their known hash;
/// the others need it computed ([`Triage::to_hash`]).
pub struct Triage {
    known: Vec<(SourceFile, String)>,
    to_hash: Vec<SourceFile>,
}

impl Triage {
    pub fn to_hash(&self) -> Vec<&SourceFile> {
        self.to_hash.iter().collect()
    }
}

pub fn triage(m: &PodcastManifest, files: Vec<SourceFile>, machine: &str) -> Triage {
    let by_source: HashMap<&str, &SourceRef> =
        m.episodes.iter().flat_map(|e| &e.sources).filter(|s| s.machine == machine).map(|s| (s.path.as_str(), s)).collect();
    let mut t = Triage { known: Vec::new(), to_hash: Vec::new() };
    for f in files {
        match by_source.get(f.abs.to_string_lossy().as_ref()) {
            Some(s) if s.size == f.size && s.mtime == f.mtime => {
                let hash = s.hash.clone();
                t.known.push((f, hash));
            }
            _ => t.to_hash.push(f),
        }
    }
    t
}

/// Second step: which source files are already on the card (by hash), and
/// which are new and need their tags read ([`Matched::to_probe`]).
pub struct Matched {
    on_card: Vec<(SourceFile, String)>,
    to_probe: Vec<(SourceFile, String)>,
    unreadable: Vec<(PathBuf, String)>,
}

impl Matched {
    pub fn to_probe(&self) -> Vec<&SourceFile> {
        self.to_probe.iter().map(|(f, _)| f).collect()
    }
}

/// `hashes` follows [`Triage::to_hash`]. A source seen twice (same hash) is
/// mirrored once.
pub fn match_hashes(m: &PodcastManifest, t: Triage, hashes: Vec<io::Result<String>>) -> Matched {
    let on_card: HashSet<&str> = m.episodes.iter().map(|e| e.hash.as_str()).collect();
    let mut seen = HashSet::new();
    let mut out = Matched { on_card: Vec::new(), to_probe: Vec::new(), unreadable: Vec::new() };
    let hashed = t.to_hash.into_iter().zip(hashes).filter_map(|(f, h)| match h {
        Ok(h) => Some((f, h)),
        Err(e) => {
            out.unreadable.push((f.abs, e.to_string()));
            None
        }
    });
    let all: Vec<(SourceFile, String)> = t.known.into_iter().chain(hashed).collect();
    for (f, h) in all {
        if !seen.insert(h.clone()) {
            continue;
        }
        if on_card.contains(h.as_str()) {
            out.on_card.push((f, h));
        } else {
            out.to_probe.push((f, h));
        }
    }
    out
}

/// What [`finish`] needs from a new source file.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct Probe {
    pub probed: Probed,
    pub title: Option<String>,
    pub date: Option<u32>,
}

pub fn probe(path: &Path, rel: &Path) -> Probe {
    match fs::File::open(path) {
        Ok(f) => probe_reader(&mut io::BufReader::new(f), path, rel),
        Err(_) => Probe { probed: tags::probe(path, rel, true), title: None, date: None },
    }
}

/// [`probe`] on any reader; `name` gives the extension.
pub fn probe_reader<R: Read + Seek>(r: &mut R, name: &Path, rel: &Path) -> Probe {
    let probed = tags::probe_reader(r, name, rel, true);
    let (title, date) = tags::episode_tags_reader(r, name);
    Probe { probed, title, date: date.as_deref().and_then(parse_date) }
}

/// One source file to write to the card.
#[derive(Debug)]
pub struct Copy {
    pub source: SourceFile,
    pub source_ref: SourceRef,
    pub probed: Probed,
    pub title: String,
    pub date: u32,
    pub folders: Vec<String>,
    pub policy: Policy,
    pub estimated_size: u64,
    pub dest: String,
}

/// A source file already on the card: its source is recorded and, when it
/// moved in the source, the card file follows (`to`).
#[derive(Debug)]
pub struct Link {
    pub hash: String,
    pub source_ref: SourceRef,
    pub folders: Vec<String>,
    /// (from, to) card paths.
    pub moved: Option<(String, String)>,
    /// A better publication date than the recorded one (see `redate`).
    pub redate: Option<u32>,
}

/// A source episode the show's keep setting leaves on the computer.
#[derive(Debug, Clone)]
pub struct Held {
    pub source: PathBuf,
    pub folders: Vec<String>,
    pub title: String,
    pub date: u32,
    pub duration: f64,
    pub estimated_size: u64,
}

#[derive(Debug, Default)]
pub struct MirrorPlan {
    pub copies: Vec<Copy>,
    pub links: Vec<Link>,
    /// Card paths of episodes gone from the source, or past their show's
    /// keep setting.
    pub deletes: Vec<String>,
    /// The deletes that are past their show's keep setting (still in the
    /// source).
    pub leaving: Vec<String>,
    /// New source episodes past their show's keep setting: not copied.
    pub held: Vec<Held>,
    pub unsupported: Vec<(PathBuf, String)>,
    pub unreadable: Vec<(PathBuf, String)>,
}

impl MirrorPlan {
    pub fn moves(&self) -> usize {
        self.links.iter().filter(|l| l.moved.is_some()).count()
    }
    pub fn bytes_needed(&self) -> u64 {
        self.copies.iter().map(|c| c.estimated_size).sum()
    }
    /// Nothing to do on the card (links that don't move only refresh the
    /// manifest).
    pub fn is_empty(&self) -> bool {
        self.copies.is_empty() && self.deletes.is_empty() && self.moves() == 0 && self.redates() == 0
    }
    pub fn redates(&self) -> usize {
        self.links.iter().filter(|l| l.redate.is_some()).count()
    }
}

/// An episode dated by its file time (older syncs didn't read ISO dates in
/// names) whose name has a date: that one. Tag dates are left alone.
fn redate(e: &Episode, f: &SourceFile) -> Option<u32> {
    let name = f.rel.file_name()?.to_string_lossy();
    let d = date_in_name(&name)?;
    (e.date == days_of_mtime(f.mtime) && d != e.date).then_some(d)
}

/// A new source file the player can take.
struct NewFile {
    f: SourceFile,
    hash: String,
    probe: Probe,
    policy: Policy,
    date: u32,
}

/// Which episodes are past their show's keep setting: indexes into
/// `on_card` and `new`. A show keeps its latest `keep` episodes, on the card
/// or new alike, newest first by date, then by file name.
fn past_keep(m: &PodcastManifest, on_card: &[(SourceFile, u32)], new: &[NewFile]) -> (HashSet<usize>, HashSet<usize>) {
    let name = |f: &SourceFile| f.rel.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    // (date, name, on the card, index) per show.
    let mut shows: HashMap<String, Vec<(u32, String, bool, usize)>> = HashMap::new();
    let cands = on_card.iter().enumerate().map(|(i, (f, d))| (f, *d, true, i)).chain(new.iter().enumerate().map(|(i, n)| (&n.f, n.date, false, i)));
    for (f, date, card, i) in cands {
        let key = super::show_key(&folders_of(&f.rel));
        if m.keep.contains_key(&key) {
            shows.entry(key).or_default().push((date, name(f), card, i));
        }
    }
    let (mut leave, mut hold) = (HashSet::new(), HashSet::new());
    for (key, mut eps) in shows {
        eps.sort_by(|a, b| (b.0, &b.1).cmp(&(a.0, &a.1)));
        for &(_, _, card, i) in eps.iter().skip(m.keep[&key] as usize) {
            if card { leave.insert(i) } else { hold.insert(i) };
        }
    }
    (leave, hold)
}

/// Last step. `probes` follows [`Matched::to_probe`].
pub fn finish(m: &PodcastManifest, mt: Matched, probes: Vec<Probe>, opts: &Options) -> MirrorPlan {
    let mut plan = MirrorPlan { unreadable: mt.unreadable, ..Default::default() };
    let by_hash: HashMap<&str, &Episode> = m.episodes.iter().map(|e| (e.hash.as_str(), e)).collect();

    let mut new = Vec::new();
    for ((f, hash), p) in mt.to_probe.into_iter().zip(probes) {
        let policy = policy::decide(&p.probed.info, opts.have_ffmpeg);
        if let Policy::Unsupported(reason) = &policy {
            plan.unsupported.push((f.abs, reason.clone()));
            continue;
        }
        let name = f.rel.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let date = p.date.or_else(|| date_in_name(&name)).unwrap_or_else(|| days_of_mtime(f.mtime));
        new.push(NewFile { f, hash, probe: p, policy, date });
    }
    // On the card, with their (corrected) dates.
    let on_card: Vec<(SourceFile, u32)> = mt
        .on_card
        .iter()
        .map(|(f, h)| {
            let e = by_hash[h.as_str()];
            (f.clone(), redate(e, f).unwrap_or(e.date))
        })
        .collect();
    let (leave, hold) = past_keep(m, &on_card, &new);
    let leaving: HashSet<String> = leave.iter().map(|&i| mt.on_card[i].1.clone()).collect();
    let on_card: Vec<(SourceFile, String)> =
        mt.on_card.into_iter().enumerate().filter_map(|(i, x)| (!leave.contains(&i)).then_some(x)).collect();

    // Unreadable files keep their episode: a read error isn't a deletion.
    let unreadable: HashSet<String> = plan.unreadable.iter().map(|(p, _)| p.to_string_lossy().into_owned()).collect();
    let mut keep: HashSet<&str> = on_card.iter().map(|(_, h)| h.as_str()).collect();
    for e in &m.episodes {
        if e.sources.iter().any(|s| s.machine == opts.machine && unreadable.contains(&s.path)) {
            keep.insert(&e.hash);
        }
    }
    let mut taken = Taken::default();
    for e in &m.episodes {
        if keep.contains(e.hash.as_str()) {
            taken.insert(&e.path);
        } else {
            plan.deletes.push(e.path.clone());
            if leaving.contains(&e.hash) {
                plan.leaving.push(e.path.clone());
            }
        }
    }

    for (f, hash) in on_card {
        let e = by_hash[hash.as_str()];
        let wanted = card_path(&f.rel, ext_of(&e.path));
        let moved = if wanted.eq_ignore_ascii_case(&e.path) {
            None
        } else {
            taken.remove(&e.path);
            let dest = taken.claim(&wanted);
            (!dest.eq_ignore_ascii_case(&e.path)).then(|| (e.path.clone(), dest))
        };
        if moved.is_none() {
            taken.insert(&e.path);
        }
        let redate = redate(e, &f);
        plan.links.push(Link { source_ref: source_ref(&opts.machine, &f, &hash), hash, folders: folders_of(&f.rel), moved, redate });
    }

    for (i, n) in new.into_iter().enumerate() {
        let NewFile { f, hash, probe: p, policy, date } = n;
        let stem = f.rel.file_stem().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let title = p.title.clone().unwrap_or(stem);
        let estimated_size = policy::estimated_size(&policy, &p.probed.info, f.size);
        if hold.contains(&i) {
            plan.held.push(Held { folders: folders_of(&f.rel), title, date, duration: p.probed.info.duration, estimated_size, source: f.abs });
            continue;
        }
        let ext = f.abs.extension().map(|e| e.to_string_lossy().to_lowercase()).unwrap_or_default();
        let dest = taken.claim(&card_path(&f.rel, policy.output_ext(&ext)));
        plan.copies.push(Copy {
            source_ref: source_ref(&opts.machine, &f, &hash),
            title,
            date,
            folders: folders_of(&f.rel),
            estimated_size,
            policy,
            probed: p.probed,
            dest,
            source: f,
        });
    }
    plan
}

/// Plans the mirror of `root` (native: reads the files itself).
pub fn plan(m: &PodcastManifest, root: &Path, opts: &Options) -> io::Result<MirrorPlan> {
    let files = sync::scan(&[root.to_path_buf()], &[])?;
    let t = triage(m, files, &opts.machine);
    let hashes: Vec<io::Result<String>> = t.to_hash().par_iter().map(|f| sync::content_hash(&f.abs, f.size)).collect();
    let mt = match_hashes(m, t, hashes);
    let probes: Vec<Probe> = mt.to_probe().par_iter().map(|f| probe(&f.abs, &f.rel)).collect();
    Ok(finish(m, mt, probes, opts))
}

// --- applying the plan ------------------------------------------------------------------

/// Drops deleted episodes and applies the links to the manifest. Returns
/// the card renames (from, to) the caller performs, then the deleted card
/// paths: deletes first, so a move can take a deleted episode's name.
pub fn apply_links(m: &mut PodcastManifest, plan: &MirrorPlan, machine: &str) -> Vec<(String, String)> {
    let deleted: HashSet<&str> = plan.deletes.iter().map(String::as_str).collect();
    m.episodes.retain(|e| !deleted.contains(e.path.as_str()));
    let mut renames = Vec::new();
    for l in &plan.links {
        let Some(e) = m.episodes.iter_mut().find(|e| e.hash == l.hash) else { continue };
        e.sources.retain(|s| s.machine != machine);
        e.sources.push(l.source_ref.clone());
        e.folders = l.folders.clone();
        if let Some(d) = l.redate {
            e.date = d;
        }
        if let Some((from, to)) = &l.moved {
            e.path = to.clone();
            renames.push((from.clone(), to.clone()));
        }
    }
    renames
}

/// Records a copy that landed on the card (`size` bytes).
pub fn record_copy(m: &mut PodcastManifest, c: &Copy, size: u64) {
    m.episodes.push(Episode {
        path: c.dest.clone(),
        size,
        hash: c.source_ref.hash.clone(),
        folders: c.folders.clone(),
        title: c.title.clone(),
        date: c.date,
        duration: c.probed.info.duration,
        conversion: convert::describe(&c.policy, &c.probed.info),
        sources: vec![c.source_ref.clone()],
        added_at: now_secs(),
    });
}

#[derive(Debug, Default)]
pub struct Outcome {
    /// Card paths written.
    pub copied: Vec<String>,
    pub moved: usize,
    pub deleted: usize,
    /// Episodes whose date was corrected.
    pub redated: usize,
    pub failed: Vec<(PathBuf, String)>,
    pub bytes: u64,
    pub write_secs: f64,
    pub interrupted: bool,
}

impl Outcome {
    /// The card's podcasts changed: the tables need rebuilding.
    pub fn changed(&self) -> bool {
        !self.copied.is_empty() || self.moved > 0 || self.deleted > 0 || self.redated > 0
    }
}

/// Runs the plan on a mounted card: deletes, renames, then copies
/// (through [`sync::pipeline`], `jobs` conversions at a time). The manifest is
/// saved as it goes, so an interrupted run (`stop`) loses nothing.
pub fn execute(
    card: &Path,
    m: &mut PodcastManifest,
    plan: &MirrorPlan,
    machine: &str,
    jobs: usize,
    stop: &AtomicBool,
    progress: &dyn Progress,
) -> io::Result<Outcome> {
    let mut out = Outcome::default();
    for p in &plan.deletes {
        match fs::remove_file(card.join(p)) {
            Ok(()) => out.deleted += 1,
            Err(e) if e.kind() == io::ErrorKind::NotFound => out.deleted += 1,
            Err(e) => progress.warn(&format!("deleting {p}: {e}")),
        }
        crate::library::remove_empty_dirs(card, p);
    }
    out.redated = plan.redates();
    for (from, to) in apply_links(m, plan, machine) {
        let dst = card.join(&to);
        if let Some(dir) = dst.parent() {
            fs::create_dir_all(dir)?;
        }
        match fs::rename(card.join(&from), &dst) {
            Ok(()) => out.moved += 1,
            Err(e) => progress.warn(&format!("moving {from} to {to}: {e}")),
        }
        crate::library::remove_empty_dirs(card, &from);
    }
    super::save(card, m)?;

    let mut cover_dirs: HashSet<(PathBuf, PathBuf)> = HashSet::new();
    for l in &plan.links {
        let path = l.moved.as_ref().map(|(_, to)| to.clone()).or_else(|| m.episodes.iter().find(|e| e.hash == l.hash).map(|e| e.path.clone()));
        if let (Some(src), Some(path)) = (Path::new(&l.source_ref.path).parent(), path) {
            if let Some(dst) = card.join(path).parent() {
                cover_dirs.insert((src.to_path_buf(), dst.to_path_buf()));
            }
        }
    }

    if !plan.copies.is_empty() {
        let tmp = std::env::temp_dir().join(format!("opentone-podcasts-{}", std::process::id()));
        fs::create_dir_all(&tmp)?;
        progress.start(plan.copies.len(), plan.bytes_needed());
        let items: Vec<sync::Job> = plan
            .copies
            .iter()
            .map(|c| sync::Job { src: &c.source.abs, policy: &c.policy, info: &c.probed.info, dest: &c.dest })
            .collect();
        let mut since_save = 0;
        let mut last_save = Instant::now();
        let result = sync::pipeline(card, &items, jobs, &tmp, stop, progress, &mut |i, r| {
            let c = &plan.copies[i];
            match r {
                Ok(size) => {
                    record_copy(m, c, size);
                    out.copied.push(c.dest.clone());
                    out.bytes += size;
                    if let (Some(src), Some(dst)) = (c.source.abs.parent(), card.join(&c.dest).parent()) {
                        cover_dirs.insert((src.to_path_buf(), dst.to_path_buf()));
                    }
                }
                Err(f) => {
                    let e = f.report(progress, &c.source.abs, &c.dest);
                    out.failed.push((c.source.abs.clone(), e));
                }
            }
            progress.advance(c.estimated_size, &c.dest);
            since_save += 1;
            if since_save >= sync::CHECKPOINT_EVERY || last_save.elapsed().as_secs() >= sync::CHECKPOINT_SECS {
                super::save(card, m)?;
                since_save = 0;
                last_save = Instant::now();
            }
            Ok(())
        });
        let _ = fs::remove_dir_all(&tmp);
        out.write_secs = result?;
    }
    for (src, dst) in cover_dirs {
        if dst.is_dir() {
            sync::copy_cover_images(&src, &dst);
        }
    }
    super::save(card, m)?;
    out.interrupted = stop.load(Ordering::Relaxed);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::audio::{AudioInfo, Codec};

    fn src(rel: &str) -> SourceFile {
        SourceFile { abs: PathBuf::from("/pod").join(rel), rel: PathBuf::from(rel), size: 10, mtime: 1 }
    }

    fn mp3() -> Probe {
        let info = AudioInfo { codec: Codec::Mp3, sample_rate: 44_100, bits: 0, channels: 2, bitrate: 128, duration: 60.0, lpc_order: 0 };
        Probe { probed: Probed { tags: Default::default(), quality: tags::TagQuality::Unknown, info }, title: None, date: None }
    }

    fn episode(path: &str, hash: &str) -> Episode {
        Episode {
            path: path.into(),
            size: 10,
            hash: hash.into(),
            folders: vec![],
            title: "t".into(),
            date: 0,
            duration: 60.0,
            conversion: "copied".into(),
            sources: vec![],
            added_at: 0,
        }
    }

    #[test]
    fn paths() {
        assert_eq!(card_path(Path::new("France Culture/Show: 1/ep?.m4a"), "mp3"), "Podcasts/France Culture/Show_ 1/ep_.mp3");
        assert_eq!(card_path(Path::new("loose.mp3"), "mp3"), "Podcasts/loose.mp3");
        let long = format!("{}.m4a", "x".repeat(200));
        assert!(card_path(Path::new(&long), "mp3").ends_with("xx.mp3"));
    }

    #[test]
    fn copy_move_delete_keep() {
        let m = PodcastManifest {
            episodes: vec![
                episode("Podcasts/A/kept.mp3", "01"),
                episode("Podcasts/A/moved.mp3", "02"),
                episode("Podcasts/A/gone.mp3", "03"),
            ],
            ..Default::default()
        };
        let opts = Options { machine: "pc".into(), have_ffmpeg: false };
        let files = vec![src("A/kept.mp3"), src("B/moved.mp3"), src("A/new 12.10.2026.mp3")];
        let t = triage(&m, files, "pc");
        assert_eq!(t.to_hash().len(), 3);
        let mt = match_hashes(&m, t, vec![Ok("01".into()), Ok("02".into()), Ok("04".into())]);
        assert_eq!(mt.to_probe().len(), 1);
        let plan = finish(&m, mt, vec![mp3()], &opts);
        assert_eq!(plan.deletes, ["Podcasts/A/gone.mp3"]);
        assert_eq!(plan.moves(), 1);
        assert_eq!(plan.links[1].moved, Some(("Podcasts/A/moved.mp3".into(), "Podcasts/B/moved.mp3".into())));
        assert_eq!(plan.copies.len(), 1);
        assert_eq!(plan.copies[0].dest, "Podcasts/A/new 12.10.2026.mp3");
        assert_eq!(plan.copies[0].title, "new 12.10.2026");
        assert_eq!(plan.copies[0].folders, ["A"]);

        let mut m = m;
        let renames = apply_links(&mut m, &plan, "pc");
        assert_eq!(renames.len(), 1);
        assert_eq!(m.episodes.len(), 2);
        assert_eq!(m.episodes[1].path, "Podcasts/B/moved.mp3");
        assert_eq!(m.episodes[1].folders, ["B"]);
        record_copy(&mut m, &plan.copies[0], 10);
        assert_eq!(m.episodes.len(), 3);
    }

    #[test]
    fn keep_latest() {
        let mut old = episode("Podcasts/A/old.mp3", "01");
        old.date = 10;
        let mut mid = episode("Podcasts/A/mid.mp3", "02");
        mid.date = 20;
        let mut m = PodcastManifest { episodes: vec![old, mid], ..Default::default() };
        m.keep.insert("A".into(), 2);
        let opts = Options { machine: "pc".into(), have_ffmpeg: false };
        // A newer episode and an older one appear: the newest two stay.
        let files = vec![src("A/old.mp3"), src("A/mid.mp3"), src("A/new 30.01.1970.mp3"), src("A/older 01.01.1971.mp3")];
        let t = triage(&m, files, "pc");
        let mt = match_hashes(&m, t, ["01", "02", "03", "04"].map(|h| Ok(h.to_string())).into());
        let mut dated = mp3();
        dated.date = Some(30);
        let mut older = mp3();
        older.date = Some(5);
        let plan = finish(&m, mt, vec![dated, older], &opts);
        assert_eq!(plan.copies.iter().map(|c| c.dest.as_str()).collect::<Vec<_>>(), ["Podcasts/A/new 30.01.1970.mp3"]);
        assert_eq!(plan.deletes, ["Podcasts/A/old.mp3"]);
        assert_eq!(plan.leaving, plan.deletes);
        assert_eq!(plan.held.len(), 1);
        assert_eq!(plan.held[0].date, 5);
        assert_eq!(plan.links.len(), 1);
    }

    #[test]
    fn unreadable_source_is_not_a_deletion() {
        let mut e = episode("Podcasts/x.mp3", "01");
        e.sources.push(SourceRef { machine: "pc".into(), path: "/pod/x.mp3".into(), size: 1, mtime: 0, hash: "01".into() });
        let m = PodcastManifest { episodes: vec![e], ..Default::default() };
        let t = triage(&m, vec![src("x.mp3")], "pc");
        let mt = match_hashes(&m, t, vec![Err(io::Error::other("io"))]);
        let plan = finish(&m, mt, vec![], &Options { machine: "pc".into(), have_ffmpeg: false });
        assert!(plan.deletes.is_empty());
    }
}
