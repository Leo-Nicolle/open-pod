mod config;

use std::collections::BTreeMap;
use std::io::{BufRead, Write as _};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::{Duration, Instant};

use anyhow::{Context, bail};
use clap::{Args, Parser, Subcommand, ValueEnum};
use indicatif::{ProgressBar, ProgressStyle};
use opentone_core::index::{self, AlbumKey, thumbs};
use opentone_core::library::{self, Filter, PlanInfo};
use opentone_core::sync::{self, PlanOptions, WriteReason};
use opentone_core::podcasts::{self, mirror};
use opentone_core::{convert, manifest, policy, tags};

#[derive(Parser)]
#[command(name = "opentone", version, about = "opentone SD card library tool")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Args)]
struct CardArg {
    /// Mounted SD card (default: $OPENTONE_SD, then `card` in the config file).
    #[arg(long, short)]
    card: Option<PathBuf>,
}

#[derive(Args)]
struct LibraryArgs {
    /// Music libraries to sync from (default: $OPENTONE_LIBRARY, then
    /// `libraries` in the config file).
    libraries: Vec<PathBuf>,
    #[command(flatten)]
    card: CardArg,
    /// Library-relative folder to skip (repeatable; adds to the config's `exclude`).
    #[arg(long, short = 'x')]
    exclude: Vec<String>,
    /// Name of this computer in the card's manifest (default: config, then hostname).
    #[arg(long)]
    machine: Option<String>,
    /// Also bring back tracks that were removed with `rm`.
    #[arg(long)]
    include_removed: bool,
    /// Podcasts folder to mirror to <card>/Podcasts (default:
    /// $OPENTONE_PODCASTS, then `podcasts` in the config file). The mirror
    /// adds, moves and deletes episodes.
    #[arg(long)]
    podcasts: Option<PathBuf>,
}

#[derive(Subcommand)]
enum Command {
    /// Copy what the card is missing from your libraries, converting files the
    /// player can't play. Never deletes anything (see `rm`).
    Sync {
        #[command(flatten)]
        lib: LibraryArgs,
        /// Parallel conversions (default: CPU count).
        #[arg(long, short)]
        jobs: Option<usize>,
        #[arg(long, value_enum)]
        album_key: Option<AlbumKeyArg>,
    },
    /// Show what `sync` would do, without touching the card.
    Status {
        #[command(flatten)]
        lib: LibraryArgs,
        /// List every file that would be written.
        #[arg(long, short)]
        verbose: bool,
    },
    /// List the albums on the card.
    Ls {
        #[command(flatten)]
        card: CardArg,
        #[arg(long)]
        artist: Option<String>,
        #[arg(long)]
        album: Option<String>,
        /// List tracks instead of albums.
        #[arg(long, short)]
        tracks: bool,
    },
    /// Remove tracks from the card (files, manifest, index).
    Rm {
        #[command(flatten)]
        card: CardArg,
        /// Track artist or album artist (case-insensitive).
        #[arg(long)]
        artist: Option<String>,
        #[arg(long)]
        album: Option<String>,
        /// Card path or folder, e.g. "Yoa/La Favorite".
        #[arg(long)]
        path: Option<String>,
        /// Don't ask for confirmation.
        #[arg(long, short)]
        yes: bool,
    },
    /// Create the card's manifest from the music already on it (done
    /// automatically by the first `sync`).
    Adopt {
        #[command(flatten)]
        card: CardArg,
        /// Replace an existing manifest.
        #[arg(long)]
        force: bool,
    },
    /// Rebuild the opentone/*.bin index: from the card's manifest, or (with
    /// MUSIC and OUT) by crawling a folder laid out like the card.
    Index {
        /// Music folder to crawl (legacy mode, no manifest needed).
        music: Option<PathBuf>,
        /// Output folder for the legacy mode (usually <card>/opentone).
        out: Option<PathBuf>,
        #[command(flatten)]
        card: CardArg,
        #[arg(long, value_enum)]
        album_key: Option<AlbumKeyArg>,
        #[arg(long, value_enum, default_value_t = FormatArg::Raw565)]
        thumb_format: FormatArg,
        /// Ordered (Bayer) dither when snapping covers to RGB565.
        #[arg(long)]
        dither: bool,
    },
    /// Copy opentone binaries to <card>/opentone/tools, so the card carries the
    /// tool to any computer. Without --from, copies this binary only.
    InstallTools {
        #[command(flatten)]
        card: CardArg,
        /// Folder with release binaries (opentone-linux-x86_64,
        /// opentone-macos-universal, opentone-windows-x86_64.exe…).
        #[arg(long)]
        from: Option<PathBuf>,
    },
    /// Probe one file and convert it the way sync would (debugging aid).
    #[command(hide = true)]
    Convert { src: PathBuf, dst: PathBuf },
    /// Write a synthetic library's index to a card for the firmware test bench
    /// (software/bench): seeded fake tracks whose files don't exist, plus the
    /// real audio files of SAMPLES as the album "opentone Bench Samples".
    #[command(hide = true)]
    BenchDataset {
        /// Mounted bench card.
        card: PathBuf,
        /// Folder of audio files to copy to the card and index.
        #[arg(long)]
        samples: Option<PathBuf>,
        #[arg(long, default_value_t = 20_000)]
        tracks: u32,
        #[arg(long, default_value_t = 1)]
        seed: u64,
        /// Give every album a generated cover instead of the placeholder.
        #[arg(long)]
        covers: bool,
        /// Overwrite the index of a card that holds a synced library.
        #[arg(long)]
        force: bool,
    },
}

