# openpod (Rust sync tool)

Single binary that replaces the former `sync/sync.sh` and `indexer/`: additive sync
from any number of computers, conversion to formats the VS1053b plays, and
the `openpod/*.bin` index the firmware reads. Design and phases:
[`plans/sync-tool.md`](../plans/sync-tool.md).

Status: **phases 1–3 done** and used on the real card: indexer port; `sync`,
`status`, `adopt`, `index`, `ls`, `rm` and `install-tools`; CI builds in
`.github/workflows/syncer.yml` (push a `syncer-vX.Y.Z` tag to publish a
release). Phase 4, the desktop app, and phase 5, the same app in the
browser (docs site, Tools → OpenPod Sync), are in `app/` (see below).

## Build

```sh
cd syncer
cargo build --release   # -> target/release/openpod
cargo test
```

## Sync a card

```sh
openpod status ~/Music --card /mnt/openpod   # what sync would do
openpod sync   ~/Music --card /mnt/openpod
```

With a config file, both are just `openpod status` and `openpod sync`:

```toml
# ~/.config/openpod/config.toml (every key optional)
machine = "laptop"          # name in the card's manifest (default: hostname)
card = "/mnt/openpod"
libraries = ["~/Music"]
exclude = ["Various Artists/Bootlegs"]  # library-relative; hidden ones are always skipped
album_key = "artist-album"  # or "name"
podcasts = "~/Podcasts"     # mirrored to <card>/Podcasts (see below)
```

Flags win over the `OPENPOD_SD` / `OPENPOD_LIBRARY` / `OPENPOD_EXCLUDE` /
`OPENPOD_PODCASTS` environment variables, which win over the file.

- **Additive.** `sync` only adds and upgrades: run it from any number of
  computers, each with its own library, and the card gets the union. A
  track is identified by its tags (album artist, album, disc, track,
  title), so the same album from two computers is stored once, and a FLAC
  replaces an MP3 of the same track.
- **Removal is explicit:** `openpod rm --album "…"` (or `--artist`,
  `--path`) deletes the files and remembers it, so later syncs from any
  computer don't bring them back (`sync --include-removed` does).
  `openpod ls [--tracks]` lists what's on the card.
- **Conversion** to what the VS1053b plays (48 kHz, 24-bit, stereo, FLAC
  LPC order ≤ 12): MP3 and FLAC within limits are copied; hi-res or
  multichannel FLAC, WAV, AIFF and ALAC become FLAC (16-bit at 44.1/48 kHz
  when over the limits); FLACs at a higher LPC order are re-encoded
  losslessly. Other lossy formats (AAC, Vorbis, Opus, WMA) become MP3 V0
  and WavPack/APE become FLAC, both only when `ffmpeg` is installed.
- **Card layout:** `Album artist/Album/[disc-]NN Title.ext` with FAT-safe
  names, plus the album's `cover`/`folder`/`album`/`front` image.
- **Bad tags:** missing values come from the library layout
  (`Artist/Album/NN Title`), else "Unknown". `openpod/sync-report.txt` on
  the card lists those folders, the conversions and skipped files.
- **Interruptible:** Ctrl-C finishes the current files and saves; the next
  sync carries on. Files are written as `*.openpod-part`, flushed to the
  card, then renamed.
- **Existing cards:** the first sync reads the card once (`adopt`), builds
  `openpod/library.json`, and links library files to the tracks already
  there instead of copying them again.
- **Speed:** a no-op sync of ~1,600 tracks takes under a second. Copies run
  at the card's write speed (a USB 2 card reader caps it at ~20 MB/s).
- **Podcasts** (`podcasts = …` or `--podcasts <dir>`): the folder is
  **mirrored** to `<card>/Podcasts/`, laid out as it is (publisher / show /
  [series] / episode, any depth). New episodes are copied (and converted
  like music), episodes deleted from the folder are deleted from the card,
  and episodes that only moved (same content hash) are renamed on the card
  instead of copied again. Without the setting, a computer leaves the card's
  podcasts alone. The card keeps them in `openpod/podcasts.json`, apart
  from the music manifest, and every change rebuilds the player's podcast
  tables (`openpod/podcast_*.bin`). The listening state the player writes
  (`podcast_state.bin`: position, played) is keyed by the episode's content
  hash, so it follows the episode across rebuilds, renames and moves. A
  podcasts folder inside a music library is left out of the music.
