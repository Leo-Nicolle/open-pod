#!/usr/bin/env bash
set -euo pipefail

# convert-flac.sh <src> <dst> <logfile>
#
# Ensures a single FLAC file is decodable by the VS1053b FLAC plugin, whose
# limits are 48 kHz sample rate, 24-bit depth, and 2 channels (see
# sync/README.md and datasheets/vs1053.pdf). Files inside those limits are
# copied verbatim (mtime preserved). Files outside them are re-encoded to
# 48 kHz / 16-bit / stereo, preserving tags and embedded cover art. Every
# conversion is appended to <logfile> as a one-line audit record.

SRC="${1:?usage: convert-flac.sh <src> <dst> <logfile>}"
DST="${2:?}"
LOG="${3:?}"

TARGET_RATE=48000
TARGET_BITS=16
TARGET_CHANNELS=2

# Read one field from the first audio stream (key=value output, so we are
# immune to ffprobe's internal field ordering).
meta() {
  ffprobe -v error -select_streams a:0 \
    -show_entries "stream=$1" \
    -of default=noprint_wrappers=1 "$SRC" 2>/dev/null \
    | sed -n 's/^[^=]*=//p' | head -n1
}

RATE=$(meta sample_rate)
BITS_RAW=$(meta bits_per_raw_sample)
BITS=$(meta bits_per_sample)
CH=$(meta channels)
FMT=$(meta sample_fmt)

# numeric <value> -> first numeric token of <value>, or 0. Always prints a
# number so callers can compare with -gt without tripping set -e.
numeric() {
  local v="$1"
  case "$v" in
    ''|'N/A'|'n/a') printf 0; return ;;
    *[!0-9]*) v=$(printf '%s' "$v" | grep -oE '[0-9]+' | head -n1); printf '%s' "${v:-0}" ;;
    *) printf '%s' "$v" ;;
  esac
}

RATE_N=$(numeric "$RATE")
CH_N=$(numeric "$CH")

# Effective bit depth: bits_per_raw_sample, then bits_per_sample, then
# inferred from sample_fmt.
depth() {
  local raw bits
  raw=$(numeric "$BITS_RAW")
  bits=$(numeric "$BITS")
  if [ "$raw" -gt 0 ]; then printf '%s' "$raw"; return; fi
  if [ "$bits" -gt 0 ]; then printf '%s' "$bits"; return; fi
  case "$FMT" in
    s16|s16p) printf 16 ;;
    s24|s24p) printf 24 ;;
    s32|s32p|flt|fltp) printf 32 ;;
    *) printf 16 ;;
  esac
}
DEPTH_N=$(depth)

needs_conv=0
if [ "$RATE_N" -gt "$TARGET_RATE" ]; then needs_conv=1; fi
if [ "$DEPTH_N" -gt 24 ]; then needs_conv=1; fi
if [ "$CH_N" -gt "$TARGET_CHANNELS" ]; then needs_conv=1; fi

# Unprobeable (corrupt / non-audio) file: leave it untouched rather than
# risk destroying it with a failed transcode.
if [ "$RATE_N" -eq 0 ]; then
  echo "warning: cannot probe '$SRC' — copying unchanged" >&2
  needs_conv=0
fi

mkdir -p "$(dirname "$DST")"

if [ "$needs_conv" -eq 0 ]; then
  cp -p "$SRC" "$DST"
  echo "copied"
  exit 0
fi

TMP="${DST}.tmp.$$.flac"
trap 'rm -f "$TMP"' EXIT

ffmpeg -nostdin -y -loglevel error \
  -i "$SRC" \
  -map 0:a:0 -map "0:v:0?" \
  -map_metadata 0 \
  -c:v copy -disposition:v:0 attached_pic \
  -af "aresample=${TARGET_RATE}" -ac "${TARGET_CHANNELS}" \
  -sample_fmt s16 -c:a flac -compression_level 8 \
  "$TMP"
mv "$TMP" "$DST"
trap - EXIT

printf '%s | converted | %s | %s/%s/%s -> %s/%s/%s\n' \
  "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  "$SRC" \
  "$RATE_N" "$DEPTH_N" "$CH_N" \
  "$TARGET_RATE" "$TARGET_BITS" "$TARGET_CHANNELS" \
  >> "$LOG"

echo "converted"
