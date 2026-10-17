//! opentone Sync in the browser. JavaScript walks folders and writes to the
//! card (File System Access API); this crate does the rest:
//!
//! - per-file work in Web Workers ([`probe`], [`hash`], [`convert`],
//!   [`art`]), reading browser `File`s synchronously through
//!   `FileReaderSync`, which only workers have;
//! - planning and manifest bookkeeping in a [`Session`] on the page.
//!
//! Structured values cross the boundary as JSON strings, file contents as
//! `Uint8Array`s. Paths are virtual: `lib<N>/<library-relative path>` for
//! library files and `card/<card path>` for files already on the card.

use std::collections::HashMap;
use std::io::{self, BufReader, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};

use opentone_core::index::{self, AlbumKey, crawl, model};
use opentone_core::library::{self, PlanInfo};
use opentone_core::manifest::{self, Manifest};
use opentone_core::podcasts::{self, PodcastManifest, mirror};
use opentone_core::policy::Policy;
use opentone_core::sync::{self, Matched, Outcome, Plan, PlanOptions, SourceFile, Triage, Write};
use opentone_core::tags::{self, Probed};
use opentone_core::{convert, view};
use serde::{Deserialize, Serialize};
use wasm_bindgen::prelude::*;
use web_sys::{Blob, File, FileReaderSync};

fn js_err(e: impl std::fmt::Display) -> JsError {
    JsError::new(&e.to_string())
}

fn to_json(v: &impl Serialize) -> Result<String, JsError> {
    serde_json::to_string(v).map_err(js_err)
}

fn from_json<'a, T: Deserialize<'a>>(s: &'a str) -> Result<T, JsError> {
    serde_json::from_str(s).map_err(js_err)
}

// ---- Reading browser files ------------------------------------------------

/// A browser `File` as `Read + Seek`: each read is a synchronous
/// `FileReaderSync` read of a `Blob` slice (workers only).
struct JsFile {
    blob: Blob,
    reader: FileReaderSync,
    pos: u64,
    len: u64,
}

// A worker runs one WebAssembly instance on one thread: these JS handles
// never cross threads. The core's converter wants `Send + Sync` sources.
unsafe impl Send for JsFile {}
unsafe impl Sync for JsFile {}

impl JsFile {
    fn new(file: &File) -> Result<Self, JsError> {
        let reader = FileReaderSync::new().map_err(|_| JsError::new("FileReaderSync is only available in workers"))?;
        Ok(Self { len: file.size() as u64, blob: file.clone().into(), reader, pos: 0 })
    }
}

impl Read for JsFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let end = (self.pos + buf.len() as u64).min(self.len);
        if end <= self.pos {
            return Ok(0);
        }
        let slice = self
            .blob
            .slice_with_f64_and_f64(self.pos as f64, end as f64)
            .map_err(|_| io::Error::other("Blob.slice failed"))?;
        let ab = self.reader.read_as_array_buffer(&slice).map_err(|_| io::Error::other("the file can't be read"))?;
        let bytes = js_sys::Uint8Array::new(&ab);
        let n = bytes.length() as usize;
        bytes.copy_to(&mut buf[..n]);
        self.pos += n as u64;
        Ok(n)
    }
}

impl Seek for JsFile {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        let next = match pos {
            SeekFrom::Start(p) => p as i128,
            SeekFrom::End(d) => self.len as i128 + d as i128,
            SeekFrom::Current(d) => self.pos as i128 + d as i128,
        };
        if next < 0 {
            return Err(io::Error::new(io::ErrorKind::InvalidInput, "seek before start"));
        }
        self.pos = next as u64;
        Ok(self.pos)
    }
}

/// Reads go through a 256 KiB buffer: lofty and symphonia read in small
/// pieces, and every `FileReaderSync` call costs a round trip.
fn open(file: &File) -> Result<BufReader<JsFile>, JsError> {
    Ok(BufReader::with_capacity(256 * 1024, JsFile::new(file)?))
}