#[derive(Clone, Copy, ValueEnum)]
pub enum AlbumKeyArg {
    /// Album name only (legacy TS indexer behaviour).
    Name,
    /// Album artist + album name.
    ArtistAlbum,
}

impl From<AlbumKeyArg> for AlbumKey {
    fn from(a: AlbumKeyArg) -> Self {
        match a {
            AlbumKeyArg::Name => AlbumKey::Name,
            AlbumKeyArg::ArtistAlbum => AlbumKey::ArtistAlbum,
        }
    }
}

#[derive(Clone, Copy, ValueEnum)]
enum FormatArg {
    Raw565,
    Qoi,
}

static STOP: AtomicBool = AtomicBool::new(false);

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let cfg = config::Config::load()?;
    match cli.command {
        Command::Sync { lib, jobs, album_key } => {
            let album_key = album_key.map_or(cfg.0.album_key(), Into::into);
            cmd_sync(&cfg, lib, jobs, album_key)
        }
        Command::Status { lib, verbose } => cmd_status(&cfg, lib, verbose),
        Command::Ls { card, artist, album, tracks } => {
            cmd_ls(&cfg.card(card.card)?, Filter { artist, album, ..Default::default() }, tracks)
        }
        Command::Rm { card, artist, album, path, yes } => {
            let machine = cfg.machine(None);
            cmd_rm(&cfg.card(card.card)?, Filter { artist, album, path, keys: None }, yes, cfg.0.album_key(), &machine)
        }
        Command::Adopt { card, force } => cmd_adopt(&cfg.card(card.card)?, force),
        Command::Index { music, out, card, album_key, thumb_format, dither } => {
            let album_key = album_key.map_or(cfg.0.album_key(), Into::into);
            let thumbs = thumbs::Config {
                format: match thumb_format {
                    FormatArg::Raw565 => thumbs::Format::Raw565,
                    FormatArg::Qoi => thumbs::Format::Qoi,
                },
                dither,
            };
            match (music, out) {
                (Some(music), Some(out)) => cmd_index_folder(&music, &out, index::Options { album_key, thumbs }),
                (None, None) => cmd_index_card(&cfg.card(card.card)?, album_key),
                _ => bail!("give both MUSIC and OUT (folder mode), or neither (rebuild from the card's manifest)"),
            }
        }
        Command::InstallTools { card, from } => cmd_install_tools(&cfg.card(card.card)?, from.as_deref()),
        Command::Convert { src, dst } => convert_one(&src, &dst),
        Command::BenchDataset { card, samples, tracks, seed, covers, force } => {
            cmd_bench_dataset(&card, samples.as_deref(), tracks, seed, covers, force)
        }
    }
}

fn check_card(card: &Path) -> anyhow::Result<()> {
    if !card.is_dir() {
        bail!("card not found at {} (is it mounted?)", card.display());
    }
    // An empty mount point: nothing is mounted there.
    if std::fs::read_dir(card).is_ok_and(|mut d| d.next().is_none()) {
        bail!("{} is empty: is the card plugged in and mounted?", card.display());
    }
    opentone_core::card::check_writable(card).with_context(|| {
        format!(
            "no write permission on {} — remount with your user id:\n  sudo mount -o remount,uid=$(id -u),gid=$(id -g) {}",
            card.display(),
            card.display()
        )
    })?;
    Ok(())
}

