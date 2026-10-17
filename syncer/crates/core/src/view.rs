//! What the app's screens show, built from a [`Plan`] or the manifest:
//! plain serialisable data, worded for people (no keys or policy names).

use std::collections::{BTreeMap, HashMap};
use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::audio::{AudioInfo, Codec};
use crate::manifest::{self, Entry, Manifest, RemovedAlbum};
use crate::podcasts::{PodcastManifest, show_key};
use crate::policy::Policy;
use crate::sync::{Plan, WriteReason};
use crate::tags::{TagQuality, Tags};

/// Card write speed assumed before any sync has been measured (a USB 2
/// card reader).
pub const DEFAULT_WRITE_SPEED: u64 = 18_000_000;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Skipped {
    pub path: String,
    pub reason: String,
}

/// One album in "What's new".
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PlanAlbum {
    pub id: String,
    pub title: String,
    pub artist: String,
    pub tracks: usize,
    pub bytes: u64,
    pub new: bool,
    pub upgrade: bool,
    pub converted: bool,
    pub tags_guessed: bool,
    /// A library file of the album, for its cover.
    pub cover: String,
}

/// "What's new": what a sync would do.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PlanSummary {
    /// Tracks to add (new, or changed in the library).
    pub tracks: usize,
    pub albums: usize,
    pub upgrades: usize,
    /// Files already on the card converted in place for the player.
    pub fixes: usize,
    pub conversions: usize,
    pub bytes: u64,
    pub free: u64,
    /// Bytes/s the estimate uses, and whether it was measured on this card.
    pub write_speed: u64,
    pub speed_measured: bool,
    pub est_seconds: u64,
    /// Library files whose track is already on the card (linked now).
    pub already_on_card: usize,
    /// Library files synced before and unchanged.
    pub unchanged: usize,
    /// Library files skipped because their album was removed.
    pub removed: usize,
    pub album_list: Vec<PlanAlbum>,
    pub skipped: Vec<Skipped>,
    pub unreadable: Vec<Skipped>,
    /// The podcast mirror, when a podcasts folder is set.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub podcasts: Option<PodcastPlanSummary>,
}

