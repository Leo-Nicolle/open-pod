# Performance and memory: a music index that scales to a full SD card

> **Status (2026-10-16): superseded by the paged index.** The syncer now writes every
> table and the search trie into one file, `openpod/index.pack`
> (`syncer/crates/core/src/index/pack.rs`), with dense formats (no id arrays, relations
> with prefix-sum starts). The firmware no longer copies anything at boot: it reads
> 8 KB pages of the pack on demand into a PSRAM cache (`software/src/storage/PageStore.h`,
> ~240 pages, CLOCK eviction), so the library is bounded by the card (20k tracks: 5.3 MB
> pack; 100k: 24 MB). Podcasts get `podcasts.pack`; the listening state has a fixed
> 128 KB region (`PODCAST_STATE_*` in `PSRAM_controller.hpp`). Old cards show
> "Library needs a re-sync". What follows is the original analysis.

## Context

At boot, the whole music index (`/openpod/*.bin`, from the indexer) is copied into the
2 MB music region of PSRAM (`MUSIC_INDEX_BASE_ADDRESS`, 6–8 MB, see
`software/src/storage/PSRAM_controller.hpp`). Two problems:

1. **No bounds check.** `MusicLookup::init()` (`storage/Music_lookup_load.cpp`) copies its
   16 files one after another, and `MusicIndex::init()` (`storage/Music_index.cpp`)
   copies the search trie after them. Neither checks the end address. `writeData()`
   sends 24-bit addresses and the chip has 23 address bits, so anything past 8 MB wraps
   to address 0, the audio ring buffer. Nothing is reported: the index gets overwritten
   by audio once a track plays, and lists/search/paths read garbage from then on.
2. **It doesn't scale.** Measured on the test library (552 tracks, 165 KB), each track
   costs ~305 B. With realistic names (30-char titles, 90-char paths) it's ~470 B. 2 MB
   holds only ~4,500–6,900 tracks: about 26–40 GB of 4-minute MP3s, or 13–20 GB of
   3-minute 128 kbps ones. A 128 GB card is 3–5× over.

Where the 305 B/track goes:

| File | B/track | Scales with |
|---|---|---|
| `music_index.bin` (search trie) | 183 | ~3 results/track (20 B each), 24 B nodes, word pool |
| `path_index.bin` | 65 | 8 B/entry + path (avg 57 chars in the test library) |
| `track_index.bin` | 24 | 8 B/entry + title (avg 15.5 chars) |
| relations, durations, track→album/artist | ~33 | fixed 2–8 B each |

The trie and the paths are 80% of it, and neither needs to be in PSRAM:
- **Paths** are read once per track start, and the track file is opened on the SD card
  right after anyway.
- **The trie** is only walked during search.

With both on the SD card, PSRAM holds ~57 B/track (test names) to ~75 B/track (realistic
names): **~28,000–36,800 tracks in 2 MB**.

| 128 GB card of… | Tracks | PSRAM now | PSRAM after |
|---|---|---|---|
| 4 min @ 192 kbps (5.8 MB) | ~21,500 | 6.6–10 MB ❌ | 1.2–1.6 MB ✅ |
| 3 min @ 128 kbps (2.9 MB) | ~43,000 | 13–20 MB ❌ | 2.4–3.2 MB ❌ (✅ with step 4a) |
| 1 min @ 128 kbps (0.96 MB) | ~130,000 | 40–61 MB ❌ | 7.4–9.8 MB ❌ |
| 30 s @ 64 kbps (0.24 MB) | ~520,000 | 160–245 MB ❌ | 30–39 MB ❌ |

The scaling is linear, extrapolated from 552 tracks. Step 0 checks it on a big dataset.

## Steps

### 0. Measure on a big dataset (before changing anything)

The user is building a much bigger music library. For it, and for the test library,
record:
- Indexer output: size of each `.bin`, track/artist/album counts, average title and path
  length, trie node/result/pool counts (the header of `music_index.bin`). That gives
  B/track per file and shows whether the trie really grows linearly (shared words should
  make it a bit sublinear).
- On device: the PSRAM end address after both loads (add a log of
  `musicLookup.getLastPSRAMAddress()` and the trie end), and boot load times. A
  timestamped boot log (reset the board over ST-Link with openocd, timestamp each
  serial line on the PC) gave ~200 ms for MusicLookup and ~205 ms for MusicIndex on the
  test library, with 512-byte chunks.
- Search latency per keystroke and track-start latency (log `millis()` around
  `SearchModel::runQuery()` and `PodPlayer::startPendingTrack()`). These are the
  baselines for steps 2–3.

Update the tables above with the measured numbers.

### 1. Bounds checks (small, do first)

- `MusicLookup::init()`: before each file's `loadDataFromSDCard()`, check that
  `base_address + total_size <= MUSIC_INDEX_BASE_ADDRESS + MUSIC_INDEX_SIZE`.