/// Elapsed time, and speed and ETA over the last [`Rate::WINDOW`] rather
/// than since the start: files still cached from an interrupted run, or
/// writes the OS has buffered, would otherwise skew both.
struct Rate {
    start: Instant,
    samples: std::sync::Mutex<std::collections::VecDeque<(Instant, u64)>>,
}

impl Rate {
    const WINDOW: Duration = Duration::from_secs(10);

    fn new() -> Self {
        Self { start: Instant::now(), samples: Default::default() }
    }

    /// (units per second, time left), once there are 2 s of samples.
    fn update(&self, done: u64, total: u64) -> Option<(f64, Duration)> {
        let now = Instant::now();
        let mut w = self.samples.lock().unwrap();
        w.push_back((now, done));
        while w.len() > 2 && now.duration_since(w[1].0) > Self::WINDOW {
            w.pop_front();
        }
        let (t0, d0) = w[0];
        let secs = now.duration_since(t0).as_secs_f64();
        (secs > 2.0 && done > d0).then(|| {
            let rate = (done - d0) as f64 / secs;
            (rate, Duration::from_secs_f64(total.saturating_sub(done) as f64 / rate))
        })
    }

    /// "1:05 elapsed, ~3:10 left".
    fn times(&self, eta: Option<Duration>) -> String {
        let left = eta.map(|d| format!(", ~{} left", clock(d))).unwrap_or_default();
        format!("{} elapsed{left}", clock(self.start.elapsed()))
    }
}

/// 0:42, 3:05, 1:02:09.
fn clock(d: Duration) -> String {
    let s = d.as_secs();
    if s >= 3600 { format!("{}:{:02}:{:02}", s / 3600, s / 60 % 60, s % 60) } else { format!("{}:{:02}", s / 60, s % 60) }
}

/// Reads the card's existing files into a manifest, with a progress bar.
fn adopt_card(card: &Path) -> anyhow::Result<manifest::Manifest> {
    let bar = ProgressBar::new(0)
        .with_style(ProgressStyle::with_template("Reading the card: {bar:30} {msg}").unwrap());
    let rate = Rate::new();
    let m = library::adopt(card, &|p| {
        let eta = rate.update(p.cost_done, p.cost).map(|(_, eta)| eta);
        bar.set_length(p.cost);
        bar.set_position(p.cost_done);
        bar.set_message(format!("{}/{} files, {}", p.files_done, p.files, rate.times(eta)));
    })?;
    bar.finish_and_clear();
    println!("Read {} tracks on the card in {}.", m.tracks.len(), clock(rate.start.elapsed()));
    Ok(m)
}

fn load_manifest(card: &Path) -> anyhow::Result<manifest::Manifest> {
    let m = if !manifest::exists(card) {
        println!("No library.json on the card yet: reading the tracks already on it (once).");
        let m = adopt_card(card)?;
        manifest::save(card, &m)?;
        m
    } else {
        manifest::load(card)?
    };
    Ok(m)
}

struct Resolved {
    include_removed: bool,
    card: PathBuf,
    libraries: Vec<PathBuf>,
    excludes: Vec<String>,
    machine: String,
    podcasts: Option<PathBuf>,
}

fn resolve(cfg: &config::Config, lib: LibraryArgs) -> anyhow::Result<Resolved> {
    let card = cfg.card(lib.card.card)?;
    let libraries = cfg.libraries(lib.libraries)?;
    for l in &libraries {
        if !l.is_dir() {
            bail!("library not found: {}", l.display());
        }
    }
    let podcasts = cfg.podcasts(lib.podcasts);
    let mut excludes = cfg.excludes(lib.exclude);
    if let Some(p) = &podcasts {
        if !p.is_dir() {
            bail!("podcasts folder not found: {}", p.display());
        }
        excludes.extend(podcasts::library_excludes(&libraries, p));
    }
    Ok(Resolved {
        include_removed: lib.include_removed,
        card,
        libraries,
        excludes,
        machine: cfg.machine(lib.machine),
        podcasts,
    })
}