/// "What's new" for podcasts: the mirror's copies, moves and deletions.
#[derive(Debug, Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PodcastPlanSummary {
    pub new: usize,
    pub moved: usize,
    pub deleted: usize,
    /// Deletions past a show's keep setting (the rest are gone from the folder).
    pub leaving: usize,
    pub bytes: u64,
    /// Shows getting new episodes or losing some ("France Culture / Les chemins…").
    pub shows: Vec<PodcastPlanShow>,
    /// Source episodes not on the card: the new ones, and those past their
    /// show's keep setting (`held`), for the show page.
    pub episodes: Vec<PodcastSourceEpisode>,
    pub skipped: Vec<Skipped>,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PodcastPlanShow {
    /// [`crate::podcasts::show_key`].
    pub key: String,
    pub folder: String,
    pub episodes: usize,
    pub leaving: usize,
    pub bytes: u64,
    /// A source file of the folder, for its cover.
    pub cover: String,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PodcastSourceEpisode {
    pub show: String,
    pub title: String,
    /// Days since 1970-01-01.
    pub date: u32,
    pub duration: f64,
    pub bytes: u64,
    /// Past the show's keep setting: stays on the computer.
    pub held: bool,
}

impl PlanSummary {
    /// Adds the podcast mirror: its section, and its bytes to the total
    /// and the time estimate.
    pub fn with_podcasts(mut self, m: &PodcastManifest, plan: &crate::podcasts::mirror::MirrorPlan) -> Self {
        let p = podcast_plan_summary(m, plan);
        self.bytes += p.bytes;
        self.est_seconds = self.bytes / self.write_speed.max(1);
        self.podcasts = Some(p);
        self
    }
}

/// The podcast part of "What's new".
pub fn podcast_plan_summary(m: &PodcastManifest, plan: &crate::podcasts::mirror::MirrorPlan) -> PodcastPlanSummary {
    let mut shows: Vec<PodcastPlanShow> = Vec::new();
    let mut show = |folders: &[String], cover: &str| -> usize {
        let key = show_key(folders);
        shows.iter().position(|s| s.key == key).unwrap_or_else(|| {
            shows.push(PodcastPlanShow { folder: folders.join(" / "), key, episodes: 0, leaving: 0, bytes: 0, cover: cover.to_owned() });
            shows.len() - 1
        })
    };
    let mut indexes = Vec::new();
    for c in &plan.copies {
        indexes.push((show(&c.folders, &c.source.abs.to_string_lossy()), c.estimated_size));
    }
    let mut left = Vec::new();
    for p in &plan.leaving {
        if let Some(e) = m.episodes.iter().find(|e| &e.path == p) {
            left.push(show(&e.folders, &e.path));
        }
    }
    for (i, bytes) in indexes {
        shows[i].episodes += 1;
        shows[i].bytes += bytes;
    }
    for i in left {
        shows[i].leaving += 1;
    }
    shows.sort_by(|a, b| a.folder.to_lowercase().cmp(&b.folder.to_lowercase()));
    let skipped = plan
        .unsupported
        .iter()
        .chain(&plan.unreadable)
        .map(|(p, r)| Skipped { path: p.to_string_lossy().into_owned(), reason: r.clone() })
        .collect();
    let episodes = plan
        .copies
        .iter()
        .map(|c| PodcastSourceEpisode {
            show: show_key(&c.folders),
            title: c.title.clone(),
            date: c.date,
            duration: c.probed.info.duration,
            bytes: c.estimated_size,
            held: false,
        })
        .chain(plan.held.iter().map(|h| PodcastSourceEpisode {
            show: show_key(&h.folders),
            title: h.title.clone(),
            date: h.date,
            duration: h.duration,
            bytes: h.estimated_size,
            held: true,
        }))
        .collect();
    PodcastPlanSummary {
        new: plan.copies.len(),
        moved: plan.moves(),
        deleted: plan.deletes.len(),
        leaving: plan.leaving.len(),
        bytes: plan.bytes_needed(),
        shows,
        episodes,
        skipped,
    }
}

/// One episode on the card, in "On the card › Podcasts".
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardEpisode {
    pub title: String,
    /// Days since 1970-01-01, 0 when unknown.
    pub date: u32,
    pub duration: f64,
    pub bytes: u64,
    /// Card path.
    pub path: String,
    /// Where the player stopped, in seconds (0: not started).
    pub position: u32,
    pub played: bool,
}

/// One show (a folder holding episodes) on the card.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardShow {
    /// [`show_key`]: what [`crate::podcasts::set_keep`] takes.
    pub key: String,
    /// The folder's name.
    pub title: String,
    /// The folders above it ("France Culture"), empty at the top.
    pub publisher: String,
    /// The latest episodes kept on the card; None: all of them.
    pub keep: Option<u32>,
    /// Its folder on this computer; empty when it was mirrored from another.
    pub source: String,
    /// Computers it was mirrored from.
    pub machines: Vec<String>,
    pub bytes: u64,
    /// Card path of an episode, for the cover.
    pub cover: String,
    /// Newest first.
    pub episodes: Vec<CardEpisode>,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardPodcasts {
    pub shows: Vec<CardShow>,
    pub episodes: usize,
    pub bytes: u64,
    /// Listening state the player wrote: episodes played, and started
    /// but not finished.
    pub played: usize,
    pub started: usize,
    /// Whether `podcast_state.bin` could be read.
    pub state_read: bool,
}

/// The card's podcasts by show, with the listening state the player wrote
/// (`state`: the card's `podcast_state.bin`). `machine`: this computer, for
/// each show's source folder.
pub fn card_podcasts(m: &PodcastManifest, state: Option<&[u8]>, machine: &str) -> CardPodcasts {
    let parsed = state.and_then(crate::podcasts::state::parse);
    let state_read = parsed.is_some();
    let records: HashMap<u64, crate::podcasts::state::Record> =
        parsed.map(|(_, r)| r.into_iter().map(|r| (r.key, r)).collect()).unwrap_or_default();
    let mut shows: BTreeMap<String, CardShow> = BTreeMap::new();
    for e in &m.episodes {
        let key = show_key(&e.folders);
        let s = shows.entry(key.clone()).or_insert_with(|| CardShow {
            keep: m.keep.get(&key).copied(),
            key,
            title: e.folders.last().cloned().unwrap_or_else(|| "Podcasts".into()),
            publisher: e.folders[..e.folders.len().saturating_sub(1)].join(" / "),
            source: String::new(),
            machines: Vec::new(),
            bytes: 0,
            cover: e.path.clone(),
            episodes: Vec::new(),
        });
        for src in &e.sources {
            if !s.machines.contains(&src.machine) {
                s.machines.push(src.machine.clone());
            }
            if s.source.is_empty() && src.machine == machine {
                if let Some(dir) = Path::new(&src.path).parent() {
                    s.source = dir.to_string_lossy().into_owned();
                }
            }
        }
        let r = records.get(&e.key()).copied().unwrap_or_default();
        s.bytes += e.size;
        s.episodes.push(CardEpisode {
            title: e.title.clone(),
            date: e.date,
            duration: e.duration,
            bytes: e.size,
            path: e.path.clone(),
            position: r.position,
            played: r.flags & crate::podcasts::state::PLAYED != 0,
        });
    }
    let mut shows: Vec<CardShow> = shows.into_values().collect();
    for s in &mut shows {
        s.episodes.sort_by(|a, b| b.date.cmp(&a.date).then_with(|| b.path.cmp(&a.path)));
    }
    shows.sort_by(|a, b| a.title.to_lowercase().cmp(&b.title.to_lowercase()));
    let all = || shows.iter().flat_map(|s| &s.episodes);
    CardPodcasts {
        episodes: all().count(),
        bytes: all().map(|e| e.bytes).sum(),
        played: all().filter(|e| e.played).count(),
        started: all().filter(|e| !e.played && e.position > 0).count(),
        state_read,
        shows,
    }
}

fn owner(t: &Tags) -> String {
    t.album_artist.clone().or_else(|| t.artist.clone()).unwrap_or_else(|| "Unknown Artist".into())
}

fn album_title(t: &Tags) -> String {
    t.album.clone().unwrap_or_else(|| "Unknown Album".into())
}

pub fn plan_summary(plan: &Plan, m: &Manifest, free: u64) -> PlanSummary {
    let (write_speed, speed_measured) = match m.last_sync.as_ref().map(|s| s.write_speed).filter(|&s| s > 0) {
        Some(s) => (s, true),
        None => (DEFAULT_WRITE_SPEED, false),
    };
    let mut albums: Vec<PlanAlbum> = Vec::new();
    let mut index: HashMap<String, usize> = HashMap::new();
    for w in &plan.writes {
        if w.reason == WriteReason::Fix {
            continue;
        }
        let t = &w.probed.tags;
        let id = manifest::album_id(t);
        let i = *index.entry(id.clone()).or_insert_with(|| {
            albums.push(PlanAlbum {
                id,
                title: album_title(t),
                artist: owner(t),
                tracks: 0,
                bytes: 0,
                new: false,
                upgrade: false,
                converted: false,
                tags_guessed: false,
                cover: w.source.abs.to_string_lossy().into_owned(),
            });
            albums.len() - 1
        });
        let a = &mut albums[i];
        a.tracks += 1;
        a.bytes += w.estimated_size;
        a.new |= w.reason != WriteReason::Upgrade;
        a.upgrade |= w.reason == WriteReason::Upgrade;
        a.converted |= w.policy != Policy::Copy;
        a.tags_guessed |= w.probed.quality != TagQuality::Tagged;
    }
    albums.sort_by(|a, b| (a.artist.to_lowercase(), a.title.to_lowercase()).cmp(&(b.artist.to_lowercase(), b.title.to_lowercase())));
    let count = |f: &dyn Fn(&crate::sync::Write) -> bool| plan.writes.iter().filter(|w| f(w)).count();
    let bytes = plan.bytes_needed();
    let skipped = |v: &[(std::path::PathBuf, String)]| {
        v.iter().map(|(p, r)| Skipped { path: p.to_string_lossy().into_owned(), reason: r.clone() }).collect()
    };
    PlanSummary {
        tracks: count(&|w| matches!(w.reason, WriteReason::New | WriteReason::SourceChanged)),
        albums: albums.len(),
        upgrades: count(&|w| w.reason == WriteReason::Upgrade),
        fixes: count(&|w| w.reason == WriteReason::Fix),
        conversions: count(&|w| w.policy != Policy::Copy),
        bytes,
        free,
        write_speed,
        speed_measured,
        est_seconds: bytes / write_speed.max(1),
        already_on_card: plan.attach.len(),
        unchanged: plan.unchanged,
        removed: plan.removed,
        album_list: albums,
        skipped: skipped(&plan.unsupported),
        unreadable: skipped(&plan.unreadable),
        podcasts: None,
    }
}

/// "FLAC 44.1/16", "MP3 320k".
pub fn format_label(a: &AudioInfo) -> String {
    let khz = |r: u32| {
        let k = r as f64 / 1000.0;
        if k.fract() == 0.0 { format!("{k:.0}") } else { format!("{k:.1}") }
    };
    match a.codec {
        Codec::Mp3 => format!("MP3 {}k", a.bitrate),
        Codec::OtherLossy => format!("{}k", a.bitrate),
        c => format!("{} {}/{}", codec_name(c), khz(a.sample_rate), a.bits),
    }
}

fn codec_name(c: Codec) -> &'static str {
    match c {
        Codec::Mp3 => "MP3",
        Codec::Flac => "FLAC",
        Codec::Wav => "WAV",
        Codec::Aiff => "AIFF",
        Codec::Alac => "ALAC",
        Codec::OtherLossless => "WavPack/APE",
        Codec::OtherLossy => "AAC/Ogg/Opus",
    }
}

