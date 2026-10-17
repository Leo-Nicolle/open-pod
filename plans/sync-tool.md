# opentone library sync: Rust rewrite plan

> **Status (2026-10-12):** phases 1–3 done and used on the real card; `sync/` is removed. Phase 4: the desktop app is in `syncer/app/` (design in `.agents/opentone-Sync-design/`), used on the real card. Phase 5: the same app runs in the browser (`syncer/crates/web` + `app/src/api/web.ts`), embedded in the docs at Tools → opentone Sync; tested end to end on OPFS in headless Chrome, not yet on a real card. Usage: [`syncer/README.md`](../syncer/README.md).

## Context

Today `sync/sync.sh` mirrors **one** library (`~/Music`) into a host-side `sync/staging/` folder. On the way it converts FLACs the VS1053b can't play (>48 kHz, >24-bit, >2 ch, or LPC order >12 via `flac-max-lpc-order.py`). It then runs the TS indexer (`indexer/generate.ts`) over staging and does `rsync --delete` onto the card. This has some limits:

- **It's single-source.** `--delete` means syncing from a second computer wipes the first one's music.
- **It's slow and heavy.** Bash runs one file at a time, uses ffprobe per file, keeps a full 15 GB staging copy on the host, and re-crawls and re-reads tags for every file at each index build.
- **It needs a toolchain** (bash, rsync, ffmpeg, python, node and sharp), so it isn't portable to other users, Windows or the browser.
- **Albums are keyed by name only** (`indexer/src/crawler.ts:L194` `readMetadata`), so different artists' "Greatest Hits" merge.

The goal is one portable tool. Sync is additive and works from any number of machines, removal only happens on explicit request, and it's fast and parallel. It handles mp3/flac and converts everything else, tolerates bad tags, and is built as a pure-Rust core so a Tauri app and a browser (WASM) build can reuse it later.

### Decisions (agreed)
- **Removal:** sync only adds or updates. Removal is explicit, driven by a manifest on the card.
- **Bad tags:** infer from the path (`Artist/Album/NN Title`), otherwise "Unknown …". Always copy the track and report weak files. beets/MusicBrainz is an optional extra, not built in.
- **Delivery:** Rust core and CLI first. Tauri UI and the WASM/website build come later on the same core.
- **Album identity:** `albumartist + album` (fallback: majority track artist).

## Architecture

New Cargo workspace at `syncer/`:

```
syncer/
  crates/core/   opentone-core: no direct std::fs, I/O behind traits
    storage.rs     trait Source / trait Card (read/write/rename/list); native impl = std::fs
    scan.rs        walk + fingerprint (size, mtime, xxh3 of first+last 64 KiB)
    probe.rs       format/rate/depth/channels/duration (symphonia), FLAC LPC order (port of flac-max-lpc-order.py)
    tags.rs        lofty read; path-inference fallback; normalisation (sanitizeName parity with indexer/src/utils.ts)
    policy.rs      decide: Copy | ReencodeFlac{rate,bits} | TranscodeLossy | Skip(reason)
    convert.rs     symphonia decode -> rubato resample -> flacenc encode; tags+cover copied with lofty
    manifest.rs    on-card library manifest (load/merge/atomic save)
    plan.rs        diff(source scan, manifest) -> SyncPlan {add, upgrade, skip, conflicts}
    index/         port of indexer: trie, string/relationship/duration/track_to_* binaries, thumbnails (QOI/raw565 + dither)
  crates/cli/    `opentone` binary (clap, indicatif)
```

Crates: `symphonia` (decodes mp3/flac/wav/aiff/alac/aac/vorbis), `flacenc` (a pure-Rust FLAC encoder that we limit to the subset, max LPC 12), `rubato`, `lofty`, `image`, `rayon`, `xxhash-rust`, `serde` + `ciborium`/JSON. The core avoids C dependencies, which keeps the `wasm32-unknown-unknown` build open.