/// The podcast mirror's plan, when a podcasts folder is set.
fn make_podcast_plan(r: &Resolved, m: &podcasts::PodcastManifest) -> anyhow::Result<Option<mirror::MirrorPlan>> {
    let Some(root) = &r.podcasts else { return Ok(None) };
    let spinner = ProgressBar::new_spinner().with_message(format!("Comparing {} with the card's podcasts…", root.display()));
    spinner.enable_steady_tick(Duration::from_millis(120));
    let opts = mirror::Options { machine: r.machine.clone(), have_ffmpeg: convert::ffmpeg_available() };
    let plan = mirror::plan(m, root, &opts)?;
    spinner.finish_and_clear();
    Ok(Some(plan))
}

fn print_podcast_plan(plan: &mirror::MirrorPlan, verbose: bool) {
    let unchanged = plan.links.len() - plan.moves();
    println!(
        "  Podcasts: {} new episodes, {} moved, {} to delete, {} unchanged",
        plan.copies.len(),
        plan.moves(),
        plan.deletes.len(),
        unchanged
    );
    let limit = if verbose { usize::MAX } else { 10 };
    for c in plan.copies.iter().take(limit) {
        println!("    + {}", c.dest);
    }
    for (from, to) in plan.links.iter().filter_map(|l| l.moved.as_ref()).take(limit) {
        println!("    > {from} -> {to}");
    }
    for p in plan.deletes.iter().take(limit) {
        println!("    - {p}");
    }
    for (p, why) in plan.unsupported.iter().chain(&plan.unreadable).take(limit) {
        println!("    skipped {} — {why}", p.display());
    }
}

fn human(bytes: u64) -> String {
    let b = bytes as f64;
    if b >= 1e9 { format!("{:.1} GB", b / 1e9) } else { format!("{:.0} MB", b / 1e6) }
}

fn make_plan(r: &Resolved, m: &manifest::Manifest) -> anyhow::Result<sync::Plan> {
    let start = Instant::now();
    let files = sync::scan(&r.libraries, &r.excludes)?;
    let spinner = ProgressBar::new_spinner().with_message(format!("Comparing {} library files with the card…", files.len()));
    spinner.enable_steady_tick(Duration::from_millis(120));
    let opts = PlanOptions {
        machine: r.machine.clone(),
        have_ffmpeg: convert::ffmpeg_available(),
        card: r.card.clone(),
        include_removed: r.include_removed,
    };
    let plan = sync::plan(m, files, &opts);
    spinner.finish_and_clear();
    println!("Scanned in {:.1}s.", start.elapsed().as_secs_f64());
    Ok(plan)
}

fn print_plan(plan: &sync::Plan, verbose: bool) {
    let count = |r: WriteReason| plan.writes.iter().filter(|w| w.reason == r).count();
    let converts = plan.writes.iter().filter(|w| w.policy != policy::Policy::Copy).count();
    println!(
        "  {} new, {} upgrades, {} changed in the library ({} to convert)\n  {} already on the card (same tags; this file is now linked to it), {} unchanged",
        count(WriteReason::New),
        count(WriteReason::Upgrade),
        count(WriteReason::SourceChanged),
        converts,
        plan.attach.len(),
        plan.unchanged
    );
    if count(WriteReason::Fix) > 0 {
        println!("  {} files already on the card will be converted so the player can play them", count(WriteReason::Fix));
    }
    if plan.removed > 0 {
        println!("  {} skipped: removed with `rm` (use --include-removed to sync them again)", plan.removed);
    }
    if !plan.unsupported.is_empty() || !plan.unreadable.is_empty() {
        println!("  {} skipped:", plan.unsupported.len() + plan.unreadable.len());
        for (p, why) in plan.unsupported.iter().chain(&plan.unreadable).take(if verbose { usize::MAX } else { 10 }) {
            println!("    {} — {why}", p.display());
        }
    }
    let mut albums: BTreeMap<String, usize> = BTreeMap::new();
    for w in &plan.writes {
        let t = &w.probed.tags;
        let owner = t.album_artist.as_deref().or(t.artist.as_deref()).unwrap_or("Unknown Artist");
        *albums.entry(format!("{owner} — {}", t.album.as_deref().unwrap_or("Unknown Album"))).or_default() += 1;
    }
    if verbose {
        for w in &plan.writes {
            println!("    {:?}: {} ({})", w.reason, w.source.abs.display(), convert::describe(&w.policy, &w.probed.info));
        }
    } else if !albums.is_empty() {
        println!("  Albums to write:");
        for (album, n) in albums.iter().take(25) {
            println!("    {album} ({n})");
        }
        if albums.len() > 25 {
            println!("    … and {} more", albums.len() - 25);
        }
    }
}