// ---- Worker functions ------------------------------------------------------

/// Tags and audio properties of a file (JSON `Probed`). `name` gives the
/// extension, `rel` its path in the library (the tag fallback), `lpc` scans
/// a FLAC's LPC order (off for card files: see `tags::probe`).
#[wasm_bindgen]
pub fn probe(file: &File, name: &str, rel: &str, lpc: bool) -> Result<String, JsError> {
    let p = tags::probe_reader(&mut open(file)?, Path::new(name), Path::new(rel), lpc);
    to_json(&p)
}

/// The content hash sync uses to recognise identical files.
#[wasm_bindgen]
pub fn hash(file: &File) -> Result<String, JsError> {
    sync::content_hash_reader(&mut open(file)?, file.size() as u64).map_err(js_err)
}

/// Converts a file for the player (`policy`: JSON `Policy`). Only the
/// conversions done in Rust exist here; ffmpeg ones are skipped by
/// planning (no ffmpeg in the browser).
#[wasm_bindgen]
pub fn convert(file: &File, name: &str, policy: &str) -> Result<Vec<u8>, JsError> {
    match from_json::<Policy>(policy)? {
        Policy::Flac(t) => convert::to_flac_bytes(open(file)?, Path::new(name), t).map_err(js_err),
        p => Err(JsError::new(&format!("{p:?} can't run in the browser"))),
    }
}

/// The picture embedded in a track, if any.
#[wasm_bindgen]
pub fn art(file: &File, name: &str) -> Result<Option<Vec<u8>>, JsError> {
    Ok(crawl::embedded_art(&mut open(file)?, Path::new(name)))
}

/// What the podcast mirror needs from a new episode (JSON `Probe`: tags,
/// audio properties, title and date). `rel`: its path in the podcasts
/// folder.
#[wasm_bindgen]
pub fn probe_episode(file: &File, name: &str, rel: &str) -> Result<String, JsError> {
    to_json(&mirror::probe_reader(&mut open(file)?, Path::new(name), Path::new(rel)))
}

/// Which of a folder's file names is its cover image.
#[wasm_bindgen]
pub fn cover_file(names: Vec<String>) -> Option<String> {
    crawl::cover_file(&names).cloned()
}

/// A FLAC's LPC order (see `Session::finish`); undefined when unreadable.
#[wasm_bindgen]
pub fn lpc_order(file: &File) -> Result<Option<u8>, JsError> {
    Ok(tags::lpc_order_reader(&mut open(file)?))
}

/// Weight of a file in progress bars (the bytes probing reads).
#[wasm_bindgen]
pub fn probe_cost(name: &str, size: f64, lpc: bool) -> f64 {
    tags::probe_cost(Path::new(name), size as u64, lpc) as f64
}

/// The card's podcasts by show (JSON `CardPodcasts`). `manifest`: the
/// card's `opentone/podcasts.json`; `state`: its `podcast_state.bin`.
#[wasm_bindgen]
pub fn card_podcasts(manifest: Option<String>, state: Option<Vec<u8>>, machine: &str) -> Result<String, JsError> {
    let m: PodcastManifest = match manifest {
        Some(j) => from_json(&j)?,
        None => PodcastManifest::default(),
    };
    to_json(&view::card_podcasts(&m, state.as_deref(), machine))
}

/// `podcasts.json` with `show` keeping its latest `keep` episodes (none:
/// all of them).
#[wasm_bindgen]
pub fn podcast_set_keep(manifest: Option<String>, show: &str, keep: Option<u32>) -> Result<String, JsError> {
    let mut m: PodcastManifest = match manifest {
        Some(j) => from_json(&j)?,
        None => PodcastManifest::default(),
    };
    m.set_keep(show, keep);
    to_json(&m)
}

