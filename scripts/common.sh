# Shared by the scripts in this folder (sourced, not run). docs/BUILDING.md "Scripts".
#   BUILD_TYPE   CMake build type (default RelWithDebInfo)
root=$(cd "$(dirname "$0")/.." && pwd)

# configure DIR TOOLCHAIN [cmake args]: configure the build folder DIR with cmake/TOOLCHAIN; a cache
# made with another toolchain, or a half-deleted folder, is configured afresh (the toolchain file is
# only read on a first configure). compile_commands.json is written for editors (clangd, VS Code).
configure() {
    dir=$1 toolchain=$root/cmake/$2; shift 2
    if [ -f "$dir/CMakeCache.txt" ] && { ! grep -q "^CMAKE_TOOLCHAIN_FILE:[A-Z]*=$toolchain\$" "$dir/CMakeCache.txt" ||
                                         [ ! -f "$dir/CMakeFiles/rules.ninja" ]; }; then
        echo "$dir: stale CMake cache, configuring afresh" >&2
        rm -rf "$dir/CMakeCache.txt" "$dir/CMakeFiles"
    fi
    cmake -S "$root" -B "$dir" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
}
