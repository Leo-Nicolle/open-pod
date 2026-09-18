#!/usr/bin/env bash
set -euo pipefail

# Sync the local sd-card/ mirror to the SD card used for on-device testing.
#
# 1. Runs the indexer over sd-card/ (the music) and writes the generated
#    *.bin files into sd-card/openpod/ (where the firmware loads them from).
# 2. rsyncs sd-card/ onto the SD card mounted at /mnt/openpod.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SD_CARD_DIR="${SD_CARD_DIR:-$REPO_ROOT/sd-card}"
INDEXER_DIR="$REPO_ROOT/indexer"
OPENPOD_DIR="$SD_CARD_DIR/openpod"
MEDIA_DIR="${1:-${OPENPOD_MEDIA:-/mnt/openpod}}"

# --- sanity checks ---
if [ ! -d "$SD_CARD_DIR" ]; then
  echo "error: sd-card directory not found: $SD_CARD_DIR" >&2
  exit 1
fi

if [ ! -d "$INDEXER_DIR/node_modules" ]; then
  echo "error: indexer dependencies not installed; run 'npm install' in $INDEXER_DIR" >&2
  exit 1
fi

if [ ! -d "$MEDIA_DIR" ]; then
  echo "error: SD card not mounted at: $MEDIA_DIR" >&2
  exit 1
fi

if [ ! -w "$MEDIA_DIR" ]; then
  echo "error: no write permission on $MEDIA_DIR (it is owned by root)" >&2
  echo "" >&2
  echo "The exFAT card is mounted without uid/gid, so only root can write." >&2
  echo "Fix it once by remounting with your user id:" >&2
  echo "  sudo umount $MEDIA_DIR" >&2
  echo "  sudo mount -o uid=\$(id -u),gid=\$(id -g) $MEDIA_DIR" >&2
  echo "  (or add uid/gid to the fstab entry for a permanent fix)" >&2
  exit 1
fi

# --- 1. index the music ---
echo "==> Indexing music in $SD_CARD_DIR"
mkdir -p "$OPENPOD_DIR"
(cd "$INDEXER_DIR" && npm run generate -- "$SD_CARD_DIR" "$OPENPOD_DIR")

# --- 2. sync to the SD card ---
echo "==> Syncing to $MEDIA_DIR"
rsync -avh --delete "$SD_CARD_DIR/" "$MEDIA_DIR/"
sync

echo "==> Done. You can now safely eject $MEDIA_DIR."