/// `podcast_state.bin` with the episode at card path `path` marked played
/// or not, as the player's "Mark as played/unplayed" does.
#[wasm_bindgen]
pub fn podcast_set_played(manifest: String, mut state: Vec<u8>, path: &str, played: bool) -> Result<Vec<u8>, JsError> {
    let m: PodcastManifest = from_json(&manifest)?;
    let e = m.episodes.iter().find(|e| e.path == path).ok_or_else(|| JsError::new(&format!("{path}: not a podcast on the card")))?;
    if !opentone_core::podcasts::state::set_played(&mut state, e.key(), played) {
        return Err(JsError::new("the player's listening state has no record of this episode"));
    }
    Ok(state)
}

// ---- Session -----------------------------------------------------------------

/// A library file, as JavaScript found it.
#[derive(Deserialize)]
struct FileIn {
    /// `lib<N>/<rel>`.
    path: String,
    rel: String,
    size: u64,
    /// `File.lastModified`, milliseconds.
    modified: f64,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct OptsIn {
    machine: String,
    include_removed: bool,
}

/// A card file read when adopting a card.
#[derive(Deserialize)]
struct AdoptIn {
    rel: String,
    size: u64,
    probed: Probed,
}

/// A file to write, for JavaScript.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct WriteOut<'a> {
    /// Virtual path of the source (`lib<N>/…`, or `card/…` for a fix).
    source: String,
    dest: &'a str,
    /// Copied as is (false: [`convert`] it with `policy` first).
    copy: bool,
    policy: &'a Policy,
    estimated_size: u64,
    /// "new", "upgrade", "changed" or "fix".
    reason: &'static str,
}

/// Holds the card's manifest and a sync in progress.
#[wasm_bindgen]
pub struct Session {
    m: Manifest,
    opts: Option<PlanOptions>,
    triage: Option<Triage>,
    matched: Option<Matched>,
    plan: Option<Plan>,
    lpc: Vec<sync::LpcCheck>,
    card_files: HashMap<String, (u64, f64)>,
    info: PlanInfo,
    writes: Vec<Write>,
    dests: Vec<String>,
    out: Outcome,
    arts: HashMap<String, Vec<u8>>,
    index_files: Vec<(String, Vec<u8>)>,
    index_summary: Option<index::Summary>,
}

#[wasm_bindgen]
impl Session {
    /// `manifest`: the card's `opentone/library.json`, or nothing for a card
    /// without one.
    #[wasm_bindgen(constructor)]
    pub fn new(manifest: Option<String>) -> Result<Session, JsError> {
        let m = match manifest {
            Some(j) => from_json(&j)?,
            None => Manifest::default(),
        };
        Ok(Session {
            m,
            opts: None,
            triage: None,
            matched: None,
            plan: None,
            lpc: Vec::new(),
            card_files: HashMap::new(),
            info: PlanInfo::default(),
            writes: Vec::new(),
            dests: Vec::new(),
            out: Outcome::default(),
            arts: HashMap::new(),
            index_files: Vec::new(),
            index_summary: None,
        })
    }

    /// `opentone/library.json`.
    pub fn manifest(&self) -> Result<String, JsError> {
        to_json(&self.m)
    }

    /// Builds the manifest of a card that has music but no manifest, from
    /// its probed files (JSON `[{rel, size, probed}]`).
    pub fn adopt(&mut self, files: &str) -> Result<(), JsError> {
        let files: Vec<AdoptIn> = from_json(files)?;
        let now = sync::now_secs();
        self.m = Manifest {
            tracks: files.into_iter().map(|f| library::adopted_entry(f.rel, f.size, f.probed, now)).collect(),
            ..Default::default()
        };
        Ok(())
    }

