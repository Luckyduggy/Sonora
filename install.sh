#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

APP_NAME="Sonora"
APP_DIR="/usr/local/lib/sonora"
BIN_DIR="/usr/local/bin"
ICON_SOURCE="$PROJECT_DIR/mp3player.svg"
ICON_DEST="/usr/share/pixmaps/sonora.svg"
DESKTOP_FILE="/usr/share/applications/sonora.desktop"

run_root() {
    if (( EUID == 0 )); then
        "$@"
    else
        sudo "$@"
    fi
}

install_dependencies() {
    echo "Installing dependencies..."

    if command -v apt-get >/dev/null 2>&1; then
        run_root apt-get update

        if apt-cache show libtag-c-dev >/dev/null 2>&1; then
            TAGLIB_PACKAGE="libtag-c-dev"
        elif apt-cache show libtagc0-dev >/dev/null 2>&1; then
            TAGLIB_PACKAGE="libtagc0-dev"
        else
            echo "Error: TagLib C development package not found." >&2
            exit 1
        fi

        run_root apt-get install -y \
            build-essential \
            pkg-config \
            libsdl2-dev \
            libsdl2-mixer-dev \
            libsdl2-ttf-dev \
            "$TAGLIB_PACKAGE" \
            zenity \
            xdg-user-dirs

    elif command -v dnf >/dev/null 2>&1; then
        run_root dnf install -y \
            gcc \
            make \
            pkgconf-pkg-config \
            SDL2-devel \
            SDL2_mixer-devel \
            SDL2_ttf-devel \
            taglib-devel \
            zenity \
            xdg-user-dirs

    elif command -v pacman >/dev/null 2>&1; then
        echo "Arch Linux: the following operation may upgrade other installed packages."
        run_root pacman -Syu --needed \
            base-devel \
            pkgconf \
            sdl2 \
            sdl2_mixer \
            sdl2_ttf \
            taglib \
            zenity \
            xdg-user-dirs

    else
        echo "Error: Unsupported package manager." >&2
        echo "Install GCC, Make, pkg-config, SDL2, SDL2_mixer," >&2
        echo "SDL2_ttf, TagLib and Zenity manually." >&2
        exit 1
    fi
}

echo "================================"
echo "       Installing Sonora"
echo "================================"

if [[ ! -f "$PROJECT_DIR/Makefile" && ! -f "$PROJECT_DIR/makefile" ]]; then
    echo "Error: Makefile not found in $PROJECT_DIR" >&2
    exit 1
fi

if (( EUID != 0 )) && ! command -v sudo >/dev/null 2>&1; then
    echo "Error: sudo is required to install Sonora." >&2
    exit 1
fi

install_dependencies

if ! command -v pkg-config >/dev/null 2>&1; then
    echo "Error: pkg-config is not available." >&2
    exit 1
fi

if ! command -v zenity >/dev/null 2>&1; then
    echo "Error: Zenity is required for folder selection." >&2
    exit 1
fi

echo
echo "Building Sonora..."

make clean >/dev/null 2>&1 || true
make

if [[ ! -x "$PROJECT_DIR/player" ]]; then
    echo "Error: the player executable was not created." >&2
    exit 1
fi

echo
echo "Installing application files..."

run_root install -d \
    "$APP_DIR" \
    "$BIN_DIR" \
    /usr/share/pixmaps \
    /usr/share/applications

run_root install -m 755 \
    "$PROJECT_DIR/player" \
    "$APP_DIR/player"

if [[ -f "$ICON_SOURCE" ]]; then
    run_root install -m 644 "$ICON_SOURCE" "$ICON_DEST"
else
    echo "Warning: mp3player.svg not found. Creating a default icon."

    run_root tee "$ICON_DEST" >/dev/null <<'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
  <rect x="4" y="4" width="120" height="120" rx="28" fill="#24242a"/>
  <path d="M76 24v58c0 12-9 21-22 21-10 0-18-6-18-15s8-16 19-16c5 0 9 1 13 4V38l31-8v39"
        fill="none"
        stroke="#1ed760"
        stroke-width="10"
        stroke-linecap="round"
        stroke-linejoin="round"/>
</svg>
SVG
fi

echo "Creating launcher..."

run_root tee "$BIN_DIR/sonora" >/dev/null <<'WRAPPER'
#!/usr/bin/env bash
set -euo pipefail

PLAYER="/usr/local/lib/sonora/player"

if [[ ! -x "$PLAYER" ]]; then
    echo "Error: Sonora executable not found: $PLAYER" >&2
    exit 1
fi

if (( $# > 0 )); then
    exec "$PLAYER" "$@"
fi

music_dir=""

if command -v xdg-user-dir >/dev/null 2>&1; then
    music_dir="$(xdg-user-dir MUSIC 2>/dev/null || true)"
fi

if [[ -z "$music_dir" || "$music_dir" == "$HOME" ]]; then
    music_dir="$HOME/Music"
fi

if ! mkdir -p -- "$music_dir"; then
    echo "Error: cannot create music directory: $music_dir" >&2
    exit 1
fi

exec "$PLAYER" "$music_dir"
WRAPPER

run_root chmod 755 "$BIN_DIR/sonora"

echo "Creating desktop entry..."

run_root tee "$DESKTOP_FILE" >/dev/null <<'DESKTOP'
[Desktop Entry]
Version=1.0
Type=Application
Name=Sonora
Comment=A lightweight music player
Exec=/usr/local/bin/sonora
TryExec=/usr/local/bin/sonora
Icon=/usr/share/pixmaps/sonora.svg
Terminal=false
Categories=AudioVideo;Audio;Player;
StartupNotify=true
DESKTOP

if command -v update-desktop-database >/dev/null 2>&1; then
    run_root update-desktop-database /usr/share/applications || \
        echo "Warning: could not update the desktop database."
fi

echo
echo "================================"
echo "  Sonora installed successfully!"
echo "================================"
echo
echo "Launch Sonora from your application menu or run:"
echo "  sonora"
echo
echo "You can also specify a music directory:"
echo "  sonora /path/to/music"
echo
echo "Default music directory: $HOME/Music"