fn space_check(card: &Path, needed: u64) -> anyhow::Result<()> {
    let free = fs4::available_space(card).unwrap_or(u64::MAX);
    println!("  ~{} to write, {} free on the card", human(needed), human(free));
    if needed > free {
        bail!("not enough space on the card");
    }
    Ok(())
}

fn cmd_status(cfg: &config::Config, lib: LibraryArgs, verbose: bool) -> anyhow::Result<()> {
    let r = resolve(cfg, lib)?;
    check_card(&r.card)?;
    let m = load_manifest(&r.card)?;
    let plan = make_plan(&r, &m)?;
    print_plan(&plan, verbose);
    let pm = podcasts::load(&r.card)?;
    let pplan = make_podcast_plan(&r, &pm)?;
    if let Some(p) = &pplan {
        print_podcast_plan(p, verbose);
    }
    let free = fs4::available_space(&r.card).unwrap_or(0);
    let needed = plan.bytes_needed() + pplan.as_ref().map_or(0, |p| p.bytes_needed());
    println!("  ~{} to write, {} free on the card", human(needed), human(free));
    Ok(())
}

struct Bar {
    bar: ProgressBar,
    rate: Rate,
    label: std::sync::Mutex<String>,
}

impl Bar {
    fn new(bar: ProgressBar) -> Self {
        Self { bar, rate: Rate::new(), label: Default::default() }
    }
}

impl sync::Progress for Bar {
    fn start(&self, files: usize, bytes: u64) {
        self.bar.set_length(bytes);
        self.bar.set_message(format!("{files} files"));
    }
    fn advance(&self, bytes: u64, label: &str) {
        self.bar.inc(bytes);
        let mut l = self.label.lock().unwrap();
        if !label.is_empty() {
            *l = label.to_owned();
        }
        let (pos, len) = (self.bar.position(), self.bar.length().unwrap_or(0));
        let r = self.rate.update(pos, len);
        let speed = r.map(|(bps, _)| format!("{}/s, ", indicatif::HumanBytes(bps as u64))).unwrap_or_default();
        self.bar.set_message(format!("{speed}{}  {l}", self.rate.times(r.map(|(_, eta)| eta))));
    }
    fn warn(&self, msg: &str) {
        self.bar.println(format!("warning: {msg}"));
    }
}