    /// Planning, step 1. `files`: JSON `[{path, rel, size, modified}]`;
    /// `opts`: JSON `{machine, includeRemoved}`. Returns the paths to
    /// [`hash`] (JSON array).
    pub fn triage(&mut self, files: &str, opts: &str) -> Result<String, JsError> {
        let files: Vec<FileIn> = from_json(files)?;
        let o: OptsIn = from_json(opts)?;
        let opts = PlanOptions {
            machine: o.machine,
            have_ffmpeg: false,
            card: PathBuf::from("card"),
            include_removed: o.include_removed,
        };
        let files = files
            .into_iter()
            .map(|f| SourceFile {
                abs: PathBuf::from(f.path),
                rel: PathBuf::from(f.rel),
                size: f.size,
                mtime: (f.modified * 1e6) as i128,
            })
            .collect();
        let t = sync::triage(&self.m, files, &opts);
        let out: Vec<String> = t.to_hash().iter().map(|f| f.abs.to_string_lossy().into_owned()).collect();
        self.triage = Some(t);
        self.opts = Some(opts);
        to_json(&out)
    }

    /// Planning, step 2. `hashes`: JSON array following [`Session::triage`]'s
    /// paths, null for a file that couldn't be read. Returns the paths to
    /// [`probe`] (JSON `[{path, rel}]`).
    pub fn match_hashes(&mut self, hashes: &str) -> Result<String, JsError> {
        let hashes: Vec<Option<String>> = from_json(hashes)?;
        let t = self.triage.take().ok_or_else(|| JsError::new("triage first"))?;
        let opts = self.opts.as_ref().unwrap();
        let hashes = hashes.into_iter().map(|h| h.ok_or_else(|| io::Error::other("the file can't be read"))).collect();
        let mt = sync::match_hashes(&self.m, t, hashes, opts);
        #[derive(Serialize)]
        struct P {
            path: String,
            rel: String,
        }
        let out: Vec<P> = mt
            .to_probe()
            .iter()
            .map(|f| P { path: f.abs.to_string_lossy().into_owned(), rel: f.rel.to_string_lossy().into_owned() })
            .collect();
        self.matched = Some(mt);
        to_json(&out)
    }