/// A [`crate::convert::describe`] string, parsed back.
struct Conversion {
    src: String,
    /// (rate, bits, channels) before and after, for FLAC outputs.
    formats: Option<((u32, u8, u8), (u32, u8, u8))>,
    to_mp3: bool,
    lossless_reencode: bool,
}

fn parse_conversion(desc: &str) -> Option<Conversion> {
    if desc.starts_with("lpc order") {
        return Some(Conversion { src: "flac".into(), formats: None, to_mp3: false, lossless_reencode: true });
    }
    let (from, to) = desc.split_once(" -> ")?;
    let mut f = from.split_whitespace();
    let src = f.next()?.to_owned();
    let rbc = |s: &str| -> Option<(u32, u8, u8)> {
        let mut it = s.split('/');
        Some((it.next()?.parse().ok()?, it.next()?.parse().ok()?, it.next()?.parse().ok()?))
    };
    if to.starts_with("mp3") {
        return Some(Conversion { src, formats: None, to_mp3: true, lossless_reencode: false });
    }
    let before = rbc(f.next()?)?;
    let after = rbc(to.split_whitespace().nth(1)?)?;
    Some(Conversion { src, formats: Some((before, after)), to_mp3: false, lossless_reencode: false })
}

fn src_label(src: &str) -> &'static str {
    match src {
        "flac" => "FLAC",
        "wav" => "WAV",
        "aiff" => "AIFF",
        "alac" => "ALAC",
        "otherlossless" => "WavPack/APE",
        "otherlossy" => "AAC/Ogg/Opus",
        _ => "Audio",
    }
}

