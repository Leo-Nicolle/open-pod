# Podcasts and audiobooks: tagging, indexing and the on-device tree

## Context

TODO section "Podcasts and audiobooks" asks for four things:

- a script to normalize podcast ID3 tags (the role beets plays for music),
- the indexer to crawl them,
- device submenus for a **variable-depth** tree
  (`France-Culture / Les Pieds sur Terre`, `France-Culture / La série Fiction / Le projet Horloff`,
  `L'équipe / L'équipe du tour`),
- saved listened state + resume timestamp, with a mark read/unread shortcut.

Samples live in `~/Podcasts/`. Nothing podcast-specific exists in the repo yet: the
syncer only *excludes* `Podcasts` by default (`syncer/crates/core/src/config.rs:7`,
`syncer/app/src/api/mock.ts:60`), and `docs/index.md:127` lists them as planned.

## What the samples actually contain

Two generations of files, and their tags disagree:

| Field | Old MP3 (`CheminsdelaPhilo2017/*`, from archive.org) | New M4A (`Podcasts/*.m4a`, app/aggregator) |
|---|---|---|
| `album` | **Show** — "Les chemins de la philosophie" | a slate ("Les émissions de savoir de France Culture") or a series ("[ATLAS] LA SERIE FICTION") |
| `artist` | Presenter ("Adèle Van Reeth") | Presenter **or** "Radio France" (inconsistent) |
| `title` | Episode | Episode (e.g. "Debout les morts de Fred Vargas 10/10") |
| station | absent | absent |
| series | inside title `(1/4)` only | sometimes in `album` |
| date | `TIT3`/`TDAT` = `06.04.2018` | absent |
| extras | `genre=Podcast`, `TCAT` (iTunes cat), `TGID` (source feed URL), `TDES` (desc) | `comment` (desc) |

The old MP3s already carry the standard **podcast ID3 frames** (`TGID`, `TDES`,
`TCAT`, `TIT3`); the spec's `PCST`, `WFED`, `TKWD` and the series frames
`MVNM`/`MVIN` are unset. Conclusion: show + episode + title are present, **station is
missing**, series is inconsistent/absent as a field, there is no season/episode
numbering. Not a clean fit for the wanted tree as-is.

Filename encodes the join key: `10467-06.04.2018-ITEMA_21639733-1.mp3` → feed id
`10467`, date `06.04.2018`. New files: `23118-12.10.2026-…m4a`.

## Findings (the five questions)

### 1. Do the tags match? No — see the table above.

### 2. Auto-tagging / MusicBrainz for podcasts?

- **MusicBrainz has no podcast support** — music only, no podcast entity. No MB
  tagger for speech.