- `MusicIndex::loadFromSDCard()`: same check for `base_address + file_size`.
- Clamp each size computed from a file's header (`entry_count`, `data_size`) to the
  file's real size, so a damaged file can't claim more than it has.
- When something doesn't fit: stop, log
  `Music index too big: needs X KB, PSRAM has Y KB`, and boot with an empty library
  (the UI already copes with a failed `musicLookup.init()`). Never write past the region.
- Host tests (`test/test_music_lookup.cpp`, `test/test_music_index.cpp`, fake PSRAM):
  an index that just fits loads; one byte over is refused and nothing is written past
  the region.
- Indexer (`indexer/src/music-indexes.ts` / `index.ts`): after export, sum the
  PSRAM-loaded files and warn if they exceed `MUSIC_INDEX_SIZE`. Keep the constant in
  sync with `PSRAM_controller.hpp`, as a comment on both sides. After steps 2–3, only
  the files that stay in PSRAM count.

### 2. Paths read from the SD card

- `MusicLookup::getTrackPath()` (`storage/Music_lookup.cpp:28`) reads from PSRAM today.
  Instead, keep `path_index.bin` open and read entry → offset → string from the file
  (two small reads: the offset pair, then the path).
- Stop copying `path_index.bin` in `MusicLookup::init()`. Keep only its header in RAM
  (entry count, where the offsets and strings start).
- Callers: `PodPlayer::startPendingTrack()` (current and next track) and
  `test/test_music_lookup.cpp`. Nothing else reads paths.
- Open the file through MusicLookup's own SdFat (`sd.open(...)`), not
  `FsFile::open(path)`, which uses whichever SdFat instance began last (see
  `bluetooth/BtConfig.h`).
- Measure track-start latency against step 0. Expected: a few ms more, negligible next
  to opening the track.

### 3. Search reads the trie from the SD card

- `MusicIndex` reads the trie in exactly three places, all through `psram.readData()`:
  `readString()` (`Music_index.cpp:174`), the node batch read (`:191`) and the results
  read (`:208`). Replace them with reads from the open `music_index.bin`
  (`seekSet` + `read`), at file offsets instead of PSRAM addresses. Keep the header and
  the computed section offsets in RAM.
- Stop copying `music_index.bin` at boot, which also removes ~205 ms from boot.
- Read straight from the file; don't load the whole trie into PSRAM on entering search.
  A 40k-track trie is ~7–12 MB, which doesn't fit even temporarily.
- The existing node and string caches (`node_cache_entry_t`, `string_cache_entry_t`)
  matter much more now. Check their hit rate while typing, and consider caching the top
  levels of the tree (root and first letters are read on every keystroke).
- Expected cost: one walk down the tree per letter, ~10–30 small reads plus the result
  entries and their names, so roughly 10–40 ms per keystroke at 25 MHz SPI. Measure
  against step 0.
- **Risk: SD contention with playback.** Audio streaming reads the same card. Type fast
  in search while a FLAC plays and watch the audio monitor (`LOG_AUDIO_BUFFER`) for
  underruns. If they appear, limit trie reads per loop iteration, or pause the search
  briefly while the audio buffer refills.

### 4. Optional, if step 0 shows we need more room

- **a. Cover buffer → index.** `COVER_BUFFER_SIZE` (1 MB) is a scratch region for the
  cover-format benchmark and isn't used by the production cover path. Giving it to
  `MUSIC_INDEX_SIZE` makes 3 MB: ~42–55k tracks after steps 2–3. Check
  `test/onboard/` before taking it.
- **b. Track titles on the SD card.** A list shows 7 titles at once, so reading them per
  visible row (same pattern as the paths) leaves only fixed-size arrays in PSRAM,
  ~41 B/track: ~75k tracks in 3 MB. Fast scroll (one title per screen jump, plus the
  letter overlay) needs a look: it reads titles while the wheel spins.
- **c. Narrower ids.** All ids and offsets are 32-bit. Under 65k tracks, 16-bit
  relation entries would halve them, at the cost of an indexer format version.

## Verification

- Host tests pass (`pio test -e test_native`), including the new bounds tests.
- Test library and big dataset, on the device:
  - The boot log shows the PSRAM end address inside the region and no errors.
  - Lists, Now Playing and search results show the right names.
  - Tracks start (paths from the SD card), including next/previous and the next-track
    prefetch.
- Big dataset: search while a track plays, no audio underruns in the audio monitor.
- Overflow: an index deliberately bigger than the region (shrink `MUSIC_INDEX_SIZE` in a
  test build) is refused with the "too big" log, and playback/UI keep working with an
  empty library.
- Compare boot time, keystroke latency and track-start latency with the step-0 baselines.
