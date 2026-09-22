#!/usr/bin/env bash
set -euo pipefail

# sync.sh — sync the local music library onto the player's SD card, converting
# FLAC files the VS1053b cannot decode (sample rate > 48 kHz, depth > 24-bit,
# or > 2 channels) into readable 48 kHz / 16-bit / stereo FLAC on the way.
#
# The library is never modified. A persistent staging directory holds the
# library plus its converted files, so rsync only re-copies what changed.
#
# Config (env vars, or set them in sync/.env — see sync/.env.example):
#   OPENPOD_LIBRARY   source music library          (default: ~/Music)
#   OPENPOD_SD        mounted SD card               (default: /mnt/openpod)
#   OPENPOD_STAGING   persistent staging directory  (default: sync/staging)
#   OPENPOD_EXCLUDE   ':'-separated library-relative paths to skip
#                     (also sync/exclude.txt, one path per line)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- load .env (sync/.env first, then repo root .env) ---
# KEY=VALUE lines only; '#' starts a full-line comment; values with spaces must
# be quoted. Variables already set in the real environment win over .env.
load_env_file() {
  local f="$1" line key val
  [ -f "$f" ] || return 0
  while IFS= read -r line || [ -n "$line" ]; do
    line="$(printf '%s' "$line" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
    case "$line" in ''|'#'*) continue ;; esac
    case "$line" in *=*) ;; *) continue ;; esac
    key="${line%%=*}"
    val="${line#*=}"
    key="$(printf '%s' "$key" | sed 's/[[:space:]]*$//')"
    val="$(printf '%s' "$val" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
    case "$val" in
      \"*\") val="${val#\"}"; val="${val%\"}" ;;
      \'*\') val="${val#\'}"; val="${val%\'}" ;;
    esac
    if [ -n "$key" ] && [ -z "${!key+x}" ]; then
      export "$key=$val"
    fi
  done < "$f"
}
load_env_file "$SCRIPT_DIR/.env"
load_env_file "$REPO_ROOT/.env"

LIB="${OPENPOD_LIBRARY:-$HOME/Music}"
LIB="${LIB%/}"
SD="${OPENPOD_SD:-/mnt/openpod}"
STAGING="${OPENPOD_STAGING:-$REPO_ROOT/sync/staging}"
STAGING="${STAGING%/}"
LOG="$REPO_ROOT/sync/conversions.log"
INDEXER_DIR="$REPO_ROOT/indexer"
CONVERT="$SCRIPT_DIR/convert-flac.sh"

# --- exclusion list: OPENPOD_EXCLUDE (':' separated) + sync/exclude.txt ---
EXCLUDE_PATHS=()
if [ -n "${OPENPOD_EXCLUDE:-}" ]; then
  IFS=':' read -r -a _ex <<< "$OPENPOD_EXCLUDE"
  for e in "${_ex[@]}"; do
    e="${e#/}"; e="${e%/}"
    [ -n "$e" ] && EXCLUDE_PATHS+=("$e")
  done
