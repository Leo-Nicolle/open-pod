//! Two computers syncing into one card, with generated (untagged) WAVs laid
//! out as Artist/Album/NN Title.wav, so tags come from the folder layout.

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::AtomicBool;

use opentone_core::index::AlbumKey;
use opentone_core::library::{self, Filter};
use opentone_core::manifest::{self, Manifest};
use opentone_core::sync::{self, PlanOptions, Progress, WriteReason};

struct Quiet;
impl Progress for Quiet {
    fn start(&self, _: usize, _: u64) {}
    fn advance(&self, _: u64, _: &str) {}
    fn warn(&self, msg: &str) {
        panic!("{msg}");
    }
}

fn tmp(name: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("opentone-test-{}-{name}", std::process::id()));
    let _ = fs::remove_dir_all(&d);
    fs::create_dir_all(&d).unwrap();
    d
}

/// 0.2 s of 16-bit stereo 44.1 kHz sine.
fn wav(path: &Path, freq: f64) {
    let rate = 44_100u32;
    let frames = rate / 5;
    let mut pcm = Vec::with_capacity(frames as usize * 4);
    for i in 0..frames {
        let v = ((i as f64 * freq * std::f64::consts::TAU / rate as f64).sin() * 8000.0) as i16;
        pcm.extend_from_slice(&v.to_le_bytes());
        pcm.extend_from_slice(&v.to_le_bytes());
    }
    let mut d = Vec::new();
    d.extend_from_slice(b"RIFF");
    d.extend_from_slice(&(36 + pcm.len() as u32).to_le_bytes());
    d.extend_from_slice(b"WAVEfmt ");
    d.extend_from_slice(&16u32.to_le_bytes());
    d.extend_from_slice(&1u16.to_le_bytes()); // PCM
    d.extend_from_slice(&2u16.to_le_bytes());
    d.extend_from_slice(&rate.to_le_bytes());
    d.extend_from_slice(&(rate * 4).to_le_bytes());
    d.extend_from_slice(&4u16.to_le_bytes());
    d.extend_from_slice(&16u16.to_le_bytes());
    d.extend_from_slice(b"data");
    d.extend_from_slice(&(pcm.len() as u32).to_le_bytes());
    d.extend_from_slice(&pcm);
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(path, d).unwrap();
}

/// What a sync planned, after running it.
struct Ran {
    writes: Vec<WriteReason>,
    attached: usize,
    unchanged: usize,
    removed: usize,
}

fn run(card: &Path, m: &mut Manifest, lib: &Path, machine: &str) -> Ran {
    let files = sync::scan(&[lib.to_path_buf()], &[]).unwrap();
    let opts =
        PlanOptions { machine: machine.into(), have_ffmpeg: false, card: card.into(), include_removed: false };
    let plan = sync::plan(m, files, &opts);
    let ran = Ran {
        writes: plan.writes.iter().map(|w| w.reason).collect(),
        attached: plan.attach.len(),
        unchanged: plan.unchanged,
        removed: plan.removed,
    };
    sync::execute(card, m, plan, 2, &AtomicBool::new(false), &Quiet).unwrap();
    ran
}

#[test]
fn two_machines_additive() {
    let card = tmp("card");
    let lib_a = tmp("lib-a");
    let lib_b = tmp("lib-b");
    wav(&lib_a.join("Band/First Album/01 Opening.wav"), 440.0);
    wav(&lib_a.join("Band/First Album/02 Closing.wav"), 550.0);
    // Same recording on the other computer (different bytes), plus a new album.
    wav(&lib_b.join("Band/First Album/01 Opening.wav"), 441.0);
    wav(&lib_b.join("Other/Second Album/01 Hello.wav"), 660.0);

    let (mut m, _) = library::load_or_adopt(&card, &|_| {}).unwrap();
    assert!(m.tracks.is_empty());

    let ran = run(&card, &mut m, &lib_a, "a");
    assert_eq!(ran.writes, [WriteReason::New, WriteReason::New]);
    assert!(card.join("Band/First Album/01 Opening.flac").is_file(), "WAV becomes FLAC");

    let ran = run(&card, &mut m, &lib_b, "b");
    assert_eq!(ran.writes, [WriteReason::New], "only the new album is written");
    assert_eq!(ran.attached, 1, "the shared recording is attached, not copied");
    let opening = m.tracks.iter().find(|e| e.path.ends_with("01 Opening.flac")).unwrap();
    let machines: Vec<&str> = opening.sources.iter().map(|s| s.machine.as_str()).collect();
    assert_eq!(machines, ["a", "b"]);

    // Re-syncing is a no-op, and the manifest on the card matches.
    let ran = run(&card, &mut m, &lib_a, "a");
    assert!(ran.writes.is_empty());
    assert_eq!(ran.unchanged, 2);
    assert_eq!(manifest::load(&card).unwrap().tracks.len(), 3);

    // Removing an album sticks across syncs.
    let removed =
        library::remove(&card, &mut m, &Filter { album: Some("first album".into()), ..Default::default() }, "a").unwrap();
    assert_eq!(removed.len(), 2);
    assert_eq!(m.removed_albums.len(), 1, "the removal is recorded as one album");
    assert!(!card.join("Band").exists(), "empty folders go too");
    let ran = run(&card, &mut m, &lib_a, "a");
    assert!(ran.writes.is_empty());
    assert_eq!(ran.removed, 2);

    let s = library::reindex(&card, &m, AlbumKey::default()).unwrap();
    assert_eq!((s.tracks, s.albums), (1, 1));

    // Bringing it back: the next sync copies it again.
    let id = m.removed_albums[0].id.clone();
    assert!(library::bring_back(&card, &mut m, &id).unwrap());
    assert!(m.removed.is_empty() && m.removed_sources.is_empty() && m.removed_albums.is_empty());
    let ran = run(&card, &mut m, &lib_a, "a");
    assert_eq!(ran.writes, [WriteReason::New, WriteReason::New]);

    for d in [card, lib_a, lib_b] {
        let _ = fs::remove_dir_all(d);
    }
}