    /// Planning, step 3. `probed`: JSON `Probed[]` following
    /// [`Session::match_hashes`]; `card_files`: JSON `{cardPath: [size,
    /// modifiedMs]}` of the card's audio files. Returns the FLACs whose LPC
    /// order the plan needs (JSON `[{path, onCard}]`: a library path, or a
    /// card path), to read with `lpc_order` and pass to
    /// [`Session::apply_lpc`].
    pub fn finish(&mut self, probed: &str, card_files: &str) -> Result<String, JsError> {
        let probed: Vec<Probed> = from_json(probed)?;
        self.card_files = from_json(card_files)?;
        let mt = self.matched.take().ok_or_else(|| JsError::new("match hashes first"))?;
        let opts = self.opts.as_ref().unwrap();
        let card_files = &self.card_files;
        let plan = sync::finish(&self.m, mt, probed, opts, &|rel| {
            card_files.get(rel).map(|&(size, ms)| (size, (ms * 1e6) as i128))
        });
        self.lpc = sync::lpc_needed(&self.m, &plan, opts);
        self.plan = Some(plan);
        #[derive(Serialize)]
        #[serde(rename_all = "camelCase")]
        struct C<'a> {
            path: std::borrow::Cow<'a, str>,
            on_card: bool,
        }
        to_json(&self.lpc.iter().map(|c| C { path: c.path.to_string_lossy(), on_card: c.on_card }).collect::<Vec<_>>())
    }

    /// Planning, step 4. `orders`: JSON `(number | null)[]` following
    /// [`Session::finish`]'s list; `free`: free bytes on the card. Returns
    /// "What's new" (JSON `PlanSummary`).
    pub fn apply_lpc(&mut self, orders: &str, free: f64) -> Result<String, JsError> {
        let orders: Vec<Option<u8>> = from_json(orders)?;
        let plan = self.plan.as_mut().ok_or_else(|| JsError::new("finish first"))?;
        let opts = self.opts.as_ref().unwrap();
        let card_files = &self.card_files;
        sync::apply_lpc(&self.m, plan, &self.lpc, &orders, opts, &|rel| {
            card_files.get(rel).map(|&(size, ms)| (size, (ms * 1e6) as i128))
        });
        to_json(&view::plan_summary(plan, &self.m, free as u64))
    }

    /// Starts executing the plan: links the files already on the card and
    /// returns the files to write (JSON `[{source, dest, copy, policy,
    /// estimatedSize, reason}]`). Report each with [`Session::written`] or
    /// [`Session::failed`], saving [`Session::manifest`] now and then.
    pub fn start(&mut self) -> Result<String, JsError> {
        let plan = self.plan.take().ok_or_else(|| JsError::new("plan first"))?;
        self.info = PlanInfo::from(&plan);
        self.out = Outcome::default();
        self.writes = sync::apply_links(&mut self.m, plan);
        self.dests = sync::card_paths(&self.m, &self.writes);
        let out: Vec<WriteOut> = self
            .writes
            .iter()
            .zip(&self.dests)
            .map(|(w, dest)| WriteOut {
                source: w.source.abs.to_string_lossy().into_owned(),
                dest,
                copy: w.policy == Policy::Copy,
                policy: &w.policy,
                estimated_size: w.estimated_size,
                reason: match w.reason {
                    sync::WriteReason::New => "new",
                    sync::WriteReason::Upgrade => "upgrade",
                    sync::WriteReason::SourceChanged => "changed",
                    sync::WriteReason::Fix => "fix",
                },
            })
            .collect();
        to_json(&out)
    }

    /// Write `i` of [`Session::start`] landed on the card (`size` bytes).
    /// Returns the card path of the copy it replaced, to delete, if any.
    pub fn written(&mut self, i: usize, size: f64) -> Option<String> {
        let w = self.writes.get(i)?;
        sync::record_write(&mut self.m, w, &self.dests[i], size as u64, &mut self.out)
    }

    /// Write `i` of [`Session::start`] failed.
    pub fn failed(&mut self, i: usize, error: &str) {
        if let Some(w) = self.writes.get(i) {
            self.out.failed.push((w.source.abs.clone(), error.to_owned()));
        }
    }

    /// Card paths of the tracks whose art the index uses (one per album),
    /// for [`Session::add_art`] (JSON array).
    pub fn art_tracks(&self, album_key: &str) -> Result<String, JsError> {
        let ix = model::build(&library::index_records(&self.m), album_key_of(album_key));
        let paths: Vec<&str> = ix.album_art_track.iter().map(|&i| self.m.tracks[i].path.as_str()).collect();
        to_json(&paths)
    }

    /// Cover bytes of a track listed by [`Session::art_tracks`].
    pub fn add_art(&mut self, path: String, bytes: Vec<u8>) {
        self.arts.insert(path, bytes);
    }

    /// Builds the `opentone/*` index files; returns their names (JSON
    /// array), read them with [`Session::index_file`].
    pub fn build_index(&mut self, album_key: &str) -> Result<String, JsError> {
        let records = library::index_records(&self.m);
        let opts = index::Options { album_key: album_key_of(album_key), ..Default::default() };
        let arts = std::mem::take(&mut self.arts);
        let (files, summary) =
            index::build_index(&records, opts, |i| arts.get(&self.m.tracks[i].path).cloned()).map_err(js_err)?;
        let names: Vec<&str> = files.iter().map(|(n, _)| n.as_str()).collect();
        let json = to_json(&names)?;
        self.index_files = files;
        self.index_summary = Some(summary);
        Ok(json)
    }

    pub fn index_file(&self, name: &str) -> Option<Vec<u8>> {
        self.index_files.iter().find(|(n, _)| n == name).map(|(_, b)| b.clone())
    }

    /// Ends a sync: remembers it and its write speed (`write_secs`: time
    /// spent writing), and returns `{report, text}` (JSON; `report` is
    /// `opentone/last-sync.json`, `text` `opentone/sync-report.txt`).
    pub fn finish_sync(&mut self, seconds: f64, write_secs: f64, interrupted: bool) -> Result<String, JsError> {
        let machine = self.opts.as_ref().map_or_else(|| "browser".to_owned(), |o| o.machine.clone());
        self.out.write_secs = write_secs;
        self.out.interrupted = interrupted;
        library::note_sync(&mut self.m, &machine, &self.out);
        let report = library::report(&machine, &self.m, &self.info, &self.out, seconds as u64, self.index_summary.as_ref());
        let text = library::text_report_of(&machine, &self.m, &self.info, &self.out);
        #[derive(Serialize)]
        struct Done {
            report: view::SyncReport,
            text: String,
        }
        to_json(&Done { report, text })
    }

    /// "On the card" (JSON `CardAlbum[]`; covers are card paths).
    pub fn albums(&self) -> Result<String, JsError> {
        to_json(&view::card_albums(&self.m))
    }

    /// The Removed list (JSON `RemovedView[]`).
    pub fn removed(&self) -> Result<String, JsError> {
        to_json(&view::removed_albums(&self.m))
    }

    /// Track and album counts (JSON `{tracks, albums, bytes}`).
    pub fn stats(&self) -> Result<String, JsError> {
        let albums: std::collections::HashSet<String> = self.m.tracks.iter().map(|e| manifest::album_id(&e.tags)).collect();
        #[derive(Serialize)]
        struct S {
            tracks: usize,
            albums: usize,
            bytes: u64,
        }
        to_json(&S { tracks: self.m.tracks.len(), albums: albums.len(), bytes: self.m.tracks.iter().map(|e| e.size).sum() })
    }

    /// Removes albums (JSON array of ids) from the manifest and records
    /// them as removed; returns the card paths to delete (JSON array).
    pub fn forget(&mut self, ids: &str, machine: &str) -> Result<String, JsError> {
        let ids: Vec<String> = from_json(ids)?;
        let filter = library::album_filter(&self.m, &ids);
        let removed = library::forget(&mut self.m, &filter, machine);
        let paths: Vec<String> = removed.into_iter().map(|e| e.path).collect();
        to_json(&paths)
    }

    /// Brings a removed album back (the next sync copies it again).
    pub fn unforget(&mut self, id: &str) -> bool {
        library::unforget(&mut self.m, id)
    }
}