fi
if [ -f "$SCRIPT_DIR/exclude.txt" ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%%#*}"
    line="$(printf '%s' "$line" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
    line="${line#/}"; line="${line%/}"
    [ -n "$line" ] && EXCLUDE_PATHS+=("$line")
  done < "$SCRIPT_DIR/exclude.txt"
fi

# is_excluded <rel> -> 0 if the library-relative path (or anything under an
# excluded folder) is excluded.
is_excluded() {
  local rel="$1" p
  for p in "${EXCLUDE_PATHS[@]}"; do
    if [ "$rel" = "$p" ] || [ "${rel#"$p"/}" != "$rel" ]; then
      return 0
    fi
  done
  return 1
}

# --- sanity checks ---
for cmd in rsync ffmpeg ffprobe; do
  command -v "$cmd" >/dev/null 2>&1 || { echo "error: '$cmd' not found" >&2; exit 1; }
done

[ -d "$LIB" ] || { echo "error: library not found: $LIB (set OPENPOD_LIBRARY)" >&2; exit 1; }
[ -d "$INDEXER_DIR/node_modules" ] || { echo "error: run 'npm install' in $INDEXER_DIR" >&2; exit 1; }

if ! mountpoint -q "$SD"; then
  echo "error: nothing is mounted at $SD" >&2
  echo "Mount the SD card first (uses your fstab entry if present):" >&2
  echo "  sudo mount $SD" >&2
  exit 1
fi
if [ ! -w "$SD" ]; then
  echo "error: no write permission on $SD (owned by root)" >&2
  echo "Remount with your user id:" >&2
  echo "  sudo mount -o remount,uid=\$(id -u),gid=\$(id -g) $SD" >&2
  echo "(or add uid/gid to the fstab entry)" >&2
  exit 1
fi

mkdir -p "$STAGING" "$REPO_ROOT/sync"

# Drop excluded paths from staging first (handles a folder that *was* synced
# and has since been added to the exclusion list — rsync --exclude alone would
# leave its old contents behind).
for p in "${EXCLUDE_PATHS[@]}"; do
  case "/$p/" in
    */../*) echo "warning: ignoring unsafe exclude path '$p'" >&2; continue ;;
  esac
  if [ -e "$STAGING/$p" ]; then
    echo "  excluding $p"
    rm -rf "$STAGING/$p"
  fi
done

# --- 1. mirror the library (non-FLAC) into staging, incrementally ---
echo "==> [1/4] Mirroring library (non-FLAC) into staging"
RSYNC_EXCLUDES=(-a --delete --exclude='*.flac' --exclude='*.fla' --exclude='*.FLAC' --exclude='*.FLA' --exclude='openpod/')
for p in "${EXCLUDE_PATHS[@]}"; do
  RSYNC_EXCLUDES+=(--exclude="/$p")
done
rsync "${RSYNC_EXCLUDES[@]}" "$LIB/" "$STAGING/"

# --- 2. copy/convert FLAC files (incremental, idempotent) ---
# Progress bar is drawn only when stdout is a terminal; otherwise we stay
# quiet per-file and report a summary at the end.
BAR_ONLY_TTY=0; [ -t 1 ] && BAR_ONLY_TTY=1

draw_flac_progress() {
  local done=$1 total=$2 conv=$3 width=40 filled pct i bar="" pad="" note=""
  if [ "$BAR_ONLY_TTY" -ne 1 ]; then return; fi
  pct=$((done * 100 / total))
  filled=$((done * width / total))
  for ((i = 0; i < filled; i++)); do bar+="="; done
  for ((i = filled; i < width; i++)); do pad+=" "; done
  [ "$conv" -gt 0 ] && note=" ($conv converted)"
  printf "\r[%s>%s] %3d%% %d/%d%s  " "$bar" "$pad" "$pct" "$done" "$total" "$note"
}

flac_files=()
while IFS= read -r -d '' src; do
  rel="${src#"$LIB"/}"
  is_excluded "$rel" || flac_files+=("$src")
done < <(find "$LIB" -type f \( -iname '*.flac' -o -iname '*.fla' \) -print0)

total=${#flac_files[@]}
done=0
converted=0
converted_list=()

echo "==> [2/4] Syncing FLAC files ($total total)"
for src in "${flac_files[@]}"; do
  rel="${src#"$LIB"/}"
  dst="$STAGING/$rel"
  if [ ! -e "$dst" ] || [ "$src" -nt "$dst" ]; then
    out=""
    if ! out="$("$CONVERT" "$src" "$dst" "$LOG")"; then
      echo "warning: failed to sync '$rel' — continuing" >&2
    fi
    if [ "$out" = "converted" ]; then
      converted=$((converted + 1))
      converted_list+=("$rel")
    fi
  fi
  done=$((done + 1))
  draw_flac_progress "$done" "$total" "$converted"
done

if [ "$BAR_ONLY_TTY" -eq 1 ]; then printf '\n'; fi

if [ "$converted" -gt 0 ]; then
  echo "Converted $converted FLAC file(s):"
  for r in "${converted_list[@]}"; do
    printf '  - %s\n' "$r"
  done
fi

# Remove staging FLACs whose source no longer exists (parity with --delete).
while IFS= read -r -d '' dst; do
  rel="${dst#"$STAGING"/}"
  if [ ! -f "$LIB/$rel" ] || is_excluded "$rel"; then
    echo "  rm $rel"
    rm -f "$dst"
  fi
done < <(find "$STAGING" -type f \( -iname '*.flac' -o -iname '*.fla' \) -print0)

# --- 3. build the openpod index over staging ---
echo "==> [3/4] Building openpod index"
mkdir -p "$STAGING/openpod"
(cd "$INDEXER_DIR" && npm run generate -- "$STAGING" "$STAGING/openpod")

# --- 4. sync staging to the SD card ---
echo "==> [4/4] Syncing to SD card: $SD"
# exFAT has no POSIX ownership/permissions, so don't try to preserve them
# (rsync -a would emit chown/chgrp errors on the card).
rsync -rt --delete --no-owner --no-group --no-perms --modify-window=1 \
  "$STAGING/" "$SD/"
sync

echo "==> Done. You can now safely eject $SD."
