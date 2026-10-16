//! The podcast mirror on a card: copy, then the source changes (a show
//! folder renamed, an episode added, one deleted) and the listening state
//! the device wrote follows its episode to its new id.

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::AtomicBool;

use openpod_core::index::pack;
use openpod_core::podcasts::{self, mirror, state};
use openpod_core::sync::Progress;

struct Quiet;
impl Progress for Quiet {
    fn start(&self, _: usize, _: u64) {}
    fn advance(&self, _: u64, _: &str) {}
    fn warn(&self, msg: &str) {
        panic!("{msg}");
    }
}

fn tmp(name: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("openpod-test-{}-{name}", std::process::id()));
    let _ = fs::remove_dir_all(&d);
    fs::create_dir_all(&d).unwrap();
    d
}

/// 0.2 s of 16-bit mono 22.05 kHz sine (a different `freq` is a different
/// episode).
fn wav(path: &Path, freq: f64) {
    let rate = 22_050u32;
    let frames = rate / 5;
    let mut pcm = Vec::with_capacity(frames as usize * 2);
    for i in 0..frames {
        let v = ((i as f64 * freq * std::f64::consts::TAU / rate as f64).sin() * 8000.0) as i16;
        pcm.extend_from_slice(&v.to_le_bytes());
    }
    let mut d = Vec::new();
    d.extend_from_slice(b"RIFF");
    d.extend_from_slice(&(36 + pcm.len() as u32).to_le_bytes());
    d.extend_from_slice(b"WAVEfmt ");
    d.extend_from_slice(&16u32.to_le_bytes());
    d.extend_from_slice(&1u16.to_le_bytes());
    d.extend_from_slice(&1u16.to_le_bytes());
    d.extend_from_slice(&rate.to_le_bytes());
    d.extend_from_slice(&(rate * 2).to_le_bytes());
    d.extend_from_slice(&2u16.to_le_bytes());
    d.extend_from_slice(&16u16.to_le_bytes());
    d.extend_from_slice(b"data");
    d.extend_from_slice(&(pcm.len() as u32).to_le_bytes());
    d.extend_from_slice(&pcm);
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(path, d).unwrap();
}

fn sync(card: &Path, src: &Path) -> (mirror::MirrorPlan, podcasts::Report) {
    let mut m = podcasts::load(card).unwrap();
    let opts = mirror::Options { machine: "pc".into(), have_ffmpeg: false };
    let plan = mirror::plan(&m, src, &opts).unwrap();
    let out = mirror::execute(card, &mut m, &plan, "pc", 2, &AtomicBool::new(false), &Quiet).unwrap();
    let (report, _) = podcasts::finish(card, &mut m, &plan, &out).unwrap();
    (plan, report)
}

fn read_state(card: &Path) -> (u32, Vec<state::Record>) {
    state::parse(&fs::read(card.join("openpod").join(state::FILE)).unwrap()).unwrap()
}

/// Card path of each episode id, from `podcasts.pack`.
fn episode_paths(card: &Path) -> Vec<String> {
    let file = fs::read(card.join("openpod").join(pack::PODCAST_FILE)).unwrap();
    let sections = pack::unpack(&file).unwrap();
    let b = sections.iter().find(|(id, _)| *id == pack::section::POD_EPISODE_PATHS).unwrap().1;
    let n = u32::from_le_bytes(b[0..4].try_into().unwrap()) as usize;
    let data = 8 + (n + 1) * 4;
    let off = |i: usize| u32::from_le_bytes(b[8 + i * 4..][..4].try_into().unwrap()) as usize;
    (0..n).map(|i| String::from_utf8(b[data + off(i)..data + off(i + 1) - 1].to_vec()).unwrap()).collect()
}

#[test]
fn state_follows_moved_episodes() {
    let src = tmp("pod-src");
    let card = tmp("pod-card");
    wav(&src.join("Radio/Show/1-06.04.2018-a.wav"), 440.0);
    wav(&src.join("Radio/Show/1-07.04.2018-b.wav"), 550.0);
    wav(&src.join("Radio/Other/c.wav"), 660.0);

    let (plan, report) = sync(&card, &src);
    assert_eq!(plan.copies.len(), 3);
    assert_eq!((report.copied, report.episodes), (3, 3));
    // WAV is converted to FLAC for the player.
    assert!(card.join("Podcasts/Radio/Show/1-06.04.2018-a.flac").is_file());
    let paths = episode_paths(&card);
    // Folders alphabetical (Other, Show), dated shows newest first.
    assert_eq!(paths, [
        "Podcasts/Radio/Other/c.flac",
        "Podcasts/Radio/Show/1-07.04.2018-b.flac",
        "Podcasts/Radio/Show/1-06.04.2018-a.flac",
    ]);
    let (generation, mut records) = read_state(&card);
    assert_eq!(records.len(), 3);
    assert!(records.iter().all(|r| r.is_blank()));
    let meta = fs::read(card.join("openpod/podcast_meta.bin")).unwrap();
    assert_eq!(&meta[..4], b"OPPI");
    assert_eq!(u32::from_le_bytes(meta[12..16].try_into().unwrap()), generation);

    // The device listens to "a" (id 2) and finishes "c" (id 0).
    records[2].position = 300;
    records[2].last_played = 1;
    records[0].flags = state::PLAYED;
    fs::write(card.join("openpod").join(state::FILE), state::encode(generation, &records)).unwrap();

    // Unchanged source: nothing to do, and the index isn't rebuilt.
    let (plan, report) = sync(&card, &src);
    assert!(plan.is_empty());
    assert_eq!(report.copied + report.moved + report.deleted, 0);
    assert_eq!(read_state(&card).0, generation);

    // "Show" renamed to "A Show", "c" deleted, "d" added.
    fs::rename(src.join("Radio/Show"), src.join("Radio/A Show")).unwrap();
    fs::remove_file(src.join("Radio/Other/c.wav")).unwrap();
    wav(&src.join("Radio/Other/d.wav"), 770.0);
    let (plan, report) = sync(&card, &src);
    assert_eq!((plan.copies.len(), plan.moves(), plan.deletes.len()), (1, 2, 1));
    assert_eq!((report.copied, report.moved, report.deleted), (1, 2, 1));
    assert!(!card.join("Podcasts/Radio/Show").exists());
    assert!(!card.join("Podcasts/Radio/Other/c.flac").exists());

    let paths = episode_paths(&card);
    assert_eq!(paths, [
        "Podcasts/Radio/A Show/1-07.04.2018-b.flac",
        "Podcasts/Radio/A Show/1-06.04.2018-a.flac",
        "Podcasts/Radio/Other/d.flac",
    ]);
    let (new_generation, records) = read_state(&card);
    assert_eq!(new_generation, generation + 1);
    assert!(records[0].is_blank());
    assert_eq!((records[1].position, records[1].last_played), (300, 1));
    assert!(records[2].is_blank());
    assert_eq!(report.listened, 1);

    // A damaged state file is set aside and replaced.
    fs::write(card.join("openpod").join(state::FILE), b"garbage").unwrap();
    let mut m = podcasts::load(&card).unwrap();
    podcasts::reindex(&card, &mut m).unwrap();
    assert!(card.join("openpod/podcast_state.bin.bad").is_file());
    assert_eq!(read_state(&card).1.len(), 3);

    let _ = fs::remove_dir_all(&src);
    let _ = fs::remove_dir_all(&card);
}