- No speech fingerprinting service either (AcoustID/MB don't cover it).
- But the files **point at their source**: `TGID` is the feed URL and the filename
  encodes feed id + date. Joining a file to its episode in the live RSS feed is
  deterministic and needs no fingerprint. That is the auto-tagger.
- For non-RF podcasts: **Podcast Index** (podcastindex.org, free API keys) searches
  by feed URL/title and returns feed + episode metadata.

### 3. Radio France API? Yes, two routes.

- **Public RSS, no auth** (verified live):
  - legacy `https://media.radiofrance-podcast.net/podcast09/rss_<feedid>.xml`.
    `rss_10467.xml` → channel "LES NOUVEAUX CHEMINS DE LA CONNAISSANCE", **317 items**,
    `lastBuildDate` 2016-02-10.
  - modern per-podcast `https://radiofrance-podcast.net/podcast09/podcast_<uuid>.xml`,
    advertised in `itunes:new-feed-url` and in the page HTML. `podcast_b073d78c-…xml`
    → "Avec philosophie", 42 items; each item has `guid`, `itunes:duration`,
    `itunes:subtitle/summary/keywords`, enclosure, `itunes:image`.
  - Images: `https://www.radiofrance.fr/pikapi/images/<uuid>/2048`.
  - Items also carry the RF namespace `podcastRF:` (`magnetothequeID`, `stepID`,
    `businessReference`).
- **Open API GraphQL** `https://openapi.radiofrance.fr/v1/graphql` — exists but needs
  a registered token (`403 Unauthorized: Missing x-token`; register at
  developers.radiofrance.fr). Richer station/show/episode/search if we want it.

### 4. Soulseek / a repo for old content?

**Current episodes come from Radio France itself** — see "Acquisition" below. For the
**back-catalog** (episodes RF prunes), **archive.org is the repo**, and it's already being
used (`~/Podcasts/urls.txt`, `wget-log`). Community "France Culture" uploads: **828 audio
items**, including
whole-year per-show collections ("Les chemins de la philosophie 2017/2018/2019/2021",
"Le cours de l'Histoire 2021", "La méthode scientifique 2021", "Carbone 14 2021",
"Eureka 2021", "La Série Documentaire 2021", …). They preserve the original RF ID3
tags. Scriptable via `/advancedsearch.php` and `/metadata/<id>`. Verified identifiers:
`CheminsdelaPhilo2017` (665 files), `LesCheminsdelaPhilosophie2018` (975),
`cheminsphilosophie2019` (666), `les-chemins-de-la-philosophie-2021` (551).

Soulseek has some RF material but it's unindexed and hit-or-miss. Radio France itself
prunes: the old show's feed stopped building in 2016.

### Acquisition: the Radio France userscript

Primary path (current episodes):
`docs/public/userscripts/radiofrance-podcasts.user.js`, served at
`https://leo-nicolle.github.io/open-pod/userscripts/radiofrance-podcasts.user.js`.

Verified against the live site:

- Every podcast page carries its feed:
  `<link rel="alternate" type="application/rss+xml" href="https://radiofrance-podcast.net/podcast09/podcast_<uuid>.xml">`
  — so the episode list needs no SPA scraping.
- The feed is **CORS-open** (`Access-Control-Allow-Origin: *`), so `fetch` reads it. The
  audio **enclosure** 302 (`proxycast.radiofrance.fr`) carries **no** CORS header, so the
  file is pulled with `GM_xmlhttpRequest` (bypasses CORS, follows the redirect); the redirect
  target `media.radiofrance-podcast.net` is CORS-open and supports `Accept-Ranges` / `206`.
- The enclosure `302` → `media.radiofrance-podcast.net/podcast09/<RF filename>.<ext>`; the
  served files are **already tagged** (the two `~/Podcasts/*.m4a` samples are exactly these),
  so no in-browser tag writing is needed. Normalisation stays offline (P1).
- Station comes from the page: `<link rel="canonical" href="…/<station>/podcasts/…">` plus
  `og:site_name`.

Behaviour: an "OpenPod" logo sits in the corner (collapsed by default; click it to open the
episode list — checkbox list, selected rows highlighted, new-only by default). It remembers the
chosen folder and writes `<Station>/<Show>/<YYYY-MM-DD> - <Title>.<ext>`. Each row shows a
spinner while downloading, then a tick or cross; a finished download is dropped from the
selection (downloads never re-tick the list). A **Verify** button checks each on-disk file's
size against Radio France's (`HEAD`, or a one-byte `Range` fallback) and flags mismatches;
downloads are written only after the blob is complete, and a failed transfer leaves no file
behind (so nothing is marked "on disk" that isn't). Chrome/Edge only for the folder path
(File System Access API); Firefox/Safari fall back to a normal download. `GM_xmlhttpRequest`
is only a fallback for the CORS-open endpoints.

Then OpenPod Sync (the browser app) reads `~/Podcasts` as a library and writes the card. The
userscript's origin (`radiofrance.fr`) and the app's (`leo-nicolle.github.io`) hold **separate**
folder permissions — File System Access handles are not shared across origins — so `~/Podcasts`
is picked once in each.

### 5. Can beets do the tree? Not well.

Beets *can* write nested trees and even query-conditional paths
(`paths: {'albumtype:soundtrack': …}`) and `%if{}`/`%first{}` inline, plus custom
fields via the `inline` plugin (installed 2.14.1, `~/.config/beets/config.yaml`). But
it has **no variable-depth concept** and models everything as album-or-singleton with
one template per item; its autotagger would mis-match speech and mangle tags. Using it
means disabling autotag, importing as singletons and encoding depth in a computed
field — fighting the tool. There is no mainstream "beets for podcasts".

The right home is the existing Rust `syncer` (`card_path`, `syncer/crates/core/src/paths.rs:29`,
currently fixed `AlbumArtist/Album/NN Title`) plus a **firmware** feature, since the
device is a fixed Artist→Album→Track model today.

## Design decisions (agreed)

1. **Tree** = real variable-depth folder tree on the card + a new firmware browser
   (not tag-mapping into the existing menus, though tag-mapping is kept as a fallback
   so episodes remain playable/searchable through the current menus).
2. **Enrichment** = Radio France RSS (primary) + Podcast Index (fallback for non-RF).
3. **State** = on the SD card, as a **binary** file indexed like the rest of the index
   (no JSON — the MCU would struggle; these are just numbers).
4. **Acquisition** = a Tampermonkey userscript on RF pages (primary, current episodes);
   archive.org for the back-catalog RF prunes. Fetched files keep RF's own tags.

## Index format constraints (reuse, don't invent)

All tables are little-endian, one `<name>.bin` per lookup, loaded into PSRAM and
binary-searched (`syncer/crates/core/src/index/binary.rs`, firmware
`software/src/storage/Music_lookup.h:177`, `:192`):

- `string_index` — `[u32 count][u32 bytes][u32 id]*count [u32 offset]*(count+1) [NUL UTF-8]`
- `relation` — `[u32 sources][u32 targets][u32 src]* [u32 count]* [u32 target]*`, ascending
- `dense_u32` — `[u32 count][u32 value]*count`, one value per dense id
- `durations` — `[u32 count][u16 secs]*count`

Crucial: **track ids are reassigned alphabetically on every build**
(`syncer/crates/core/src/index/model.rs:135` `build` + `alphabetical_remap`), so
playback state must be keyed by a stable value (path/hash/guid), never by track id.

## Plan

### P1 — `podcast-tagger` (the "beets for podcasts")

Separate script (Python + `mutagen`, or `openpod tag-podcasts`) so the syncer stays
offline/pure-Rust (`plans/sync-tool.md`). Writes normalized tags into the files.

Normalized fields and where they come from:

- `station` ← curated show→station map ("France-Culture", "L'équipe"); fallback
  `itunes:author` / page URL. RF's show list is bounded, so a small map is fine.
- `show` ← feed `<title>` (authoritative; fixes the wrong M4A `album`).
- `series` ← **optional**: title `(n/m)` grouping, feed series, or archive.org
  collection nesting. This optional field is what makes the depth vary.
- `album` ← `show`, `artist` ← `station` (keeps the existing player/index working).
- `episode_no`, `season` (feed / `N/M`), `date` (`TDRC`), `TDES`, `TCAT`,
  `WFED`/`TGID`, cover art.
- Join key: `TGID` feed URL when present; otherwise the userscript's
  `<Station>/<Show>/<date>` layout joined to the show's feed + `pubDate`. (The old MP3
  filename encoded feed id + date, but the new generation's numeric prefix is not the
  feed id, and the layout drops the RF filename — hence show+date, not filename.) If that
  proves lossy, add a one-line `_source.txt` (feed URL) per Show folder.
- Idempotent, `--dry-run`, reports unresolved files (→ `Unknown`).

### P2 — Syncer podcast layout

- Add a podcast layout to `card_path` (`syncer/crates/core/src/paths.rs:29`):
  `Station/Show/[Series]/NN Episode.ext`; depth depends on whether `series` is set.
- Extend `Tags`/`read_tags` (`syncer/crates/core/src/tags.rs:116`) with `series`,
  `station`, `episode_no`.

### P3 — Podcast tree index

Emit from `build_index` (`syncer/crates/core/src/index/mod.rs:52`) via
`binary::all()` (`binary.rs:75`):

- `podcast_folder_index.bin` — folder display names (`string_index`)
- `podcast_node_parent.bin` — parent folder id, `0xFFFFFFFF` = root (`dense_u32`)
- `podcast_node_kind.bin` — 0 folder / 1 episode (`dense_u8`, new small helper)
- episode nodes carry the existing **track id**, so playback/paths/durations are reused.

Keep the TS indexer in parity (`indexer/src/crawler.ts:194`, `indexer/src/music-indexes.ts`).

### P4 — Firmware browser

- Add a `PODCAST` route to `Route::RouteType` (`software/src/storage/Music_lookup.h:71`)
  and accessors for the three tables.
- `StaticMenu::createRootMenu` (`software/src/state/static_menu.cpp:91`): add "Podcasts".
- New cases in `loadCurrentRouteData:8`, `goToSelected:131`, `getSelectedEntityId:416`,
  `resolveTrackId:478` (`software/src/state/state_navigation.cpp`) doing a **generic
  folder walk** (children of `entityId`), so depth is whatever the tree is.
- Episodes resolve to a track id → normal player path.

### P5 — Playback state (binary, SD-side)

- `openpod/podcast_state.bin`, **user data — never regenerated by sync** (like
  `library.json`): header `[magic][version][count]`, then entries keyed by a **u32 xxh3
  of the card path** (stable across index rebuilds), sorted for binary search:
  `[u32 pathHash][u16 position_secs][u8 flags]` (bit0 = played).
- Firmware: load into RAM, look up by hashing the playing file's path, update on
  pause/stop/exit, flush throttled. Long-press on the player = mark read/unread
  (hook `software/src/app/input.cpp`, `ui/ui_engine_events.hpp:170`).

### P6 — Radio France userscript (done)

`docs/public/userscripts/radiofrance-podcasts.user.js` (+ a Tools card on the docs site).
See "Acquisition" above. A back-catalog helper that mirrors archive.org collections by
identifier (`advancedsearch.php` + `metadata/<id>`) stays optional and not yet written.

## Risks / open items

- **Station is not in the old tags**; curated show→station map covers RF, else
  "Radio France".
- **Series detection is heuristic**; unresolvable → 2-level tree. Acceptable because
  depth is optional.
- **State key must be a path hash**, not a track id (ids re-sort every build).
- **Enrichment is online** and must run before sync; the syncer itself stays offline.
- Podcast episodes also keep the tag mapping (station→artist, show→album) so they are
  playable/searchable from the current menus even before P4 lands.

Order: P6 (userscript, done) → P1 → P2 → P3 → P4 → P5; archive.org back-catalog helper optional.

## Implemented (2026-10-12)

What shipped differs from P2–P5 above; this section wins where they disagree.
UI: `.agents/openpod-podcasts-design/` (publisher → show → [series] → episodes,
state marks, resume, hold-Center actions, end-of-episode behaviour).

- **Card flow (replaces P2):** no tag-based layout. The `podcasts` folder
  (config key / `--podcasts`) is *mirrored* to `<card>/Podcasts/` as laid out,
  any depth: copies new episodes, deletes the ones gone from the folder,
  renames on the card the ones that only moved (same content hash). Kept in
  `openpod/podcasts.json`, apart from the music manifest
  (`syncer/crates/core/src/podcasts/`). Folder names are the display names, so
  the tree is whatever the folders say; P1's tagger is still open.
- **Tables (replaces P3):** `openpod/podcast_*.bin`, ids in display order
  (depth-first; subfolders alphabetical; a series folder's episodes by number,
  others newest first), so each folder's episodes are one contiguous id range.
  Series = every title starts with `N/M`; the prefix is stripped into
  `podcast_episode_number.bin`. Date: tags (TDRC, else TIT3 "Émission du
  16.10.2017"), else `-DD.MM.YYYY-` in the file name, else mtime.
  `podcast_meta.bin` carries a generation.
- **State (replaces P5):** `podcast_state.bin`, one 20-byte record per episode
  id (`u64 key | u32 position | u32 last_played | u32 flags`), so the firmware
  reads and writes in place. The key is the source's content hash (size + first
  and last 64 KiB: an intro jingle only matches the first part); every rebuild
  remaps the records to the new ids by key, so state survives renames and
  moves. Generation mismatch (interrupted sync) → the firmware leaves it alone.
- **Firmware (P4):** `state/podcast_{source,state,menu}`, `ui/podcast_view.hpp`,
  routes `PODCASTS` / `PODCAST_FOLDER` / `PODCAST_ACTIONS`; episodes play as
  track ids with `PODCAST_TRACK_BIT`. Saves on pause, track change and every
  30 s; there is no power-off hook yet, so the last ≤30 s can be lost on a
  hard power cut.
- **Keep latest N (app):** `podcasts.json` `keep` maps a show (the folder
  holding episodes, `show_key`) to how many of its newest episodes stay on
  the card; unlisted = all. `mirror::finish` ranks on-card and new episodes
  together by date: older ones on the card are deleted (`plan.leaving`),
  older new ones aren't copied (`plan.held`). The app's "On the card ›
  Podcasts" tab (design: `.agents/podcasts-sync-design/`) lists shows with
  the played/position state read from `podcast_state.bin`; RSS feeds from
  that design are not implemented.
- Not done: the podcast tagger (P1), fetching (P6), the browser can't convert
  M4A/AAC (no ffmpeg), so the web app skips them.