fn album_key_of(s: &str) -> AlbumKey {
    if s == "name" { AlbumKey::Name } else { AlbumKey::ArtistAlbum }
}

// ---- Podcasts ------------------------------------------------------------------

/// A file to copy for the podcast mirror, for JavaScript.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PodcastCopyOut<'a> {
    /// `pod/<rel>`.
    source: String,
    dest: &'a str,
    copy: bool,
    policy: &'a Policy,
    estimated_size: u64,
}

/// The podcast mirror of a sync (see `opentone_core::podcasts`): plans
/// against the card's `opentone/podcasts.json`, then JavaScript deletes,
/// renames and copies, and the session rebuilds the podcast tables and
/// remaps the listening state. Virtual paths are `pod/<rel>`.
#[wasm_bindgen]
pub struct PodcastSession {
    m: PodcastManifest,
    machine: String,
    triage: Option<mirror::Triage>,
    matched: Option<mirror::Matched>,
    plan: Option<mirror::MirrorPlan>,
    out: mirror::Outcome,
    arts: HashMap<String, Vec<u8>>,
    files: Vec<(String, Vec<u8>)>,
    summary: Option<podcasts::Summary>,
}

#[wasm_bindgen]
impl PodcastSession {
    /// `manifest`: the card's `opentone/podcasts.json`, if any.
    #[wasm_bindgen(constructor)]
    pub fn new(manifest: Option<String>) -> Result<PodcastSession, JsError> {
        Ok(PodcastSession {
            m: match manifest {
                Some(j) => from_json(&j)?,
                None => PodcastManifest::default(),
            },
            machine: String::new(),
            triage: None,
            matched: None,
            plan: None,
            out: mirror::Outcome::default(),
            arts: HashMap::new(),
            files: Vec::new(),
            summary: None,
        })
    }