fn cmd_sync(cfg: &config::Config, lib: LibraryArgs, jobs: Option<usize>, album_key: AlbumKey) -> anyhow::Result<()> {
    let r = resolve(cfg, lib)?;
    check_card(&r.card)?;
    println!("Syncing {} -> {} as '{}'", r.libraries.iter().map(|l| l.display().to_string()).collect::<Vec<_>>().join(", "), r.card.display(), r.machine);
    let leftovers = sync::remove_partial_files(&r.card);
    if leftovers > 0 {
        println!("Removed {leftovers} partial file(s) from an interrupted sync.");
    }
    let mut m = load_manifest(&r.card)?;
    let plan = make_plan(&r, &m)?;
    print_plan(&plan, false);
    let mut pm = podcasts::load(&r.card)?;
    let pplan = make_podcast_plan(&r, &pm)?;
    if let Some(p) = &pplan {
        print_podcast_plan(p, false);
    }
    space_check(&r.card, plan.bytes_needed() + pplan.as_ref().map_or(0, |p| p.bytes_needed()))?;
    let info = PlanInfo::from(&plan);

    ctrlc::set_handler(|| {
        if STOP.swap(true, Ordering::SeqCst) {
            std::process::exit(130);
        }
        eprintln!("\nStopping after the current files (Ctrl-C again to quit now)…");
    })?;

    let jobs = jobs.unwrap_or_else(|| std::thread::available_parallelism().map_or(4, |n| n.get()));
    let bar = ProgressBar::new(0).with_style(
        ProgressStyle::with_template("{bar:30} {bytes}/{total_bytes} {wide_msg}")
            .unwrap(),
    );
    let start = Instant::now();
    let out = sync::execute(&r.card, &mut m, plan, jobs, &STOP, &Bar::new(bar.clone()))?;
    bar.finish_and_clear();
    println!(
        "Wrote {} file(s) ({} converted) in {:.0}s{}.",
        out.written.len(),
        out.converted.len(),
        start.elapsed().as_secs_f64(),
        if out.failed.is_empty() { String::new() } else { format!(", {} failed", out.failed.len()) }
    );

    let done = library::finish_sync(&r.card, &mut m, &r.machine, album_key, &info, &out, start.elapsed().as_secs())?;
    if done.covers > 0 {
        println!("Re-copied {} missing or damaged cover image(s).", done.covers);
    }
    match &done.index {
        Some(s) => {
            for w in &s.warnings {
                println!("warning: {w}");
            }
            println!("Index: {} tracks, {} artists, {} albums, {} genres.", s.tracks, s.artists, s.albums, s.genres);
        }
        None => println!("Index unchanged."),
    }
    if let Some(pplan) = pplan {
        if STOP.load(Ordering::Relaxed) {
            println!("Podcasts skipped: interrupted.");
        } else {
            let bar = ProgressBar::new(0).with_style(ProgressStyle::with_template("{bar:30} {bytes}/{total_bytes} {wide_msg}").unwrap());
            let pout = mirror::execute(&r.card, &mut pm, &pplan, &r.machine, jobs, &STOP, &Bar::new(bar.clone()))?;
            bar.finish_and_clear();
            let (report, index) = podcasts::finish(&r.card, &mut pm, &pplan, &pout)?;
            println!(
                "Podcasts: copied {}, moved {}, deleted {}{}.",
                report.copied,
                report.moved,
                report.deleted,
                if report.failed.is_empty() { String::new() } else { format!(", {} failed", report.failed.len()) }
            );
            if let Some(s) = index {
                for w in &s.warnings {
                    println!("warning: {w}");
                }
                println!("Podcast index: {} episodes in {} folders, listening state kept for {}.", s.episodes, s.folders, s.kept_state);
            }
        }
    }
    println!("Report: {}", done.text_report.display());
    if out.interrupted || STOP.load(Ordering::Relaxed) {
        println!("Interrupted: run sync again to copy the rest.");
    }
    println!("Eject the card (or run the `sync` shell command) before unplugging it.");
    Ok(())
}

fn cmd_ls(card: &Path, filter: Filter, tracks: bool) -> anyhow::Result<()> {
    let m = manifest::load(card).context("no library.json on the card (run `opentone adopt` or `opentone sync`)")?;
    let selected: Vec<_> = m.tracks.iter().filter(|e| filter.matches(e)).collect();
    if tracks {
        for e in &selected {
            println!("{}", e.path);
        }
    } else {
        let mut albums: BTreeMap<(String, String), (usize, u64, Vec<String>)> = BTreeMap::new();
        for e in &selected {
            let t = &e.tags;
            let owner = t.album_artist.clone().or(t.artist.clone()).unwrap_or_default();
            let a = albums.entry((owner, t.album.clone().unwrap_or_default())).or_default();
            a.0 += 1;
            a.1 += e.size;
            for s in &e.sources {
                if !a.2.contains(&s.machine) {
                    a.2.push(s.machine.clone());
                }
            }
        }
        for ((artist, album), (n, size, machines)) in &albums {
            let from = if machines.is_empty() { "adopted".into() } else { machines.join(", ") };
            println!("{artist} — {album}  ({n} tracks, {}, from {from})", human(*size));
        }
        println!("{} albums, {} tracks", albums.len(), selected.len());
    }
    Ok(())
}

fn confirm(question: &str) -> bool {
    print!("{question} [y/N] ");
    let _ = std::io::stdout().flush();
    let mut line = String::new();
    std::io::stdin().lock().read_line(&mut line).is_ok() && matches!(line.trim(), "y" | "Y" | "yes")
}

