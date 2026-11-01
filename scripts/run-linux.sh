#!/bin/sh
# Build (incrementally) and run the Linux build.
#   scripts/run-linux.sh [recvx args, e.g. --save-dir DIR or a disc image]
# Settings and saves: build/pc/save (next to the program) unless --save-dir says otherwise.
# NO_BUILD=1: run without building. BUILD_DIR as scripts/build-linux.sh.
set -eu
. "$(dirname "$0")/common.sh"
dir=${BUILD_DIR:-$root/build}
[ -n "${NO_BUILD:-}" ] || "$root/scripts/build-linux.sh" --target recvx
exec "$dir/pc/recvx" "$@"
