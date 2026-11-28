#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 /path/to/linuxdeploy.AppImage /path/to/runtime-x86_64 /path/to/output.AppImage" >&2
    exit 2
fi

linuxdeploy=$(realpath "$1")
runtime=$(realpath "$2")
output=$(realpath -m "$3")
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

make -C "$root" 6502core_rel
install -Dm755 "$root/6502core_rel" "$work/6502core"
install -Dm644 "$root/LICENSE.TXT" "$work/AppDir/usr/share/licenses/6502core/LICENSE.TXT"

export APPIMAGE_EXTRACT_AND_RUN=1
export LDAI_OUTPUT="$output"
export LDAI_RUNTIME_FILE="$runtime"
"$linuxdeploy" --appdir "$work/AppDir" \
    --executable "$work/6502core" \
    --desktop-file "$root/packaging/6502core.desktop" \
    --icon-file "$root/packaging/6502core.svg" \
    --output appimage
