#!/usr/bin/env bash
set -Eeuo pipefail

APP_NAME="Sonora"
APP_DIR="/usr/local/lib/sonora"
BIN_FILE="/usr/local/bin/sonora"
ICON_FILE="/usr/share/pixmaps/sonora.svg"
DESKTOP_FILE="/usr/share/applications/sonora.desktop"

run_root() {
    if (( EUID == 0 )); then
        "$@"
    else
        sudo "$@"
    fi
}

echo "================================"
echo "      Uninstalling Sonora"
echo "================================"

if (( EUID != 0 )) && ! command -v sudo >/dev/null 2>&1; then
    echo "Error: sudo is required to uninstall Sonora." >&2
    exit 1
fi

if [[ ! -e "$APP_DIR" &&
      ! -e "$BIN_FILE" &&
      ! -e "$ICON_FILE" &&
      ! -e "$DESKTOP_FILE" ]]; then
    echo "Sonora does not appear to be installed."
    exit 0
fi

echo "Removing application files..."

run_root rm -f \
    "$BIN_FILE" \
    "$ICON_FILE" \
    "$DESKTOP_FILE"

run_root rm -rf -- "$APP_DIR"

if command -v update-desktop-database >/dev/null 2>&1; then
    run_root update-desktop-database /usr/share/applications || {
        echo "Warning: could not update the desktop database."
    }
fi

echo
echo "================================"
echo " Sonora was uninstalled."
echo "================================"
echo
echo "The following were not removed:"
echo "  - Your music files"
echo "  - Your personal files and folders"
echo "  - Shared system dependencies (SDL2, TagLib, Zenity, etc.)"