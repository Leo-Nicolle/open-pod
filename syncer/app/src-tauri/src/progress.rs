//! Sync progress as `sync-progress` events, at most every [`EVERY`].

use std::collections::VecDeque;
use std::path::Path;
use std::sync::Mutex;
use std::time::{Duration, Instant};

use openpod_core::{sync, view};
use serde::Serialize;
use tauri::{AppHandle, Emitter};

const EVERY: Duration = Duration::from_millis(200);
/// Speed and time left follow the last 10 s, not the average since the
/// start.
const WINDOW: Duration = Duration::from_secs(10);
const RECENT: usize = 6;

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Copying {
    pub path: String,
    pub done: u64,
    pub total: u64,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Converting {
    pub worker: usize,
    pub path: String,
    pub what: String,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RecentAlbum {
    /// Card folder ("Artist/Album").
    pub dir: String,
    pub title: String,
    pub artist: String,
    pub tracks: usize,
    /// Absolute card path of a written track, for the cover.
    pub cover: String,
}

#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SyncProgress {
    pub bytes_done: u64,
    pub bytes_total: u64,
    pub files_done: usize,
    pub files_total: usize,
    pub speed: Option<f64>,
    pub elapsed: f64,
    pub eta: Option<f64>,
    pub copying: Option<Copying>,
    pub converting: Vec<Converting>,
    pub recent: Vec<RecentAlbum>,
}

struct State {
    p: SyncProgress,
    /// Estimated bytes of the files already written (the bar's position
    /// without the file in progress).
    advanced: u64,
    samples: VecDeque<(Instant, u64)>,
    last_emit: Option<Instant>,
}

pub struct Reporter {
    app: AppHandle,
    card: std::path::PathBuf,
    start: Instant,
    state: Mutex<State>,
}

/// "Artist / Album / 01 Title.flac" from a path's last three components.
pub fn short_path(p: &Path) -> String {
    let parts: Vec<String> = p.components().rev().take(3).map(|c| c.as_os_str().to_string_lossy().into_owned()).collect();
    parts.into_iter().rev().collect::<Vec<_>>().join(" / ")
}

impl Reporter {
    pub fn new(app: AppHandle, card: &Path) -> Self {
        Self {
            app,
            card: card.to_path_buf(),
            start: Instant::now(),
            state: Mutex::new(State {
                p: SyncProgress::default(),
                advanced: 0,
                samples: VecDeque::new(),
                last_emit: None,
            }),
        }
    }

    /// Updates speed/ETA and emits, unless one went out less than
    /// [`EVERY`] ago (`force` sends anyway).
    fn emit(&self, s: &mut State, force: bool) {
        let now = Instant::now();
        let pos = s.p.bytes_done;
        s.samples.push_back((now, pos));
        while s.samples.len() > 2 && now.duration_since(s.samples[1].0) > WINDOW {
            s.samples.pop_front();
        }
        let (t0, d0) = s.samples[0];
        let secs = now.duration_since(t0).as_secs_f64();
        if secs > 2.0 && pos > d0 {
            let speed = (pos - d0) as f64 / secs;
            s.p.speed = Some(speed);
            s.p.eta = Some(s.p.bytes_total.saturating_sub(pos) as f64 / speed);
        }
        s.p.elapsed = self.start.elapsed().as_secs_f64();
        if force || s.last_emit.is_none_or(|t| now.duration_since(t) >= EVERY) {
            s.last_emit = Some(now);
            let _ = self.app.emit("sync-progress", s.p.clone());
        }
    }

    pub fn flush(&self) {
        let mut s = self.state.lock().unwrap();
        self.emit(&mut s, true);
    }

    pub fn snapshot(&self) -> SyncProgress {
        self.state.lock().unwrap().p.clone()
    }
}

impl sync::Progress for Reporter {
    fn start(&self, files: usize, bytes: u64) {
        let mut s = self.state.lock().unwrap();
        s.p.files_total = files;
        s.p.bytes_total = bytes;
        self.emit(&mut s, true);
    }

    fn advance(&self, bytes: u64, label: &str) {
        let mut s = self.state.lock().unwrap();
        s.advanced += bytes;
        s.p.bytes_done = s.advanced.min(s.p.bytes_total);
        s.p.files_done += 1;
        s.p.copying = None;
        if let Some((dir, _)) = label.rsplit_once('/') {
            match s.p.recent.iter().position(|r| r.dir == dir) {
                Some(i) => {
                    let mut r = s.p.recent.remove(i);
                    r.tracks += 1;
                    s.p.recent.insert(0, r);
                }
                None => {
                    let (artist, title) = dir.rsplit_once('/').unwrap_or(("", dir));
                    s.p.recent.insert(
                        0,
                        RecentAlbum {
                            dir: dir.to_owned(),
                            title: title.to_owned(),
                            artist: artist.to_owned(),
                            tracks: 1,
                            cover: self.card.join(label).to_string_lossy().into_owned(),
                        },
                    );
                    s.p.recent.truncate(RECENT);
                }
            }
        }
        self.emit(&mut s, false);
    }

    fn warn(&self, msg: &str) {
        let _ = self.app.emit("sync-warning", msg.to_owned());
    }

    fn copying(&self, dest: &str, done: u64, total: u64) {
        let mut s = self.state.lock().unwrap();
        // The bar moves within the file too, scaled to its estimated share.
        s.p.bytes_done = (s.advanced + done).min(s.p.bytes_total);
        s.p.copying = Some(Copying { path: short_path(Path::new(dest)), done, total });
        self.emit(&mut s, false);
    }

    fn converting(&self, worker: usize, src: &Path, what: Option<&str>) {
        let mut s = self.state.lock().unwrap();
        s.p.converting.retain(|c| c.worker != worker);
        if let Some(what) = what {
            let what = view::conversion_detail(what).unwrap_or_else(|| what.to_owned());
            s.p.converting.push(Converting { worker, path: short_path(src), what });
            s.p.converting.sort_by_key(|c| c.worker);
        }
        self.emit(&mut s, false);
    }
}
