#!/bin/sh
# Build (incrementally) and run the Windows build under Steam's Proton, without adding it to Steam.
#   scripts/run-windows-proton.sh [recvx.exe args]
# Linux paths work as arguments and in recvx.ini: Proton maps / to drive Z:.
# Settings and saves: build-win/pc/save (next to the program) unless --save-dir says otherwise.
#   PROTON        the Proton folder (default: Proton - Experimental, else the newest Proton, in any
#                 Steam library or compatibilitytools.d)
#   PROTON_PREFIX the Wine prefix (default build-win/proton-prefix; made on the first run)
#   PROTON_LOG=1  Proton writes ~/steam-<appid>.log
#   NO_BUILD=1    run without building. BUILD_DIR as scripts/build-windows.sh.
set -eu
. "$(dirname "$0")/common.sh"
dir=${BUILD_DIR:-$root/build-win}

steam=
for s in "$HOME/.steam/steam" "$HOME/.local/share/Steam" "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
    [ -d "$s/steamapps" ] && { steam=$(cd "$s" && pwd -P); break; }
done
[ -n "$steam" ] || { echo "no Steam installation found (~/.steam/steam, ~/.local/share/Steam, Flatpak)" >&2; exit 1; }

if [ -z "${PROTON:-}" ]; then
    # Steam libraries (libraryfolders.vdf "path" lines) and custom tools such as GE-Proton
    libs=$(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"/\1/p' "$steam/steamapps/libraryfolders.vdf" 2>/dev/null || true)
    PROTON=
    for l in "$steam" $libs; do
        [ -x "$l/steamapps/common/Proton - Experimental/proton" ] && { PROTON="$l/steamapps/common/Proton - Experimental"; break; }
    done
    if [ -z "$PROTON" ]; then
        PROTON=$(for l in "$steam" $libs; do ls -d "$l"/steamapps/common/Proton* 2>/dev/null; done;
                 ls -d "$steam"/compatibilitytools.d/*Proton* 2>/dev/null) || true
        PROTON=$(printf '%s\n' "$PROTON" | while read -r p; do [ -x "$p/proton" ] && echo "$p"; done | sort -V | tail -n 1)
    fi
fi
[ -x "$PROTON/proton" ] || { echo "no Proton found: install one in Steam (Library > Tools) or set PROTON=/path/to/Proton" >&2; exit 1; }

[ -n "${NO_BUILD:-}" ] || "$root/scripts/build-windows.sh"
prefix=${PROTON_PREFIX:-$dir/proton-prefix}
mkdir -p "$prefix"
echo "Proton: $PROTON" >&2
STEAM_COMPAT_CLIENT_INSTALL_PATH=$steam STEAM_COMPAT_DATA_PATH=$prefix \
    exec "$PROTON/proton" run "$dir/pc/recvx.exe" "$@"