- `openpod install-tools [--from <release folder>]` copies the binaries to
  `<card>/openpod/tools/`, so the card carries the tool to other computers.

## Desktop app (OpenPod Sync)

`app/`: a Tauri 2 app with a Vue 3 + Vite frontend, built from the design
handoff in `.agents/OpenPod-Sync-design/`. It does what the CLI does:
detects the card, reads an existing card once, shows what's new, syncs with
progress, browses and removes albums (and brings them back), shows the last
report and edits `~/.config/openpod/config.toml`.

```sh
cd syncer/app
npm install
npx tauri dev                 # run it (Linux needs libwebkit2gtk-4.1-dev …)
npx tauri build               # installers in syncer/target/release/bundle/
npm run dev                   # the browser version on http://localhost:1420
npm run build:web             # the browser version into docs/public/sync-app
```

`npm run dev` and `build` first build the WebAssembly module (`npm run wasm`:
needs `rustup target add wasm32-unknown-unknown` and
`cargo install wasm-bindgen-cli --version 0.2.129`). The docs site's
`npm run publish` runs `build:web` before building the site.

URL parameters for checking screens without a card: `?mock` (sample data,
`src/api/mock.ts`) with `?screen=whatsnew|syncing|library|settings`,
`?card=nocard|locked|empty|unread`, `?plan=nospace|uptodate`; and
`?theme=light|dark`, `?embed` (docs-page layout). `?selftest` runs the
browser backend end to end on the browser's private storage (OPFS) and
prints the result.

- `src-tauri/src/lib.rs`: the commands, over `openpod-core` (`card`,
  `view`, `library`, `sync`), and the `sync-progress` / `card-read` events.
- `src-tauri/src/covers.rs`: `cover://` thumbnails for the album grids.
- `src-tauri/src/cards.rs`: finding cards (removable FAT/exFAT volumes, or
  any volume holding `openpod/library.json`) and ejecting them.
- `src/api/`: the frontend's backend interface: `tauri.ts` (desktop),
  `web.ts` (browser: File System Access API, workers in `web-worker.ts`),
  `mock.ts` (sample data).
- `../crates/web`: `openpod-core` for the browser (wasm-bindgen). Workers
  read files synchronously (`FileReaderSync`) for probing, hashing,
  conversion and covers; a `Session` on the page plans and keeps the
  manifest. JavaScript walks folders and writes to the card.

## Index a folder

```sh
openpod index <music-folder> <card>/openpod
```

`<music-folder>` must have the same layout as the card: paths in
`path_index.bin` are relative to it.

| Option | Default | Meaning |
| --- | --- | --- |
| `--album-key` | `artist-album` | `artist-album` groups by album artist + album (folder when there is no album-artist tag), so two "Greatest Hits" stay separate. `name` is the legacy TS behaviour. |
| `--thumb-format` | `raw565` | `raw565` or `qoi` cover blobs |
| `--dither` | off | Bayer dither when snapping covers to RGB565 |

Tags are read in parallel (lofty), and covers are resized in parallel. On the
~950-track dev library a warm run takes 0.7 s, against 10.6 s for the TS
indexer.

## Differences from the TS indexer

`scripts/parity.sh <music-folder>` byte-compares both outputs with
`--album-key name`. All `.bin` files match except:

- `thumbs.bin`: Lanczos3 from the `image` crate vs sharp. Offsets, lengths,
  dedupe and `srcHash` match. Pixels differ by about 3.5/255 on average,
  which isn't visible.
- **Genre:** only `GENRE` is read. music-metadata also maps Vorbis `STYLE`
  onto genre, and the first one in the file wins, so "Roots Reggae" replaces
  "Reggae".
- **Some MP3s with a stale second ID3 block:** music-metadata returns
  unrelated tags for these, while lofty and ffprobe read the real ones.
- **MP3 durations:** come from an exact frame count (or the Xing/VBRI
  header), not the bitrate estimate.
