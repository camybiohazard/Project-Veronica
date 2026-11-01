#!/bin/sh
# Both builds: scripts/build-linux.sh, then scripts/build-windows.sh.
set -eu
here=$(dirname "$0")
"$here/build-linux.sh"
"$here/build-windows.sh"