/// How a track was converted, for its album page: "96 kHz/24 → 44.1 kHz/16",
/// "WAV → FLAC", "AAC/Ogg/Opus → MP3". None when it was copied.
pub fn conversion_detail(desc: &str) -> Option<String> {
    let c = parse_conversion(desc)?;
    let khz = |r: u32| {
        let k = r as f64 / 1000.0;
        if k.fract() == 0.0 { format!("{k:.0} kHz") } else { format!("{k:.1} kHz") }
    };
    Some(if c.lossless_reencode {
        "Re-encoded losslessly so the player can decode it".into()
    } else if c.to_mp3 {
        format!("{} → MP3", src_label(&c.src))
    } else {
        let ((r0, b0, ch0), (r1, b1, ch1)) = c.formats?;
        let mut s = if c.src == "flac" { String::new() } else { format!("{} → FLAC, ", src_label(&c.src)) };
        if (r0, b0) != (r1, b1) {
            s += &format!("{}/{} → {}/{}", khz(r0), b0, khz(r1), b1);
        }
        if ch0 != ch1 {
            if !s.is_empty() && !s.ends_with(", ") {
                s += ", ";
            }
            s += &format!("{ch0} channels → stereo");
        }
        s.trim_end_matches(", ").to_owned()
    })
}