    /// `opentone/podcasts.json`.
    pub fn manifest(&self) -> Result<String, JsError> {
        to_json(&self.m)
    }

    /// Planning, step 1: `files` as for [`Session::triage`] (paths
    /// `pod/<rel>`). Returns the paths to [`hash`].
    pub fn triage(&mut self, files: &str, machine: &str) -> Result<String, JsError> {
        let files: Vec<FileIn> = from_json(files)?;
        let files = files
            .into_iter()
            .map(|f| SourceFile {
                abs: PathBuf::from(f.path),
                rel: PathBuf::from(f.rel),
                size: f.size,
                mtime: (f.modified * 1e6) as i128,
            })
            .collect();
        self.machine = machine.to_owned();
        let t = mirror::triage(&self.m, files, machine);
        let out: Vec<String> = t.to_hash().iter().map(|f| f.abs.to_string_lossy().into_owned()).collect();
        self.triage = Some(t);
        to_json(&out)
    }

    /// Planning, step 2 (`hashes` as for [`Session::match_hashes`]). Returns
    /// the new episodes to [`probe_episode`] (JSON `[{path, rel}]`).
    pub fn match_hashes(&mut self, hashes: &str) -> Result<String, JsError> {
        let hashes: Vec<Option<String>> = from_json(hashes)?;
        let t = self.triage.take().ok_or_else(|| JsError::new("triage first"))?;
        let hashes = hashes.into_iter().map(|h| h.ok_or_else(|| io::Error::other("the file can't be read"))).collect();
        let mt = mirror::match_hashes(&self.m, t, hashes);
        #[derive(Serialize)]
        struct P {
            path: String,
            rel: String,
        }
        let out: Vec<P> = mt
            .to_probe()
            .iter()
            .map(|f| P { path: f.abs.to_string_lossy().into_owned(), rel: f.rel.to_string_lossy().into_owned() })
            .collect();
        self.matched = Some(mt);
        to_json(&out)
    }

    /// Planning, step 3. `probes`: JSON `Probe[]` following
    /// [`PodcastSession::match_hashes`]. Returns the podcast part of
    /// "What's new" (JSON `PodcastPlanSummary`).
    pub fn finish(&mut self, probes: &str) -> Result<String, JsError> {
        let probes: Vec<mirror::Probe> = from_json(probes)?;
        let mt = self.matched.take().ok_or_else(|| JsError::new("match hashes first"))?;
        // No ffmpeg in the browser: AAC/M4A episodes are skipped.
        let opts = mirror::Options { machine: self.machine.clone(), have_ffmpeg: false };
        let plan = mirror::finish(&self.m, mt, probes, &opts);
        let summary = view::podcast_plan_summary(&self.m, &plan);
        self.plan = Some(plan);
        to_json(&summary)
    }

