#!/bin/sh
# Cross-build the 32-bit Windows executable with mingw-w64: build-win/pc/recvx.exe (docs/BUILDING.md "Windows").
#   scripts/build-windows.sh [cmake --build args] (default --target recvx)
#   BUILD_DIR (default build-win), BUILD_TYPE (default RelWithDebInfo),
#   MINGW_PREFIX (folder holding bin/i686-w64-mingw32-gcc; default: PATH)
set -eu
. "$(dirname "$0")/common.sh"
dir=${BUILD_DIR:-$root/build-win}
configure "$dir" mingw-i686.cmake ${MINGW_PREFIX:+-DMINGW_PREFIX="$MINGW_PREFIX"}
[ $# -gt 0 ] || set -- --target recvx
cmake --build "$dir" "$@"