/// Conversion family for the report's breakdown: "Hi-res FLAC → FLAC",
/// "WAV → FLAC", "AAC/Ogg/Opus → MP3"…
pub fn conversion_kind(desc: &str) -> Option<String> {
    let c = parse_conversion(desc)?;
    Some(if c.lossless_reencode {
        "FLAC → FLAC (lossless re-encode)".into()
    } else if c.to_mp3 {
        format!("{} → MP3", src_label(&c.src))
    } else if c.src == "flac" {
        let ((r, b, ch), _) = c.formats?;
        if ch > 2 && r <= 48_000 && b <= 24 { "Multichannel FLAC → FLAC".into() } else { "Hi-res FLAC → FLAC".into() }
    } else {
        format!("{} → FLAC", src_label(&c.src))
    })
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardTrack {
    pub disc: u32,
    pub no: u32,
    pub title: String,
    pub duration: f64,
    pub format: String,
    /// Card path.
    pub path: String,
}

/// One album in "On the card".
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CardAlbum {
    pub id: String,
    pub title: String,
    pub artist: String,
    pub bytes: u64,
    pub duration: f64,
    /// "FLAC 44.1/16" when every track shares it, else "FLAC, MP3".
    pub format: String,
    /// Computers it was synced from; empty: it was on the card already.
    pub machines: Vec<String>,
    /// How its tracks were converted (distinct details), empty if copied.
    pub conversions: Vec<String>,
    pub tags_guessed: bool,
    /// Unix seconds (latest track).
    pub added_at: u64,
    /// Card path of a track, for the cover.
    pub cover: String,
    pub tracks: Vec<CardTrack>,
}

pub fn card_albums(m: &Manifest) -> Vec<CardAlbum> {
    let mut groups: BTreeMap<String, Vec<&Entry>> = BTreeMap::new();
    for e in &m.tracks {
        groups.entry(manifest::album_id(&e.tags)).or_default().push(e);
    }
    let mut out: Vec<CardAlbum> = groups
        .into_iter()
        .map(|(id, mut entries)| {
            entries.sort_by(|a, b| (a.tags.disc, a.tags.track, &a.path).cmp(&(b.tags.disc, b.tags.track, &b.path)));
            let first = entries[0];
            let mut machines: Vec<String> = Vec::new();
            let mut conversions: Vec<String> = Vec::new();
            let mut formats: Vec<String> = Vec::new();
            for e in &entries {
                for s in &e.sources {
                    if !machines.contains(&s.machine) {
                        machines.push(s.machine.clone());
                    }
                }
                if let Some(c) = conversion_detail(&e.conversion) {
                    if !conversions.contains(&c) {
                        conversions.push(c);
                    }
                }
                let f = format_label(&e.audio);
                if !formats.contains(&f) {
                    formats.push(f);
                }
            }
            let format = if formats.len() == 1 {
                formats.remove(0)
            } else {
                let mut codecs: Vec<&str> = formats.iter().filter_map(|f| f.split_whitespace().next()).collect();
                codecs.dedup();
                codecs.join(", ")
            };
            CardAlbum {
                id,
                title: album_title(&first.tags),
                artist: owner(&first.tags),
                bytes: entries.iter().map(|e| e.size).sum(),
                duration: entries.iter().map(|e| e.audio.duration).sum(),
                format,
                machines,
                conversions,
                tags_guessed: entries.iter().any(|e| e.tag_quality != TagQuality::Tagged),
                added_at: entries.iter().map(|e| e.added_at).max().unwrap_or(0),
                cover: first.path.clone(),
                tracks: entries
                    .iter()
                    .map(|e| CardTrack {
                        disc: e.tags.disc,
                        no: e.tags.track,
                        title: e.tags.title.clone().unwrap_or_else(|| {
                            Path::new(&e.path).file_stem().map_or_else(String::new, |s| s.to_string_lossy().into_owned())
                        }),
                        duration: e.audio.duration,
                        format: format_label(&e.audio),
                        path: e.path.clone(),
                    })
                    .collect(),
            }
        })
        .collect();
    out.sort_by(|a, b| (a.artist.to_lowercase(), a.title.to_lowercase()).cmp(&(b.artist.to_lowercase(), b.title.to_lowercase())));
    out
}

/// One entry of the "Removed" list.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RemovedView {
    pub id: String,
    pub title: String,
    pub artist: String,
    pub tracks: usize,
    pub removed_at: u64,
    pub machine: String,
}

