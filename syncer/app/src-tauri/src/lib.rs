//! OpenPod Sync desktop app: Tauri commands over `openpod-core`. The Vue
//! frontend (`../src`) calls these and listens to the progress events.

mod cards;
mod covers;
mod progress;

use std::collections::{HashSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use openpod_core::card::{self, CardInfo, CardState, free_space};
use openpod_core::config::{self, Config};
use openpod_core::index::AlbumKey;
use openpod_core::library::{self, PlanInfo};
use openpod_core::manifest::{self, Manifest};
use openpod_core::podcasts::{self, PodcastManifest, mirror};
use openpod_core::sync::{self, PlanOptions};
use openpod_core::view::{self, CardAlbum, PlanSummary, RemovedView, SyncReport};
use openpod_core::convert;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter, Manager, State};

type Res<T> = Result<T, String>;

fn err(e: impl std::fmt::Display) -> String {
    e.to_string()
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Job {
    Idle,
    Reading,
    Syncing,
}

struct App {
    cfg: Mutex<Config>,
    job: Mutex<Job>,
    stop: Arc<AtomicBool>,
    reporter: Mutex<Option<Arc<progress::Reporter>>>,
    covers: covers::Cache,
}

impl App {
    fn card(&self) -> Res<PathBuf> {
        self.cfg.lock().unwrap().card().ok_or_else(|| "No card chosen".to_owned())
    }

    /// Claims the single background job slot.
    fn begin(&self, job: Job) -> Res<()> {
        let mut j = self.job.lock().unwrap();
        match *j {
            Job::Idle => {
                *j = job;
                Ok(())
            }
            Job::Reading => Err("The card is being read".into()),
            Job::Syncing => Err("A sync is running".into()),
        }
    }

    fn end(&self) {
        *self.job.lock().unwrap() = Job::Idle;
    }
}

// ---- Settings -------------------------------------------------------------

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct LibraryInfo {
    path: String,
    exists: bool,
    tracks: usize,
    albums: usize,
    formats: Vec<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Settings {
    machine: String,
    hostname: String,
    libraries: Vec<LibraryInfo>,
    /// The podcasts folder mirrored to the card, if set.
    podcasts: Option<PodcastsInfo>,
    exclude: Vec<String>,
    album_key: AlbumKey,
    /// ffmpeg's version when it is installed.
    ffmpeg: Option<String>,
    config_path: String,
}

fn library_info(path: &Path, exclude: &[String]) -> LibraryInfo {
    let files = if path.is_dir() { sync::scan(&[path.to_path_buf()], exclude).unwrap_or_default() } else { Vec::new() };
    let albums: HashSet<&Path> = files.iter().filter_map(|f| f.abs.parent()).collect();
    let mut formats: Vec<String> = Vec::new();
    for f in &files {
        let ext = f.abs.extension().map(|e| e.to_string_lossy().to_uppercase()).unwrap_or_default();
        if !ext.is_empty() && !formats.contains(&ext) {
            formats.push(ext);
        }
    }
    formats.sort_by_key(|f| match f.as_str() {
        "FLAC" => 0,
        "MP3" => 1,
        _ => 2,
    });
    LibraryInfo { path: path.to_string_lossy().into_owned(), exists: path.is_dir(), tracks: files.len(), albums: albums.len(), formats }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PodcastsInfo {
    path: String,
    exists: bool,
    episodes: usize,
    /// Folders holding episodes.
    shows: usize,
}

fn podcasts_info(path: &Path) -> PodcastsInfo {
    let files = if path.is_dir() { sync::scan(&[path.to_path_buf()], &[]).unwrap_or_default() } else { Vec::new() };
    let shows: HashSet<&Path> = files.iter().filter_map(|f| f.abs.parent()).collect();
    PodcastsInfo { path: path.to_string_lossy().into_owned(), exists: path.is_dir(), episodes: files.len(), shows: shows.len() }
}

/// Music excludes, plus the podcasts folder when it sits in a library.
fn music_excludes(cfg: &Config) -> Vec<String> {
    let mut exclude = cfg.exclude.clone();
    if let Some(p) = cfg.podcasts() {
        exclude.extend(podcasts::library_excludes(&cfg.libraries(), &p));
    }
    exclude
}

fn ffmpeg_version() -> Option<String> {
    let out = std::process::Command::new("ffmpeg").arg("-version").output().ok()?;
    let text = String::from_utf8_lossy(&out.stdout);
    let v = text.split_whitespace().nth(2)?;
    // "6.1.1-3ubuntu5" → "6.1.1"
    Some(v.split(['-', '+']).next().unwrap_or(v).to_owned())
}

fn settings_of(cfg: &Config) -> Settings {
    Settings {
        machine: cfg.machine(),
        hostname: config::hostname(),
        libraries: cfg.libraries().iter().map(|p| library_info(p, &music_excludes(cfg))).collect(),
        podcasts: cfg.podcasts().map(|p| podcasts_info(&p)),
        exclude: cfg.exclude.clone(),
        album_key: cfg.album_key(),
        ffmpeg: ffmpeg_version(),
        config_path: Config::path().map(|p| p.to_string_lossy().into_owned()).unwrap_or_default(),
    }
}

#[tauri::command]
async fn settings(app: State<'_, App>) -> Res<Settings> {
    let cfg = app.cfg.lock().unwrap().clone();
    tauri::async_runtime::spawn_blocking(move || settings_of(&cfg)).await.map_err(err)
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct SettingsUpdate {
    machine: Option<String>,
    libraries: Option<Vec<String>>,
    exclude: Option<Vec<String>>,
    album_key: Option<AlbumKey>,
    /// "" unsets it.
    podcasts: Option<String>,
}

#[tauri::command]
async fn save_settings(app: State<'_, App>, update: SettingsUpdate) -> Res<Settings> {
    let cfg = {
        let mut cfg = app.cfg.lock().unwrap();
        if let Some(m) = update.machine {
            let m = m.trim().to_owned();
            cfg.machine = (!m.is_empty() && m != config::hostname()).then_some(m);
        }
        if let Some(l) = update.libraries {
            cfg.libraries = l.into_iter().map(PathBuf::from).collect();
        }
        if let Some(e) = update.exclude {
            cfg.exclude = e;
        }
        if let Some(k) = update.album_key {
            cfg.album_key = Some(k);
        }
        if let Some(p) = update.podcasts {
            cfg.podcasts = (!p.trim().is_empty()).then(|| PathBuf::from(p.trim()));
        }
        cfg.save().map_err(err)?;
        cfg.clone()
    };
    tauri::async_runtime::spawn_blocking(move || settings_of(&cfg)).await.map_err(err)
}

// ---- Cards ----------------------------------------------------------------

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Cards {
    /// The card syncs go to (None: nothing usable found).
    selected: Option<CardInfo>,
    /// Other cards plugged in.
    others: Vec<CardInfo>,
    busy: Option<&'static str>,
}

fn info(path: &Path, vols: &[cards::Volume]) -> CardInfo {
    let vol = vols.iter().find(|v| v.mount == path);
    card::inspect(path, vol.and_then(|v| v.label.clone()), vol.is_some())
}

#[tauri::command]
async fn cards(app: State<'_, App>) -> Res<Cards> {
    let configured = app.cfg.lock().unwrap().card();
    let busy = match *app.job.lock().unwrap() {
        Job::Idle => None,
        Job::Reading => Some("reading"),
        Job::Syncing => Some("syncing"),
    };
    let (selected, others) = tauri::async_runtime::spawn_blocking(move || {
        let vols = cards::detect();
        let mut all: Vec<CardInfo> = vols.iter().map(|v| info(&v.mount, &vols)).collect();
        if let Some(c) = configured.as_ref().filter(|c| !vols.iter().any(|v| v.mount == **c)) {
            all.insert(0, info(c, &vols));
        }
        let configured = configured.map(|p| p.to_string_lossy().into_owned());
        // The configured card first, then one OpenPod already uses, then any.
        let pick = all
            .iter()
            .position(|c| Some(c.path.as_str()) == configured.as_deref() && !matches!(c.state, CardState::Missing))
            .or_else(|| all.iter().position(|c| matches!(c.state, CardState::Ready { .. })))
            .or_else(|| all.iter().position(|c| !matches!(c.state, CardState::Missing)));
        let selected = pick.map(|i| all.remove(i));
        (selected, all.into_iter().filter(|c| !matches!(c.state, CardState::Missing)).collect())
    })
    .await
    .map_err(err)?;
    if let Some(s) = &selected {
        let mut cfg = app.cfg.lock().unwrap();
        if cfg.card().as_deref() != Some(Path::new(&s.path)) {
            cfg.card = Some(PathBuf::from(&s.path));
            let _ = cfg.save();
        }
    }
    Ok(Cards { selected, others, busy })
}

#[tauri::command]
fn select_card(app: State<'_, App>, path: String) -> Res<()> {
    let mut cfg = app.cfg.lock().unwrap();
    cfg.card = Some(PathBuf::from(path));
    cfg.save().map_err(err)
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct ReadProgress {
    files_done: usize,
    files: usize,
    fraction: f64,
    elapsed: f64,
    eta: Option<f64>,
}

/// Reads an existing card once (`library::adopt`), emitting `card-read`
/// and finally `card-read-done` (an error message, or null).
#[tauri::command]
fn read_card(handle: AppHandle, app: State<'_, App>) -> Res<()> {
    let card = app.card()?;
    app.begin(Job::Reading)?;
    std::thread::spawn(move || {
        let start = Instant::now();
        let window: Mutex<(VecDeque<(Instant, u64)>, Option<Instant>)> = Mutex::new((VecDeque::new(), None));
        let r = library::adopt(&card, &|p| {
            let now = Instant::now();
            let mut w = window.lock().unwrap();
            w.0.push_back((now, p.cost_done));
            while w.0.len() > 2 && now.duration_since(w.0[1].0) > Duration::from_secs(10) {
                w.0.pop_front();
            }
            if w.1.is_some_and(|t| now.duration_since(t) < Duration::from_millis(200)) && p.files_done < p.files {
                return;
            }
            w.1 = Some(now);
            let (t0, c0) = w.0[0];
            let secs = now.duration_since(t0).as_secs_f64();
            let eta = (secs > 2.0 && p.cost_done > c0)
                .then(|| (p.cost - p.cost_done) as f64 / ((p.cost_done - c0) as f64 / secs));
            let _ = handle.emit(
                "card-read",
                ReadProgress {
                    files_done: p.files_done,
                    files: p.files,
                    fraction: if p.cost > 0 { p.cost_done as f64 / p.cost as f64 } else { 0.0 },
                    elapsed: start.elapsed().as_secs_f64(),
                    eta,
                },
            );
        })
        .and_then(|m| manifest::save(&card, &m));
        handle.state::<App>().end();
        let _ = handle.emit("card-read-done", r.err().map(|e| e.to_string()));
    });
    Ok(())
}

#[tauri::command]
async fn eject(app: State<'_, App>) -> Res<()> {
    let card = app.card()?;
    tauri::async_runtime::spawn_blocking(move || cards::eject(&card)).await.map_err(err)?
}

// ---- What's new and sync --------------------------------------------------

struct Prepared {
    card: PathBuf,
    machine: String,
    album_key: AlbumKey,
    m: Manifest,
    plan: sync::Plan,
    /// The podcast mirror, when a podcasts folder is set.
    podcasts: Option<(PodcastManifest, mirror::MirrorPlan)>,
}

/// Scans the libraries and plans against the card's manifest.
fn prepare(cfg: &Config) -> Res<Prepared> {
    let card = cfg.card().ok_or("No card chosen")?;
    if !manifest::exists(&card) {
        // A blank card starts an empty manifest; one with music must be read first.
        match card::inspect(&card, None, true).state {
            CardState::Empty => manifest::save(&card, &Manifest::default()).map_err(err)?,
            CardState::Unread { .. } => return Err("The card has to be read first".into()),
            CardState::Locked { reason } => return Err(format!("This card can't be written to: {reason}")),
            _ => return Err("No card found".into()),
        }
    }
    let m = manifest::load(&card).map_err(err)?;
    let libraries = cfg.libraries();
    if libraries.is_empty() {
        return Err("Add a music folder first".into());
    }
    let files = sync::scan(&libraries, &music_excludes(cfg)).map_err(err)?;
    let opts = PlanOptions {
        machine: cfg.machine(),
        have_ffmpeg: convert::ffmpeg_available(),
        card: card.clone(),
        include_removed: false,
    };
    let plan = sync::plan(&m, files, &opts);
    let podcasts = match cfg.podcasts() {
        Some(root) if root.is_dir() => {
            let pm = podcasts::load(&card).map_err(err)?;
            let popts = mirror::Options { machine: opts.machine.clone(), have_ffmpeg: opts.have_ffmpeg };
            let pplan = mirror::plan(&pm, &root, &popts).map_err(err)?;
            Some((pm, pplan))
        }
        Some(root) => return Err(format!("Podcasts folder not found: {}", root.display())),
        None => None,
    };
    Ok(Prepared { card, machine: opts.machine, album_key: cfg.album_key(), m, plan, podcasts })
}

#[tauri::command]
async fn check(app: State<'_, App>) -> Res<PlanSummary> {
    let cfg = app.cfg.lock().unwrap().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let p = prepare(&cfg)?;
        let free = free_space(&p.card);
        let summary = view::plan_summary(&p.plan, &p.m, free);
        Ok(match &p.podcasts {
            Some((pm, plan)) => summary.with_podcasts(pm, plan),
            None => summary,
        })
    })
    .await
    .map_err(err)?
}


#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct SyncDone {
    report: Option<SyncReport>,
    error: Option<String>,
}

/// Starts a sync in the background: `sync-progress` events while it runs,
/// then `sync-done`.
#[tauri::command]
fn sync_start(handle: AppHandle, app: State<'_, App>) -> Res<()> {
    let cfg = app.cfg.lock().unwrap().clone();
    app.begin(Job::Syncing)?;
    app.stop.store(false, Ordering::SeqCst);
    let stop = app.stop.clone();
    std::thread::spawn(move || {
        let state = handle.state::<App>();
        let result = (|| -> Res<SyncReport> {
            let card = cfg.card().ok_or("No card chosen")?;
            card::check_writable(&card).map_err(|e| format!("This card can't be written to: {e}"))?;
            sync::remove_partial_files(&card);
            let Prepared { card, machine, album_key, mut m, plan, podcasts } = prepare(&cfg)?;
            let free = free_space(&card);
            let pod_bytes = podcasts.as_ref().map_or(0, |(_, p)| p.bytes_needed());
            let pod_files = podcasts.as_ref().map_or(0, |(_, p)| p.copies.len());
            if plan.bytes_needed() + pod_bytes > free {
                return Err("Not enough space on the card".into());
            }
            let info = PlanInfo::from(&plan);
            // One reporter for both passes: one bar, one time left.
            let reporter = Arc::new(progress::Reporter::new(
                handle.clone(),
                &card,
                plan.writes.len() + pod_files,
                plan.bytes_needed() + pod_bytes,
            ));
            *state.reporter.lock().unwrap() = Some(reporter.clone());
            let jobs = std::thread::available_parallelism().map_or(4, |n| n.get());
            let start = Instant::now();
            let out = sync::execute(&card, &mut m, plan, jobs, &stop, &*reporter).map_err(err)?;
            reporter.flush();
            let mut done = library::finish_sync(&card, &mut m, &machine, album_key, &info, &out, start.elapsed().as_secs())
                .map_err(err)?;
            // Then the podcast mirror, continuing the same bar.
            if let Some((mut pm, pplan)) = podcasts.filter(|_| !stop.load(Ordering::SeqCst)) {
                let pout = mirror::execute(&card, &mut pm, &pplan, &machine, jobs, &stop, &*reporter).map_err(err)?;
                reporter.flush();
                let (report, _) = podcasts::finish(&card, &mut pm, &pplan, &pout).map_err(err)?;
                done.report.podcasts = Some(report);
            }
            Ok(done.report)
        })();
        *state.reporter.lock().unwrap() = None;
        state.end();
        let done = match result {
            Ok(r) => SyncDone { report: Some(r), error: None },
            Err(e) => SyncDone { report: None, error: Some(e) },
        };
        let _ = handle.emit("sync-done", done);
    });
    Ok(())
}

/// Finishes the files being written, then stops (`sync-done` follows).
#[tauri::command]
fn sync_stop(app: State<'_, App>) {
    app.stop.store(true, Ordering::SeqCst);
}

/// The running sync's progress (for a window that reloads mid-sync).
#[tauri::command]
fn sync_status(app: State<'_, App>) -> Option<progress::SyncProgress> {
    app.reporter.lock().unwrap().as_ref().map(|r| r.snapshot())
}

#[tauri::command]
fn report(app: State<'_, App>) -> Res<Option<SyncReport>> {
    Ok(view::load_report(&app.card()?))
}

// ---- On the card ----------------------------------------------------------

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct LibraryView {
    albums: Vec<CardAlbum>,
    removed: Vec<RemovedView>,
    tracks: usize,
    bytes: u64,
    capacity: u64,
}

fn library_view(card: &Path) -> Res<LibraryView> {
    let m = manifest::load(card).map_err(err)?;
    let mut albums = view::card_albums(&m);
    for a in &mut albums {
        a.cover = card.join(&a.cover).to_string_lossy().into_owned();
    }
    Ok(LibraryView {
        tracks: m.tracks.len(),
        bytes: m.tracks.iter().map(|e| e.size).sum(),
        capacity: card::capacity(card),
        removed: view::removed_albums(&m),
        albums,
    })
}

#[tauri::command]
async fn library_albums(app: State<'_, App>) -> Res<LibraryView> {
    let card = app.card()?;
    tauri::async_runtime::spawn_blocking(move || library_view(&card)).await.map_err(err)?
}

#[tauri::command]
async fn remove_albums(app: State<'_, App>, ids: Vec<String>) -> Res<LibraryView> {
    let card = app.card()?;
    let (machine, album_key) = {
        let cfg = app.cfg.lock().unwrap();
        (cfg.machine(), cfg.album_key())
    };
    app.begin(Job::Syncing)?;
    let r = tauri::async_runtime::spawn_blocking(move || {
        let mut m = manifest::load(&card).map_err(err)?;
        library::remove_albums(&card, &mut m, &ids, &machine).map_err(err)?;
        library::reindex(&card, &m, album_key).map_err(err)?;
        library_view(&card)
    })
    .await
    .map_err(err);
    app.end();
    r?
}

#[tauri::command]
async fn bring_back(app: State<'_, App>, id: String) -> Res<LibraryView> {
    let card = app.card()?;
    tauri::async_runtime::spawn_blocking(move || {
        let mut m = manifest::load(&card).map_err(err)?;
        library::bring_back(&card, &mut m, &id).map_err(err)?;
        library_view(&card)
    })
    .await
    .map_err(err)?
}

/// The card's podcasts by show, with the listening state the player wrote.
fn podcasts_view(card: &Path, machine: &str) -> Res<view::CardPodcasts> {
    let m = podcasts::load(card).map_err(err)?;
    let state = std::fs::read(card.join(manifest::DIR).join(podcasts::state::FILE)).ok();
    let mut v = view::card_podcasts(&m, state.as_deref(), machine);
    for s in &mut v.shows {
        s.cover = card.join(&s.cover).to_string_lossy().into_owned();
    }
    Ok(v)
}

#[tauri::command]
async fn podcasts(app: State<'_, App>) -> Res<view::CardPodcasts> {
    let card = app.card()?;
    let machine = app.cfg.lock().unwrap().machine();
    tauri::async_runtime::spawn_blocking(move || podcasts_view(&card, &machine)).await.map_err(err)?
}

/// How many latest episodes a show keeps on the card (None: all). The next
/// sync applies it.
#[tauri::command]
async fn set_podcast_keep(app: State<'_, App>, show: String, keep: Option<u32>) -> Res<view::CardPodcasts> {
    let card = app.card()?;
    let machine = app.cfg.lock().unwrap().machine();
    tauri::async_runtime::spawn_blocking(move || {
        podcasts::set_keep(&card, &show, keep).map_err(err)?;
        podcasts_view(&card, &machine)
    })
    .await
    .map_err(err)?
}

/// Marks a podcast episode (card path) played or not, as the player's
/// "Mark as played/unplayed" does.
#[tauri::command]
async fn set_podcast_played(app: State<'_, App>, path: String, played: bool) -> Res<view::CardPodcasts> {
    let card = app.card()?;
    let machine = app.cfg.lock().unwrap().machine();
    tauri::async_runtime::spawn_blocking(move || {
        podcasts::set_played(&card, &path, played).map_err(err)?;
        podcasts_view(&card, &machine)
    })
    .await
    .map_err(err)?
}

/// Copies this app's binary to `<card>/openpod/tools/`, so the card carries
/// it to other computers.
#[tauri::command]
async fn install_on_card(app: State<'_, App>) -> Res<String> {
    let card = app.card()?;
    tauri::async_runtime::spawn_blocking(move || {
        let dir = card.join(manifest::DIR).join("tools");
        std::fs::create_dir_all(&dir).map_err(err)?;
        let ext = if cfg!(windows) { ".exe" } else { "" };
        let name = format!("openpod-sync-{}-{}{ext}", std::env::consts::OS, std::env::consts::ARCH);
        let exe = std::env::current_exe().map_err(err)?;
        std::fs::copy(exe, dir.join(&name)).map_err(err)?;
        Ok(name)
    })
    .await
    .map_err(err)?
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let cfg = Config::load().unwrap_or_default();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(App {
            cfg: Mutex::new(cfg),
            job: Mutex::new(Job::Idle),
            stop: Arc::new(AtomicBool::new(false)),
            reporter: Mutex::new(None),
            covers: covers::Cache::default(),
        })
        .register_asynchronous_uri_scheme_protocol("cover", |ctx, request, responder| {
            let handle = ctx.app_handle().clone();
            std::thread::spawn(move || {
                let app = handle.state::<App>();
                responder.respond(covers::respond(&app.covers, request.uri().path()));
            });
        })
        .invoke_handler(tauri::generate_handler![
            settings,
            save_settings,
            cards,
            select_card,
            read_card,
            eject,
            check,
            sync_start,
            sync_stop,
            sync_status,
            report,
            library_albums,
            remove_albums,
            bring_back,
            podcasts,
            set_podcast_keep,
            set_podcast_played,
            install_on_card,
        ])
        .run(tauri::generate_context!())
        .expect("error while running OpenPod Sync");
}
