#!/usr/bin/env bash
set -euo pipefail

# parity.sh <music-dir> — runs the TS indexer (indexer/) and the Rust one
# (`openpod index --album-key name`) on the same folder and byte-compares
# every .bin. thumbs.bin is expected to differ slightly: sharp and the
# `image` crate resample differently (offsets/lengths/dedupe still match, see
# album_to_cover.bin).
#
# Known intentional differences on real libraries:
#   - music-metadata maps Vorbis STYLE onto genre (first in file order wins);
#     the Rust indexer only reads GENRE.
#   - music-metadata mis-reads some MP3s carrying a stale second ID3 block;
#     lofty/ffprobe read the real tags.

MUSIC="${1:?usage: parity.sh <music-dir>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
mkdir -p "$OUT/ts" "$OUT/rs"

(cd "$ROOT/indexer" && npm run --silent generate -- "$MUSIC" "$OUT/ts" >/dev/null)
(cd "$ROOT/syncer" && cargo run --release -q -- index "$MUSIC" "$OUT/rs" --album-key name)

status=0
for f in "$OUT"/ts/*.bin; do
  b="$(basename "$f")"
  if cmp -s "$f" "$OUT/rs/$b"; then
    echo "same  $b"
  else
    echo "DIFF  $b"
    [ "$b" = thumbs.bin ] || status=1
  fi
done
exit $status
