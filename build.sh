#!/bin/sh
# Requires macOS, CMake 3.25+, Python 3.9+, and Xcode with the Metal compiler.
# The pinned SDL source is downloaded on the first build. Built apps work offline.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ff6_release=0
if [ "${1:-}" = "--release" ]; then
    ff6_release=1
    shift
fi
if [ "$#" -ne 2 ] || [ "$1" != "--rom" ] || [ ! -f "$2" ]; then
    echo 'Usage: ./build.sh [--release] --rom /path/to/user-provided-USA-Rev-1.sfc' >&2
    echo 'ROM inputs stay private and are stripped from distribution templates.' >&2
    exit 1
fi
python3 "$root/tools/check_repository.py"
ff6_input_rom=$(cd -- "$(dirname -- "$2")" && pwd)/$(basename -- "$2")
if [ -z "${DEVELOPER_DIR:-}" ]; then
    selected=$(xcode-select -p)
    if DEVELOPER_DIR="$selected" xcrun --find metal >/dev/null 2>&1; then
        DEVELOPER_DIR=$selected
    else
        for candidate in /Applications/Xcode.app/Contents/Developer /Applications/Xcode-beta.app/Contents/Developer; do
            if [ -d "$candidate" ] && DEVELOPER_DIR="$candidate" xcrun --find metal >/dev/null 2>&1; then
                DEVELOPER_DIR=$candidate
                break
            fi
        done
    fi
fi
if [ -z "${DEVELOPER_DIR:-}" ]; then
    echo "A full Xcode installation with the Metal compiler is required. Set DEVELOPER_DIR to its Contents/Developer folder." >&2
    exit 1
fi
# Fail before downloading dependencies or compiling if the exact-output shader
# toolchain is missing. Xcode updates may select a newer incompatible component.
python3 "$root/tools/build_metal.py" --check \
    --identity "$root/config/metal_identity.json" --developer-dir "$DEVELOPER_DIR"
cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DFF6_METAL_DEVELOPER_DIR="$DEVELOPER_DIR" \
    -DFF6_ROM_FILE="$ff6_input_rom" -DFF6_BUILDER_ICON_ARTWORK=
cmake --build "$root/build" --target ff6-builder-package --parallel "${FF6_BUILD_JOBS:-6}"
printf '\nBuilder: %s/build/FFVI Builder.app\n' "$root"
if [ "$ff6_release" -eq 1 ]; then
    python3 "$root/tools/release_builder.py" --builder "$root/build/FFVI Builder.app" \
        --rom "$ff6_input_rom"
fi
