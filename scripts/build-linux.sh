#!/bin/sh
# Build the 32-bit Linux executable: build/pc/recvx (docs/BUILDING.md "Linux").
#   scripts/build-linux.sh [cmake --build args, e.g. --target recvx]
#   BUILD_DIR (default build), BUILD_TYPE (default RelWithDebInfo)
set -eu
. "$(dirname "$0")/common.sh"
dir=${BUILD_DIR:-$root/build}
configure "$dir" linux-i686.cmake
cmake --build "$dir" "$@"
