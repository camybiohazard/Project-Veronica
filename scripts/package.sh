#!/bin/sh
# Release archives of both builds (pc/tools/package.sh; docs/BUILDING.md "Release packages").
#   scripts/package.sh [OUT_DIR] (default build-release)
# Builds first; set NO_BUILD=1 to package the builds as they are.
set -eu
. "$(dirname "$0")/common.sh"
[ -n "${NO_BUILD:-}" ] || "$root/scripts/build-all.sh"
"$root/pc/tools/package.sh" "${1:-$root/build-release}" --linux "$root/build" --windows "$root/build-win"