### On-card layout
```
/Music/<AlbumArtist>/<Album>/<D-NN> <Title>.<ext>   canonical, tag-derived, FAT/exFAT-safe names
/opentone/*.bin, thumbs.bin                         unchanged formats; firmware untouched
/opentone/library.json                              manifest (source of truth)
/opentone/.cache/thumbs/<srcHash>.qoi               per-album thumbnail cache, so reindexing is cheap
/opentone/tools/                                    optional: CLI binaries for every platform
```
The paths come from the tags, so the same album synced from two machines lands in the same place and dedupes. The firmware only reads `path_index.bin`, so the layout change is transparent to it.

### Manifest entry (per track)
`key` (normalised albumartist|album|disc|track|title, or the MusicBrainz recording id when present), `card_path`, `src_fingerprint`, `codec/rate/bits`, `conv_policy_version`, `tags` (the resolved values plus a `tag_quality: tagged|inferred|unknown` field), `duration`, `album_art_hash`, `added_by` (machine names), `added_at`.

The index is rebuilt **from the manifest alone**, with no re-crawl or re-parse of the card, so it takes seconds.

### Sync algorithm (`opentone sync <lib>...`)
1. Load `library.json` from the card. If it's missing, run `opentone adopt`, which scans the existing card once and builds the manifest. This migrates current cards.
2. Scan the source in parallel and fingerprint each file. A file whose fingerprint is already in the manifest is skipped without decoding (fast path).
3. Probe and read tags (rayon), then compute the key and the policy:
   - FLAC within limits and LPC ≤12 → copy. FLAC outside them → re-encode to 48k/16/2, or a lossless LPC-only re-encode (same rules as `sync/convert-flac.sh`).
   - MP3 → copy.
   - WAV/AIFF/ALAC (lossless) → FLAC (16/48 if over the limits, native otherwise).
   - Other lossy formats (m4a/ogg/opus/wma) → transcode to MP3 V0 via an **optional `ffmpeg` fallback** if it's on PATH. Otherwise skip them and report it. Pure-Rust MP3 encoding is out of scope.
4. Diff against the manifest:
   - New key → **add**.
   - Same key at better quality (lossless beats lossy, then higher bitrate) → **upgrade**.
   - Otherwise → append the machine to `added_by` and skip.
5. Execute the plan:
   - N converter threads feed one bounded channel into **a single card writer**. SD write throughput is the bottleneck, and parallel writes thrash it.
   - Each file is written as `*.part`, then renamed.
   - The manifest is checkpointed atomically (tmp + rename) every N files, so an interrupted sync resumes cleanly. Orphan `.part` files are cleaned up at startup.
6. Rebuild `/opentone/*.bin` from the manifest, reusing cached thumbnails.
7. Print a summary and write `opentone/sync-report.txt` (inferred or unknown tags, skipped files, conversions). The report replaces `sync/conversions.log`.

### Other commands
- `opentone status <lib>`: a dry run (what would be added or upgraded, and how much space it needs vs. free space).
- `opentone ls [--artist|--album]`, and `opentone rm --artist X | --album Y | --path P` (deletes the files and manifest entries, then reindexes).
- `opentone index`: rebuild only.
- `opentone install-tools`: copy the release binaries into `/opentone/tools/`.
- Config in `~/.config/opentone/config.toml`: machine name, default libraries, excludes. It replaces `sync/.env` and `exclude.txt`.
- Optional `--autotag beets`: when `beet` is on PATH, list the inferred/unknown albums and print the `beet import` commands to run. The tool never runs them silently and never modifies the source library.

## Phases

1. **Core + index port (parity).**
   - Port `indexer/src/{crawler,music-indexes,trie,thumbnails,utils}.ts` to `core/index`.
   - Add a hidden `--album-key=name` flag, so a golden test can diff the Rust output byte-for-byte against the TS output on a fixture library.
   - Then switch the default to `albumartist+album`.
