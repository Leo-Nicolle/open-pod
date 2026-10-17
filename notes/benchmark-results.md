# Benchmark results: first full bench run

First full run of the firmware test bench (`software/bench/`, `docs/tools/bench.md`) on the
Nucleo prototype, 2026-10-16. It shows where the firmware has margin, where it is thin,
and what to look at next. Raw data:
- `benchmarks/2026-10-16-1ebb1f2-dirty.json`: the report.
- `benchmarks/2026-10-16-1ebb1f2-dirty.log`: every serial line, timestamped in ms since
  the port opened.

## Setup

| | |
|---|---|
| Firmware | `1ebb1f2-dirty`, `pio run -e bench` (develop + the bench changes) |
| Card | `opentone bench-dataset --tracks 6000`: 6,008 tracks, 460 albums, 242 artists, 41 genres, no covers, plus the 8 samples |
| Run | From the docs' live page (Web Serial, Chromium), all six scenarios, 7.7 min |
| Background | `05 flac 16-44` looping (`#repeat 1`) under *scroll*, *navigation* and *search* |
| Result | **0 underruns in every scenario**, no scenario errors |

20,000 tracks, the planned default, doesn't fit (see [Memory](#memory-and-boot)), hence 6,000.

## Headline numbers

| Metric | Value | Notes |
|---|---|---|
| Boot to ready | 5.6 s | library index 2.4 s + search index 2.0 s |
| Scroll one row, p50 / p95 | 47 / 63 ms | with the background FLAC; 41 ms p95 in silence (earlier run) |
| Fast-scroll tick (one page), p95 | 67 ms | |
| Open a list / go back, p95 | 865 / 862 ms | 800 ms of that is the slide animation |
| Search keystroke | 30–45 ms | per character, query + redraw |
| Scroll search results, p95 | 40 ms | 22 ms in silence |
| Track start to first second played | 305–317 ms MP3, 445–452 ms FLAC | idle |
| Seek call / position settled | 27–79 ms / 1.0–1.25 s | |
| Next / prev | 1–69 ms | |
| Underruns | 0 | everywhere, including 24-bit FLAC under scrolling stress |
| Free RAM at the end | 5.5 KB | |

## Findings, by priority

### 1. The audio margin at a track start under load is thin (but holds)

The worst case is a track that has just started (empty buffer) while the user works the UI.
*Start tracks under load* starts each sample, then for ~5.8 s goes back through three
screens, searches `the` and `a`, opens *Tracks* and fast-scrolls. Runway per second, from
the start (`scenarios.startup[].trace`):

| Sample | Rate | During the load | After the load | Underruns |
|---|---|---|---|---|
| mp3 64k / 128k | 8 / 16 KB/s | 2.5–2.7 s | 20–33 s within 1–2 s | 0 |
| mp3 320k / V0 | 40 / 23 KB/s | 2.2–2.5 s | 15–30 s within 1–2 s | 0 |
| FLAC 16/44, WAV→FLAC, 24/96→16/48 | 69–78 KB/s | 1.9–2.5 s | +6–7 s per second | 0 |
| **FLAC 24/48** | **154 KB/s** | **1.3–1.7 s** | **+2 s per second, 8.9 s at 10 s** | 0 |

What it means:
- **The plateau is the refill policy working as designed.** `software/src/sound/RefillPolicy.h`
  stops reading the SD card while the user is busy if the runway is above
  `URGENT_RUNWAY_MS` (2.5 s). Below that it trickles 1 chunk per 20 ms, or 2 below
  `BUSY_BOOST_RUNWAY_MS` (1.5 s). The buffer only fills once input stops for
  `USER_ACTIVITY_HOLDOFF_MS` (600 ms, `AudioPlayer.h`).
- **24-bit FLAC sits on the boost threshold**: the trickle only just matches 154 KB/s, so
  under sustained input the runway hovers around 1.5 s for as long as the user keeps going.
  Its lowest point was 1.3 s.
- 1.3 s of runway against a longest real main-loop block of 190 ms (finding 3) is about
  7× margin. It breaks if something blocks the loop for more than ~1.3 s, or if the SD
  card slows down while the user is busy. Candidates not covered by this run: album art
  (the card had no covers), a 20k+ library, a slow or fragmented card, Bluetooth streaming.

