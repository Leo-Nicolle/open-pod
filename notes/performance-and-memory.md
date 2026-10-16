# Performance and memory: a music index that scales to a full SD card

> **Status (2026-10-16): superseded by the paged index.** The syncer now writes every
> table and the search trie into one file, `openpod/index.pack`
> (`syncer/crates/core/src/index/pack.rs`), with dense formats (no id arrays, relations
> with prefix-sum starts). The firmware no longer copies anything at boot: it reads
> 4 KB pages of the pack on demand into a PSRAM cache (`software/src/storage/PageStore.h`,
> `PAGE_STORE_PAGE_LOG2 = 12`: 480 pages, 4-way set-associative, CLOCK eviction), so the
> library is bounded by the card (20k tracks: 5.3 MB pack; 100k: 24 MB). Podcasts get
> `podcasts.pack`; the listening state has a fixed 128 KB region (`PODCAST_STATE_*` in
> `PSRAM_controller.hpp`). Old cards show "Library needs a re-sync". The current memory
> budget follows; after it, the original analysis.

## Current memory budget (2026-10-16)

Measured on the `nucleo_f446re` build: static sizes come from the ELF (`nm -S`, with
`gdb ptype /o` on a `-g` build for the fields inside each object), heap sizes from the
`new`/`reserve` calls, and PSRAM from the region map in `PSRAM_controller.hpp`. Every
allocation (size, file:line, variable, usage) is in [`notes/RAM.json`](RAM.json), the
source of truth, and is drawn on the docs page `/tools/memory-layout`.

**RAM: 128 KB.** Static data (.data + .bss) is 92,828 B (70.8%), down from 109,088 B
before the quick wins below (−16,260 B). The heap takes ~17.5 KB, which leaves **~20 KB
for the stack** (was ~4.4 KB).

| Allocation | Size | Main parts |
|---|---|---|
| `g_buffers` (framebuffers) | 38,408 B | two 320×30 RGB565 strips, 19,200 B each |
| `player` (PodPlayer) | 37,632 B | 32 KB audio FIFO, 4 KB SD scratch (shared with PageStore), one 256 B path |
| List row cache (heap) | 16,380 B | 7 pre-rendered rows × 2,340 B (2 bpp) |
| `state` | 5,740 B | 4 KB list strings, search hits 800 B, route stack 160 B + one 64 B name |
| `musicLookup` | 1,600 B | the card's only SdFat (1,180 B), section headers |
| `Serial2`, `pageStore` | 1,432 B, 1,348 B | 1 KB TX + 256 B RX; `slotKey[480]` u16 960 B |
| `btModel`, `bluetooth`, `musicIndex`, `fontRenderer` | 852, 608, 436, 296 B | 12 BT devices × 62 B; BtUart 256 B RX + 64 B TX; node batch; 4 × 32-level blend tables |

**PSRAM: 8 MB.**

| Region | Start | Size | Used by |
|---|---|---|---|
| Audio ring | 0x000000 | 4 MB | `Audio_buffer`: current track read-ahead |
| Cover scratch | 0x400000 | 1 MB | PSRAM self-test, LinkBench, cover benchmark only |
| Next-track prefetch | 0x500000 | 1 MB | start of the next track |
| Podcast listening state | 0x600000 | 128 KB | 20 B per episode, room for 6,553 |
| Index page cache | 0x620000 | 1,966,080 B | PageStore, 480 × 4 KB pages, all used |

Findings:
- The stack now has ~20 KB once the heap is allocated. Its depth is measured on the device
  (Step 0 below): `stackFree` / `stackMax` in the `[prof]` line, `stack_free_min` /
  `stack_used_max` in the bench `stats`. Not run on hardware yet for these numbers.
- The 1 MB cover region isn't used by the production firmware (AlbumArt reads covers from
  the SD card), so it can be reclaimed, e.g. for a bigger page cache.

## Making room (suggestions, 2026-10-16)

Every allocation in `RAM.json` checked against the code that uses it. Savings are
estimates from the field sizes; measure each change (bench report + `[prof]` line)
before keeping it.

**Step 0: measure the stack first.** *Done (2026-10-16): `app/stack_probe.cpp`, reported
in the `[prof]` line and the bench `stats`.* The ~4.4 KB of headroom is a guess. Fill the stack
region with a pattern at boot (from `_end` of the heap to `_estack`), report the
high-water mark in the `[prof]` line and the bench report, and run the bench scenarios.
The first RAM won should go to stack safety, not to new features.

PSRAM isn't memory-mapped: everything in it is read into RAM over SPI2 (~3 µs per access
plus ~0.36 µs per byte at 22.5 MHz). So only **cold** data (read on a screen change, a
key press, a BT event) can move there. Hot data (read every frame or every audio tick)
must stay in RAM.