    /// Starts executing the plan: records the moves in the manifest and
    /// returns what to do on the card, in order (JSON `{deletes: [path],
    /// renames: [[from, to]], copies: [{source, dest, copy, policy,
    /// estimatedSize}]}`). Report each copy with [`PodcastSession::copied`]
    /// or [`PodcastSession::failed`].
    pub fn start(&mut self) -> Result<String, JsError> {
        let plan = self.plan.as_ref().ok_or_else(|| JsError::new("plan first"))?;
        self.out = mirror::Outcome::default();
        let renames = mirror::apply_links(&mut self.m, plan, &self.machine);
        self.out.deleted = plan.deletes.len();
        self.out.moved = renames.len();
        #[derive(Serialize)]
        struct Ops<'a> {
            deletes: &'a [String],
            renames: Vec<(String, String)>,
            copies: Vec<PodcastCopyOut<'a>>,
        }
        let copies = plan
            .copies
            .iter()
            .map(|c| PodcastCopyOut {
                source: c.source.abs.to_string_lossy().into_owned(),
                dest: &c.dest,
                copy: c.policy == Policy::Copy,
                policy: &c.policy,
                estimated_size: c.estimated_size,
            })
            .collect();
        to_json(&Ops { deletes: &plan.deletes, renames, copies })
    }

    /// Copy `i` of [`PodcastSession::start`] landed on the card.
    pub fn copied(&mut self, i: usize, size: f64) {
        if let Some(c) = self.plan.as_ref().and_then(|p| p.copies.get(i)) {
            mirror::record_copy(&mut self.m, c, size as u64);
            self.out.copied.push(c.dest.clone());
            self.out.bytes += size as u64;
        }
    }

    /// Copy `i` of [`PodcastSession::start`] failed.
    pub fn failed(&mut self, i: usize, error: &str) {
        if let Some(c) = self.plan.as_ref().and_then(|p| p.copies.get(i)) {
            self.out.failed.push((c.source.abs.clone(), error.to_owned()));
        }
    }

    /// Whether the card's podcasts changed (the tables need rebuilding).
    pub fn changed(&self) -> bool {
        self.out.changed()
    }

    /// Each folder's cover source: JSON `[[cardDir, firstEpisodePath |
    /// null]]`. Give the bytes found (a cover file in the directory, else
    /// the episode's embedded art) to [`PodcastSession::add_art`].
    pub fn art_sources(&self) -> Result<String, JsError> {
        to_json(&podcasts::art_sources(&self.m))
    }

    pub fn add_art(&mut self, dir: String, bytes: Vec<u8>) {
        self.arts.insert(dir, bytes);
    }

    /// Builds the podcast tables and remaps the listening state
    /// (`old_state`: the card's `opentone/podcast_state.bin`). Returns the
    /// file names in write order (JSON array): write them in that order,
    /// read them with [`PodcastSession::index_file`], then save
    /// [`PodcastSession::manifest`] (its generation changed).
    pub fn build_index(&mut self, old_state: Option<Vec<u8>>) -> Result<String, JsError> {
        let arts = std::mem::take(&mut self.arts);
        let (files, summary) =
            podcasts::build_files(&mut self.m, old_state.as_deref(), Default::default(), |f, _| arts.get(&f.dir).cloned());
        let names: Vec<&str> = files.iter().map(|(n, _)| n.as_str()).collect();
        let json = to_json(&names)?;
        self.files = files;
        self.summary = Some(summary);
        Ok(json)
    }

    pub fn index_file(&self, name: &str) -> Option<Vec<u8>> {
        self.files.iter().find(|(n, _)| n == name).map(|(_, b)| b.clone())
    }

    /// What the mirror did: JSON `{report, text}` (`report` goes in
    /// `last-sync.json`'s `podcasts`, `text` at the end of
    /// `sync-report.txt`).
    pub fn report(&self, interrupted: bool) -> Result<String, JsError> {
        let plan = self.plan.as_ref().ok_or_else(|| JsError::new("plan first"))?;
        let mut out = mirror::Outcome { interrupted, ..Default::default() };
        out.copied = self.out.copied.clone();
        out.moved = self.out.moved;
        out.deleted = self.out.deleted;
        out.bytes = self.out.bytes;
        out.failed = self.out.failed.clone();
        let report = podcasts::Report::new(plan, &out, &self.m, self.summary.as_ref());
        #[derive(Serialize)]
        struct Done {
            report: podcasts::Report,
            text: String,
        }
        let text = report.text();
        to_json(&Done { report, text })
    }
}