Where to look:
- **Raise `URGENT_RUNWAY_MS`, or make the busy trickle rate-aware**: read enough chunks
  per slot to gain e.g. 2× the stream rate rather than a fixed 1–2 chunks, so 24-bit FLAC
  grows its runway under input instead of holding it.
- **Prime more before letting the UI run at a track start**: the first ~2 s are the only
  time the runway is under 1 s. A short busy-independent priority window after
  `startPlaying()` (fill to e.g. 3 s first) costs the UI ~100–200 ms of one-time lag.
- **Bench**: add *time to 5 s runway after a start* as a metric (the per-second trace has
  the data), and rerun with `--covers` and on a slow card.

### 2. Audio work dominates the CPU

Share of each second (`cpuPercent`, from the `[prof]` lines):

| Scenario | `audio` loop | `isr` feeder | `sd` | `ui` |
|---|---|---|---|---|
| Scroll + background FLAC 16/44 | 26% | 5% | 7% | 4% |
| Navigation + background | 45% | 5% | 8% | 17% |
| Start under load, FLAC 24/48 | 69% | 11% | 27% | 8% |
| Stress (scroll while FLAC 24/48 plays) | 46% | 11% | 15% | 2% |
| Idle start, FLAC 24/48 | | 11% | 40% | |

- `audio` (`PodPlayer::loop()` / `AudioPlayer::loop()`) is the biggest consumer by far,
  much more than the SD reads it does (`sd` + `psW`). The difference is PSRAM→FIFO top-ups,
  waiting for the SPI1 lock, and the monitor/rate bookkeeping. Worth profiling inside
  `AudioPlayer::loop()` with finer `audioprof` categories.
- Background audio makes the UI slower: one-row scroll p95 goes from 41 ms (silence) to
  63 ms, search-result scrolling from 22 to 40 ms. Every main-loop pass pays the audio
  work, so any cut there shows up directly in UI latency.

### 3. Main-loop blocks: real ones are ≤ 190 ms, the 400–530 ms ones are bench artifacts

The `[prof]` line's `maxLoop` (the longest gap between two `loop()` passes):
- **Real**: idle track starts 82–190 ms (24/48 FLAC worst: opening the file, FLAC header
  and first fill); navigation 123 ms; stress 90 ms; scroll 66 ms.
- **Artifacts** (the report's *Longest main loop, track start under load* = 530 ms):
  correlating the log shows every 400–530 ms stall right after a bench `#type` or
  `#clear`. They process the whole query in one loop pass: typing `opentone bench samples`
  is 21 queries back to back (412 ms), and `#clear` re-runs the query after each deleted
  character. A user types one character per wheel click, each in its own pass (30–45 ms).
  **Fix the bench** before trusting that metric: process one character per loop pass, like
  the wheel inputs (`software/src/app/bench.cpp`, `typeText()`/`clearQuery()`).
- The 190 ms track-start block is the one to watch: it happens exactly when the buffer is
  empty. Splitting `startPlaying()`'s first fill over several passes would cap it.

### 4. Screen transitions cost 850 ms each

Opening a list and going back are 850–865 ms p50/p95, almost all of it the fixed 800 ms
slide (`ui_engine_transitions.hpp`, `animManager.animate(..., 800, ...)`). Inputs during a
transition are dropped (`isNavigationBusy()`), so navigating three levels deep takes 2.5 s
whatever the hardware does. iPod-like transitions are ~250–350 ms: shortening the
animation is the single biggest perceived-speed win, and costs nothing.

### 5. Memory and boot

- **PSRAM capacity is the hard limit**: this index uses ~284 B/track (138 B lookup tables +
  146 B search trie), so the 2 MB music region holds **~7,300 tracks**. 20,000 tracks needs
  5.45 MB and overflows silently (nothing checks the end; past 8 MB it wraps onto the audio
  buffer). That's what `notes/performance-and-memory.md` predicted; its step 1 (bounds
  checks) and step 2/3 (paths and trie on SD) are the fix. Native check:
  `OPENPOD_BENCH_CARD=<card> npm test` (`test/test_bench_dataset.cpp`).
- **Boot is linear in library size**: 1.7 MB of index in 4.4 s, about 390 KB/s, far below
  what the SD and PSRAM buses do (`docs/blog/09-10.md`). At 20k tracks it was 13.9 s
  (6.3 + 6.4). Bigger read chunks or overlapping SD reads with PSRAM writes should cut it
  well before the trie moves to SD.