### Quick wins (low risk, ~17 KB)

**Done (2026-10-16): all 12, −16,260 B of static RAM** (measured with `nm -S` on the
`nucleo_f446re` ELF, per item in the last column). Host tests pass; the device checks
(bench scenarios, BT reconnect burst, small fonts) are still to do.

| # | Allocation | Was | Idea | Saved |
|---|---|---|---|---|
| 1 | `state.elementsBuffer` (`state/state.h:139`) | 8,192 B | 64 names × 128 B of room, but real names average ~30 chars (64 names ≈ 2 KB). `loadNames()` is already bounded by the buffer size: with 4 KB, a window of long names just holds fewer names and reloads a bit sooner. Check the reload count while fast-scrolling. | 4,096 B ✅ |
| 2 | Trie caches in `musicIndex` (`storage/Music_index.h:72-76`) | 2,816 B | `node_cache`, `node_cache_index` and `string_cache` date from before PageStore. Every read now goes through `pageStore.read()`, which is already served from the PSRAM page cache: these cache a cache. Drop them (keep `node_batch`), compare keystroke latency in the bench search scenario. If the root is slow, cache only the root's children. | 2,824 B ✅ |
| 3 | `rawHits` (`state/search.cpp:17`) | 2,000 B | 100 × 20 B raw trie results, copied into `hits` (8 B each) by `rankHits()`. Dedup + rank as results are collected (callback or `Hit` output from `searchByPrefix`), and `rawHits` goes away. | 2,000 B ✅ |
| 4 | Font blend tables (`rendering/font_renderer.h:12`) | 4 × 518 B | 256 blend levels per colour pair, but the RGB565 display has only 32/64 levels per channel. 16 levels (`alpha >> 4`) gives tables of 32 B. Check that small fonts still look smooth. The list rows already use 2 bpp (4 levels). Bonus: 4-bit glyph data would also halve the fonts in flash. | 1,792 B ✅ (32 levels, not 16: RGB565 has 32 levels of red/blue) |
| 5 | Serial TX buffers (`-DSERIAL_TX_BUFFER_SIZE=1024`) | 2 × 1,024 B | The flag sizes **both** UARTs (debug `Serial2` and the BT `Uart`). BT commands are ≤ 32 B (`CMD_LEN`): 64 B of TX is enough. Either give the BT driver its own small TX ring, or keep 1 KB only in builds with `LOG_LEVEL=2` / the profiler and use 128 B otherwise. | 980 B ✅ (own BT driver, `bluetooth/BtUart.h`) |
| 6 | Second SdFat (`sound/Audio_buffer.h:184`) | 1,180 B | Audio and library open the same card on the same bus. Hand `Audio_buffer` a reference to `musicLookup`'s SdFat. Bonus: no more "`FsFile::open(path)` uses whichever SdFat began last" trap (`bluetooth/BtConfig.h`). | 1,180 B ✅ |
| 7 | `pageStore.slotKey` (`storage/PageStore.h:96`) | 2,048 B | `uint32_t` per slot, 512 slots for 480 used. A page number fits in 15 bits (a 100k-track pack is 24 MB = 6,144 pages) + 1 pack bit: `uint16_t`. Size it to the real slot count. If the cache grows (see PSRAM below), 1,024 × 2 B costs the same as today. | 1,104 B ✅ (packs up to 128 MB) |
| 8 | Route stack (`state/router.h:28-40`) | 8 × 96 B | `currentPage`, `totalResults` and `hasMore` are only ever reset, never read: delete them. `entityName[64]` is only shown for the top route (header, podcast heading): keep one name for the current route, and on `popRoute()` rebuild it from `(scope, entityId)` through MusicLookup (one PageStore read) or PodcastMenu. Route ≈ 20 B. | 544 B ✅ |
| 9 | Path copies (`sound/player.h:16`, `sound/Audio_buffer.h:187,212`) | 3 × 256 B | `PodPlayer::currentPath` and `Audio_buffer::_currentFileName` hold the same path. `_nextFileName` is only compared to pick the prefetch: compare track ids instead. One 256 B path buffer is enough. `state.playingTrackName` (128 B) can be read from `playingTrackId` when drawn. | 512 B ✅ (`playingTrackName` kept: NowPlaying holds the pointer) |
| 10 | BT devices (`bluetooth/BtModel.h:46`) | 12 × 88 B | MAC as 6 bytes instead of 18 chars of text (format it when sending/drawing), the 7 `bool`s as bit flags, and `alias` kept only in `btConfig` on the SD card (draw the alias if set, else the name, from one 25 B field). ~44 B per device. Or move the table to PSRAM (see below). | ~280 B ✅ (alias kept in RAM) |
| 11 | Search row strings (`state/search.h:103-104`) | 616 B | 7 titles + 7 subtitles prepared ahead. `describeHit()` into one 88 B row buffer while each row is drawn. Costs a PageStore read per row on redraw (µs, page cached). | 616 B ✅ (+616 B of stack in `fillRows()`) |
| 12 | Debug-only objects | 292 B + | `WheelTrace trace` (`app/diagnostics.cpp:5`) exists with `DEBUG_WHEEL_TRACE=0`: put it under `#if`. `btModel.toast` and `appToast` are two copies of the same 68 B `Toast`: share one. The framework's default `Wire` (228 B) is unused by our code but pulled in by the Adafruit libraries' `&Wire` default arguments, so it's hard to drop. | 353 B ✅ (`Wire` left) |