pub fn removed_albums(m: &Manifest) -> Vec<RemovedView> {
    m.removed_albums
        .iter()
        .map(|r: &RemovedAlbum| RemovedView {
            id: r.id.clone(),
            title: r.album.clone(),
            artist: r.artist.clone(),
            tracks: r.keys.len(),
            removed_at: r.removed_at,
            machine: r.machine.clone(),
        })
        .collect()
}

/// `opentone/last-sync.json`: what the last sync did, for the report screen.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SyncReport {
    pub machine: String,
    /// Unix seconds.
    pub at: u64,
    pub seconds: u64,
    pub interrupted: bool,
    pub written: usize,
    pub bytes_written: u64,
    pub upgraded: usize,
    /// (kind, count), most frequent first.
    pub conversions: Vec<(String, usize)>,
    pub converted: usize,
    pub skipped: Vec<Skipped>,
    pub failed: Vec<Skipped>,
    /// Card folders whose tags were guessed or are missing (whole card).
    pub tags_guessed: Vec<String>,
    pub warnings: Vec<String>,
    /// Card totals after the sync.
    pub card_tracks: usize,
    pub card_albums: usize,
    /// The podcast mirror, when a podcasts folder is set.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub podcasts: Option<crate::podcasts::Report>,
}

pub const REPORT_FILE: &str = "last-sync.json";

pub fn load_report(card: &Path) -> Option<SyncReport> {
    let data = std::fs::read(card.join(manifest::DIR).join(REPORT_FILE)).ok()?;
    serde_json::from_slice(&data).ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conversion_words() {
        assert_eq!(conversion_detail("copied"), None);
        assert_eq!(conversion_detail("flac 96000/24/2 -> flac 48000/16/2").unwrap(), "96 kHz/24 → 48 kHz/16");
        assert_eq!(conversion_detail("flac 88200/24/2 -> flac 44100/16/2").unwrap(), "88.2 kHz/24 → 44.1 kHz/16");
        assert_eq!(conversion_detail("wav 44100/16/2 -> flac 44100/16/2").unwrap(), "WAV → FLAC");
        assert_eq!(conversion_detail("otherlossy 256 kbps -> mp3 v0").unwrap(), "AAC/Ogg/Opus → MP3");
        assert_eq!(conversion_kind("flac 96000/24/2 -> flac 48000/16/2").unwrap(), "Hi-res FLAC → FLAC");
        assert_eq!(conversion_kind("lpc order 32 -> <= 12 (lossless re-encode)").unwrap(), "FLAC → FLAC (lossless re-encode)");
        assert_eq!(conversion_kind("wav 44100/16/2 -> flac 44100/16/2").unwrap(), "WAV → FLAC");
    }
}