- **Free RAM: 5.5 KB** (5.6 KB at 20k tracks). Little room left for new features.

### 6. Names in non-Latin scripts become empty rows

`sanitize_name` (`syncer/crates/core/src/index/text.rs`) keeps only printable ASCII after
folding accents, so a name written only in Japanese, Cyrillic or Greek becomes `""`. Those
rows sort first: on this card the first 24 tracks and 1 album are blank. Real libraries
with such titles will look broken. Options: transliterate, fall back to the file name, or
add glyphs (`docs/roadmap.md` already lists non-ASCII glyph support).

### 7. Smaller observations

- **Fast scroll moves a page, not N rows**: on lists over 3 screens, one scroll event of
  several clicks jumps a screen (7 rows) (`FAST_SCROLL_MIN_PAGES`, `app/input.cpp`). The
  bench's *fast burst* measures page jumps.
- **Seek settles in ~1.0–1.25 s**: the call itself is 27–79 ms, but the position reported
  by the decoder takes about a second to reach the target. That's the decode-time readout,
  not necessarily the audio. Check by ear before optimising.
- **Search cost doesn't depend on the hit count** (0 hits: 33 ms, 99 hits: 45 ms per
  character): it's dominated by the trie walk and redraw. Fine at 6k tracks; re-measure
  once the trie lives on SD.

## Caveats on the bench itself

Fix these before comparing future runs against this one:
1. `#type`/`#clear` run in one loop pass (finding 3): fix in `bench.cpp`.
2. **"Lowest runway" metrics include track starts.** A track always starts at 0 runway, so
   *Lowest buffer runway* (0 s) and *Runway while browsing* (0 s: the background loop
   restarted mid-*navigation*) only show that a start happened. Exclude the first seconds
   after a start, or report *time to N s runway* instead.
3. Per-second granularity: the `runway=`/`[prof]` lines are 1 s windows, so a dip shorter
   than a second only shows as `min=`.
4. One run, no repeat yet: run twice on the same firmware to measure noise before setting
   the regression threshold (10% in `bench compare`).

## Next steps

1. Bench fixes 1 and 2 above, then rerun to get clean baselines.
2. Bounds checks on the PSRAM index (`notes/performance-and-memory.md` step 1). Then move
   the trie and paths to SD, and rerun at 20k tracks.
3. Track-start priming and a rate-aware busy trickle (finding 1); judge with *start under
   load* on 24/48 FLAC with covers.
4. Shorter slide transitions (finding 4).
5. Profile inside `AudioPlayer::loop()` (finding 2).
6. Decide what to do with non-Latin names (finding 6).

## 2026-10-16: paged index, 100k tracks (`c110316-dirty`)

The index is now one file, `opentone/index.pack`, read from the card a 4 KB page at a
time into a PSRAM cache (480 pages, 4-way set associative, `src/storage/PageStore.h`).
That lifts the PSRAM limit, so this run uses a **100,008-track** card (24 MB pack), against
the 6k-track run above:

- Boot to ready 3.1 s (was 5.6 s at 6k and 13.9 s at 20k): the boot reads only the pack's
  section table. Library index 166 ms, search index 8 ms.
- 0 underruns in every scenario.
- Costs of reading from the card, cold cache: slowest search keystroke 179 ms (45 ms),
  open a list p95 1.0 s (865 ms), scroll one row p95 76 ms (63 ms). A page miss is
  ~6.5 ms: the time to read 4 KB through SdFat. With 8 KB pages read 512 bytes at a time
  it was ~26 ms, and back-to-back cold keystrokes (~550 ms each) drained the audio buffer
  right after a track start (1,515 underruns).
- A fragmented pack makes every seek walk the FAT (+9 ms a miss). The syncer now
  writes the pack to a new file and renames it. Rewriting in place reused the old pack's
  clusters and fragmented it. The firmware warns at boot when the pack isn't contiguous.
- *Fast scroll burst* now measures letter jumps (24-click events are fast enough for
  first-letter jumps), not page jumps: not comparable with older reports.

Next: prefetch the hot sections (names of artists/albums/genres, relation starts, letter
tables, the trie's top levels) while idle after boot.