fn cmd_rm(card: &Path, filter: Filter, yes: bool, album_key: AlbumKey, machine: &str) -> anyhow::Result<()> {
    if filter.is_empty() {
        bail!("say what to remove: --artist, --album and/or --path");
    }
    check_card(card)?;
    let mut m = manifest::load(card).context("no library.json on the card")?;
    let n = m.tracks.iter().filter(|e| filter.matches(e)).count();
    if n == 0 {
        println!("Nothing matches.");
        return Ok(());
    }
    cmd_ls(card, filter.clone(), false)?;
    if !yes && !confirm(&format!("Remove these {n} tracks from the card?")) {
        println!("Cancelled.");
        return Ok(());
    }
    let removed = library::remove(card, &mut m, &filter, machine)?;
    let s = library::reindex(card, &m, album_key)?;
    println!("Removed {} tracks. Index: {} tracks, {} albums.", removed.len(), s.tracks, s.albums);
    println!("Future syncs skip them; `opentone sync --include-removed` brings them back.");
    Ok(())
}

fn cmd_adopt(card: &Path, force: bool) -> anyhow::Result<()> {
    check_card(card)?;
    if manifest::exists(card) && !force {
        bail!("{} already exists (use --force to rebuild it from the card)", manifest::path(card).display());
    }
    let m = adopt_card(card)?;
    manifest::save(card, &m)?;
    let weak = m.tracks.iter().filter(|e| e.tag_quality != tags::TagQuality::Tagged).count();
    println!(
        "{} with incomplete tags. Saved {}.",
        weak,
        manifest::path(card).display()
    );
    Ok(())
}

fn cmd_index_card(card: &Path, album_key: AlbumKey) -> anyhow::Result<()> {
    check_card(card)?;
    let m = load_manifest(card)?;
    let start = Instant::now();
    let s = library::reindex(card, &m, album_key)?;
    for w in &s.warnings {
        println!("warning: {w}");
    }
    println!(
        "{} tracks, {} artists, {} albums ({} without cover), {} genres in {:.1}s",
        s.tracks,
        s.artists,
        s.albums,
        s.placeholders,
        s.genres,
        start.elapsed().as_secs_f64()
    );
    if podcasts::path(card).is_file() {
        let mut pm = podcasts::load(card)?;
        let s = podcasts::reindex(card, &mut pm)?;
        println!("Podcasts: {} episodes in {} folders, listening state kept for {}.", s.episodes, s.folders, s.kept_state);
    }
    Ok(())
}

fn cmd_index_folder(music: &Path, out: &Path, opts: index::Options) -> anyhow::Result<()> {
    if !music.is_dir() {
        bail!("music folder not found: {}", music.display());
    }
    if !out.is_dir() {
        bail!("output folder not found: {}", out.display());
    }
    let start = Instant::now();
    let s = index::index_folder(music, out, opts).context("indexing failed")?;
    for w in &s.warnings {
        println!("warning: {w}");
    }
    println!(
        "{} tracks, {} artists, {} albums ({} without cover), {} genres in {:.1}s",
        s.tracks,
        s.artists,
        s.albums,
        s.placeholders,
        s.genres,
        start.elapsed().as_secs_f64()
    );
    Ok(())
}

/// Card folder the bench samples are copied to.
const BENCH_SAMPLES_DIR: &str = "opentone Bench/Samples";

