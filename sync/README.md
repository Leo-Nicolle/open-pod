# Sync scripts

Pipeline that copies the local music library onto the player's SD card,
converting FLAC files the VS1053b can't decode on the way.

## Why conversion is needed

The VS1053b decodes FLAC via a software plugin whose limits are (from
`datasheets/vs1053.pdf`, "Supported FLAC Formats"):

> Up to 48 kHz and 24-bit FLAC files are supported with the VS1053b Patches
> w/ FLAC Decoder plugin.

Hi-res FLAC — sample rate above 48 kHz (88.2/96/176.4/192 kHz), bit depth
above 24 (32-bit), or more than 2 channels — will not play. These are common,
so `sync.sh` re-encodes any such file to **48 kHz / 16-bit / stereo** while
preserving tags and embedded cover art. Files already inside the limits are
copied unchanged.

## Usage

```sh
./sync/sync.sh
```

Configuration is read from `sync/.env` (or the repo root `.env`), then the
environment. Copy `sync/.env.example` to `sync/.env` and edit it:

```sh
cp sync/.env.example sync/.env
```

| Variable            | Default              | Meaning                            |
| ------------------- | -------------------- | ---------------------------------- |
| `OPENPOD_LIBRARY`   | `~/Music`            | Source music library (required)    |
| `OPENPOD_SD`        | `/mnt/openpod`       | Mounted SD card                    |
| `OPENPOD_STAGING`   | `sync/staging`       | Persistent staging directory       |
| `OPENPOD_EXCLUDE`   | *(none)*             | `:`-separated paths to skip        |

Real environment variables take precedence over `.env` values.

## Excluding folders

List library-relative paths (one per line) in `sync/exclude.txt` — copy
`sync/exclude.txt.example` to start — or set `OPENPOD_EXCLUDE` in `.env`.
Anything under an excluded path is skipped, and on the next sync it is also
removed from the SD card.

```
# sync/exclude.txt
Podcasts
Various Artists/Bootlegs
audiobooks
```

The library is never modified: a persistent `sync/staging/` mirror holds the
library plus converted files, and the index is built over it. Because rsync
compares on every run, only changed or newly-converted files are copied to the
SD card.

## Dependencies

- `rsync`, `ffmpeg` (provides `ffprobe`) — `sudo apt install rsync ffmpeg`
- `indexer/` deps — `npm install` in `indexer/` (as before)

## Pipeline

1. Mirror the library (excluding FLAC) into staging (`rsync --delete`).
2. Copy/convert each FLAC (incremental via mtime; incompatible files are
   re-encoded, others copied), and drop stale FLACs no longer in the library.
3. Build the `openpod/` index over staging (`indexer/generate.ts`).
4. `rsync --delete` staging onto the SD card, then `sync`.

## Conversion audit log

Every conversion is appended to `sync/conversions.log` as a one-line record:

```
2026-09-20T12:34:56Z | converted | /home/leo/Music/.../track.flac | 96000/24/2 -> 48000/16/2
```

The staging directory and the log are gitignored.