### Bigger changes (measure first)

- **List row cache, 16,380 B heap** (`ui/list/list_cache.hpp:56`). Each row is 312 × 30 px at
  2 bpp, but text is drawn at y = 10 with a 21 px line height: rows 0–9 are always empty.
  Store only the text band (~22 rows) and let `ListRenderer` fill the margins with the
  background: 1,716 B per row, **~4.4 KB saved**. Make it one static array instead of 7
  `new[]` blocks (no heap left at all, visible in the map). Keep it in RAM: it's read every
  scroll frame (7 × 2.3 KB from PSRAM ≈ 7 ms per frame).
- **Framebuffers, 38,400 B** (`ui/ui_types.h:54-55`). Two 320 × 30 strips, so the CPU draws
  one while DMA sends the other. 15-row strips would save **19.2 KB**, but every view assumes
  one list row = one strip (`CHUNK_HEIGHT`, `podcast_view.hpp`, the fast-scroll overlay), and
  it doubles the `pushWindow()` calls per frame. Measure fps with the display bench
  (`main_display_bench.cpp`) before starting. Biggest UI win, a real refactor.
- **Audio FIFO, 32,768 B** (`sound/AudioFifo.h:19`). It's sized for the worst main-loop
  stall (190 ms measured, 530 ms under load, `notes/benchmark-results.md`), not for audio
  itself. Two ways to shrink it: (a) cut the long stalls first (page cache misses, cover
  reads, list rebuilds), then try 16 KB (**16 KB saved**); (b) let the feeder interrupt top
  up from the PSRAM ring itself when the FIFO runs low. It would only have to cover one
  PSRAM read: 8 KB, **24 KB saved**. (b) needs a lock on SPI2 shared with PageStore,
  like `SharedSpiDriver` does for SPI1. Highest reward, highest risk: do it last, with the
  FLAC bench running.

### PSRAM: ~2 MB to reclaim, and what could move there

- **Next-track prefetch, 1 MB → 64 KB.** `prepareNextTrack()` fills up to 1 MB, but
  `consumeNextTrackIfMatches()` (`sound/Audio_buffer.cpp:315`) adopts only the first
  `NEXT_TRACK_ADOPT_MAX_BYTES` = 32 KB and re-reads the rest from the SD card. The rest is
  read from the card and written to PSRAM for nothing. 64 KB (32 KB + a synthetic FLAC
  header + margin) frees **~960 KB** and SD bandwidth. Stop the prefetch at that size.
- **Cover scratch, 1 MB.** Unused in production. Keep a 4 KB scratch for the PSRAM self-test
  (`SPI_PSRAM::SCRATCH_ADDRESS`), and let LinkBench and the cover benchmark use the free
  region at run time.
- **Use it for the index page cache.** 1.88 MB → ~3.9 MB (960 × 4 KB pages): far fewer card
  reads while browsing and searching a 100k-track library, so fewer main-loop stalls, which
  in turn lets the audio FIFO shrink. With `uint16_t` keys (#7), 1,024 slots cost 2 KB of
  RAM, the same as today.
- **The audio ring (4 MB ≈ 38 s of FLAC)** is far above what refills need (1.3 s of runway
  vs a 190 ms stall). 3 MB would still be ~28 s, if more cache is needed later.
- **Cold RAM data that could move to PSRAM** (a small region after the podcast state):
  - BT device table, ~1 KB: read on the BT screens and on module events only. Keep the
    connected device's name in RAM for the header.
  - Route stack below the top route: read only on Back.
  - Search hits (800 B): only the 7 visible rows are needed in RAM.
  - Podcast roll-up cache (`podcastMenu.rollups`, 256 B) and `heading`/`navTitle`.
  Each is a few hundred bytes. Only worth doing after the quick wins, and only for data
  read on a key press, never in the render or audio path.

**Total.** The quick wins free ~17 KB of RAM (static and heap), roughly 4× today's stack
headroom. The list-row crop adds ~4.4 KB. The strip and FIFO refactors could add 19–24 KB
each. Suggested order: stack measurement → #1–#7 (simple, measurable) → prefetch shrink and
a bigger page cache → the rest.

## Original analysis

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