2. **Sync engine + CLI:** scan/probe/policy/convert/manifest/plan, plus the `sync`, `status`, `adopt`, `index`, `rm` and `ls` commands. Keep `sync/sync.sh` until the CLI has been used on the real card. After that, remove `sync/` and point `sync/README.md` → `syncer/README.md`.
3. **CI:** a GitHub Actions matrix (linux x86_64/aarch64, macOS universal, windows) that attaches binaries to releases and feeds `install-tools`.
4. **Tauri app:**
   - Folder picker, a "what's new vs. card" view, a progress bar, and an album grid for removal.
   - Start with that, not a full iTunes view. The grid comes almost for free from the manifest plus cached thumbnails.
5. **Web (WASM):**
   - Same Vue UI as the app (`syncer/app`), embedded in the docs site's Tools section; a third `Api` implementation next to the Tauri and sample-data ones.
   - **I/O split (revised 2026-10-12).** Folder access in the browser is async JS (File System Access API), the core is sync Rust, so traits over directory handles don't fit. Instead:
     - JS walks folders (`FileSystemDirectoryHandle`), writes to the card (`createWritable()`, which Chrome writes to a temp file and swaps in on close) and keeps `library.json` and the index files on the card.
     - Rust runs in workers and reads library/card files itself through a `Read + Seek` adapter over `FileReaderSync` (sync reads of `File` slices are allowed in workers), so probing, hashing, LPC scans and covers read only the bytes they need, as natively.
     - The core is split accordingly: per-file work takes a reader, planning and manifest updates are pure functions, the index is returned as bytes. Native code keeps calling the same functions with `std::fs::File`.
   - **Threads (revised).** `wasm-bindgen-rayon` needs COOP/COEP headers, which GitHub Pages can't send. Instead a pool of N workers, each with its own WASM instance; JS hands out files. Inside WASM, rayon falls back to one thread.
   - Chromium only (Firefox and Safari can't write to a picked folder): feature-detect, and point other browsers to the desktop app.
   - No ffmpeg in the browser: AAC/Ogg/Opus/WMA/WavPack/APE are reported as skipped ("needs the desktop app").

## Critical files
- New: `syncer/` (workspace), `.github/workflows/syncer.yml`
- Port from: `indexer/src/crawler.ts`, `indexer/src/music-indexes.ts`, `indexer/src/trie.ts`, `indexer/src/thumbnails.ts`, `indexer/src/utils.ts` (`sanitizeName`, `splitArtists`), `indexer/generate.ts`
- Port the logic from: `sync/convert-flac.sh` (limits and ffmpeg args), `sync/flac-max-lpc-order.py` (LPC frame parser, CRC-8 / frame-number validation)
- Firmware readers (must keep working unchanged): `software/src/storage/Music_lookup_load.cpp`, `Music_lookup.cpp`, `Music_index.cpp`
- Update: `plans/TODO.md` (the Indexer section → link to this plan), `sync/README.md`

## Verification
- `cargo test`:
  - Golden parity test: the TS indexer vs. `opentone index --album-key=name` on `indexer/test` fixtures, with identical `.bin` files.
  - LPC parser tested on FLACs encoded at compression levels 8 and 12, cross-checked against the Python script.
  - Policy table tests (96k/24, 32-bit, 5.1, LPC 32, WAV, m4a).
- Multi-machine scenario in a temp dir:
  - Sync lib A to the card, then lib B (overlapping album at MP3 vs. FLAC). Expect a union, one copy of the overlap upgraded to FLAC, and `added_by` = both machines.
  - Re-run sync A. Expect zero work, and it should finish in under 2 s on the ~1000-file library.
- Interrupt mid-sync (Ctrl-C), then re-run. Expect no `.part` leftovers, a valid manifest, and only the remaining files copied.
- `opentone rm --album X`: the files are gone and the index has no dangling ids.
- On the device: flash the current firmware and boot with a Rust-synced card. Browse artists, albums and genres, play a converted hi-res FLAC and an LPC-32 FLAC without stutter, and check that the covers display.
- Benchmark against `sync/sync.sh` on `~/Music` (15 GB, 606 flac and 348 mp3): a cold sync and a no-op re-sync.
