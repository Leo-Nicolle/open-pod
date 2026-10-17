#!/usr/bin/env sh
# Registers a desktop entry for the dev build so GNOME/Wayland shows the app
# icon in the dock and Alt-Tab. Wayland compositors ignore per-window icons and
# match the window's app_id (the binary name, `opentone-sync`) to a .desktop
# file instead. Installed bundles (.deb/.rpm/AppImage) ship their own entry.
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
icon="$here/../app/src-tauri/icons/icon.png"
bin="$here/../target/debug/opentone-sync"
dest="${XDG_DATA_HOME:-$HOME/.local/share}/applications/opentone-sync.desktop"

mkdir -p "$(dirname "$dest")"
cat > "$dest" <<EOF
[Desktop Entry]
Type=Application
Name=opentone Sync (dev)
Exec=$(realpath "$bin")
Icon=$(realpath "$icon")
StartupWMClass=opentone-sync
Terminal=false
Categories=AudioVideo;Audio;
EOF

update-desktop-database "$(dirname "$dest")" 2>/dev/null || true
echo "Installed $dest"