fn cmd_bench_dataset(card: &Path, samples: Option<&Path>, tracks: u32, seed: u64, covers: bool, force: bool) -> anyhow::Result<()> {
    use opentone_core::index::synthetic;
    check_card(card)?;
    let out = card.join("opentone");
    if out.join(manifest::FILE).exists() && !force {
        bail!("{} holds a synced library: the bench would replace its index (use a spare card, or --force)", card.display());
    }
    std::fs::create_dir_all(&out)?;
    let start = Instant::now();

    let mut records = synthetic::records(synthetic::Shape::for_tracks(tracks), seed);
    let fake_count = records.len();
    let mut sample_count = 0;
    if let Some(dir) = samples {
        let dest = card.join(BENCH_SAMPLES_DIR);
        std::fs::create_dir_all(&dest)?;
        let mut names: Vec<PathBuf> = std::fs::read_dir(dir)
            .with_context(|| format!("reading {}", dir.display()))?
            .filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| p.is_file())
            .collect();
        names.sort();
        let mut copied = Vec::new();
        for src in &names {
            let dst = dest.join(src.file_name().unwrap());
            let same = std::fs::metadata(&dst).is_ok_and(|d| std::fs::metadata(src).is_ok_and(|s| s.len() == d.len()));
            if !same {
                std::fs::copy(src, &dst).with_context(|| format!("copying {}", src.display()))?;
            }
            copied.push(dst);
        }
        for (i, mut r) in index::crawl::read_records(card, &copied).into_iter().enumerate() {
            let stem = Path::new(&r.file_name).file_stem().map(|s| s.to_string_lossy().into_owned()).unwrap_or_default();
            r.title = Some(stem);
            r.artist = Some("opentone Bench".into());
            r.album_artist = Some("opentone Bench".into());
            r.album = Some("opentone Bench Samples".into());
            r.genre = Some("Bench".into());
            r.track_no = i as u32 + 1;
            records.push(r);
        }
        sample_count = copied.len();
    }

    let opts = index::Options { album_key: AlbumKey::ArtistAlbum, thumbs: thumbs::Config::default() };
    let s = index::write_index(&records, &out, opts, |i| {
        // Album art is asked once per album, for one of its tracks: key the
        // cover on the album's folder so it doesn't depend on which one.
        (covers && i < fake_count).then(|| {
            let dir = Path::new(&records[i].card_path).parent().map(|p| p.to_string_lossy().into_owned()).unwrap_or_default();
            synthetic::cover_png(xxhash_rust::xxh3::xxh3_64(dir.as_bytes()))
        })
    })
    .context("indexing failed")?;

    // Read back by the firmware (#dataset, 255 bytes max) and the runner.
    let info = format!(
        "{{\"generator\":{},\"seed\":{seed},\"tracks\":{},\"albums\":{},\"artists\":{},\"genres\":{},\"samples\":{sample_count},\"covers\":{covers}}}",
        synthetic::GENERATOR_VERSION, s.tracks, s.albums, s.artists, s.genres
    );
    std::fs::write(out.join("bench.json"), format!("{info}\n"))?;
    println!("{info}");
    println!("indexed in {:.1}s", start.elapsed().as_secs_f64());
    Ok(())
}

fn convert_one(src: &Path, dst: &Path) -> anyhow::Result<()> {
    let probed = tags::probe(src, src.file_name().map(Path::new).unwrap_or(src), true);
    let p = policy::decide(&probed.info, convert::ffmpeg_available());
    println!("{:?}\n{:?}\n-> {}", probed.info, probed.tags, convert::describe(&p, &probed.info));
    let start = Instant::now();
    convert::run(&p, src, dst).map_err(|e| anyhow::anyhow!("{e}"))?;
    println!("done in {:.2}s", start.elapsed().as_secs_f64());
    Ok(())
}

/// Release asset name for the running platform (matches syncer.yml).
fn own_binary_name() -> String {
    let os = match std::env::consts::OS {
        "macos" => "macos",
        "windows" => "windows",
        other => other,
    };
    let ext = if os == "windows" { ".exe" } else { "" };
    format!("opentone-{os}-{}{ext}", std::env::consts::ARCH)
}

fn cmd_install_tools(card: &Path, from: Option<&Path>) -> anyhow::Result<()> {
    check_card(card)?;
    let dir = card.join(manifest::DIR).join("tools");
    std::fs::create_dir_all(&dir)?;
    let mut copied = Vec::new();
    match from {
        Some(from) => {
            for e in std::fs::read_dir(from).with_context(|| format!("reading {}", from.display()))?.flatten() {
                let name = e.file_name().to_string_lossy().into_owned();
                if name.starts_with("opentone-") && e.path().is_file() {
                    std::fs::copy(e.path(), dir.join(&name))?;
                    copied.push(name);
                }
            }
            if copied.is_empty() {
                bail!("no opentone-* binaries in {}", from.display());
            }
        }
        None => {
            let name = own_binary_name();
            std::fs::copy(std::env::current_exe()?, dir.join(&name))?;
            copied.push(name);
        }
    }
    copied.sort();
    std::fs::write(
        dir.join("README.txt"),
        "opentone: sync music onto this card.\n\
         Copy the binary for your computer somewhere, make it executable (chmod +x on Linux/macOS), then:\n\
         \n  opentone sync ~/Music --card <where this card is mounted>\n\n\
         Releases: https://github.com/Leo-Nicolle/opentone/releases\n",
    )?;
    println!("Copied to {}: {}", dir.display(), copied.join(", "));
    Ok(())
}
