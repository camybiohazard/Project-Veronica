# PC port build.
#
#   recvx_game     : the original game + platform code from the decomp, compiled
#                    unmodified except for '#ifdef TARGET_PC' asm replacements.
#   recvx_platform : the new PC abstraction layer (PS2 hardware/IOP emulation at
#                    library boundaries, renderer, audio, input, overlay).
#   recvx          : the executable.

set(DECOMP ${RECVX_DECOMP_DIR})
add_compile_definitions(_FILE_OFFSET_BITS=64)   # disc images over 2 GB (cdvd.c fseeko); mingw-w64 honours it too
if(WIN32)
    # the crash log walks the EBP frame chain (host/log.c); measured on Linux: +1..5% game logic
    add_compile_options(-fno-omit-frame-pointer)
endif()
set(PROG ${DECOMP}/src/ps2/veronica/prog)
set(KATANA_INC ${DECOMP}/include/recvx-decomp-katana/KATANA/Include)

# SDL3 is built from source and linked statically by default, so the binary does not
# depend on a (32-bit) SDL3 package being installed. SDL still dlopen()s the system's
# X11/Wayland/audio libraries at runtime.
option(RECVX_VENDORED_SDL "Build SDL3 from source and link it statically" ON)
if(RECVX_VENDORED_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.16 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 REQUIRED CONFIG)
endif()

# Dear ImGui (MIT) for the F1 overlay, with its SDL3 + OpenGL3 backends.
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ---------------------------------------------------------------- headers
# The Katana SDK headers are included with inconsistent case (SG_MWPLY.H vs
# sg_mwply.h). Mirror them under both spellings for case-sensitive filesystems.
set(CASEFOLD_DIR ${CMAKE_BINARY_DIR}/katana_casefold)
file(GLOB KATANA_HEADERS RELATIVE ${KATANA_INC} ${KATANA_INC}/*)
foreach(h ${KATANA_HEADERS})
    string(TOLOWER ${h} lo)
    string(TOUPPER ${h} up)
    foreach(n ${lo} ${up})
        if(NOT n STREQUAL h AND NOT EXISTS ${KATANA_INC}/${n} AND NOT EXISTS ${CASEFOLD_DIR}/${n})
            file(MAKE_DIRECTORY ${CASEFOLD_DIR})
            file(CREATE_LINK ${KATANA_INC}/${h} ${CASEFOLD_DIR}/${n} COPY_ON_ERROR SYMBOLIC)
        endif()
    endforeach()
endforeach()

# ---------------------------------------------------------------- game (decomp)
file(GLOB GAME_SOURCES ${PROG}/*.c)
set(GAME_EXCLUDE
    gcc_wrapper.c        # ee-gcc soft-float/int64 runtime helpers
    ps2_MovieFunc.c      # libmpeg/IPU movie player -> pc/src/movie
    ps2_MovieWork.c
    ps2_Vu1Scissor2.c    # #included by ps2_Vu1Strip.c
)
foreach(x ${GAME_EXCLUDE})
    list(REMOVE_ITEM GAME_SOURCES ${PROG}/${x})
endforeach()

# PC implementations of decomp-declared APIs that need the decomp's types
file(GLOB GLUE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/glue/*.c)

add_library(recvx_game OBJECT ${GAME_SOURCES} ${GLUE_SOURCES})
target_compile_definitions(recvx_game PRIVATE main=recvx_game_main)
target_include_directories(recvx_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
    ${DECOMP}/src/cri/mwlib/ee/lib/libadxe
    ${PROG}
)
# Katana/CRI dirs ship libc-named headers (math.h, stddef.h, ...); search them
# after the host's system headers.
target_compile_options(recvx_game PRIVATE
    "SHELL:-idirafter ${KATANA_INC}"
    "SHELL:-idirafter ${CASEFOLD_DIR}"
    "SHELL:-idirafter ${DECOMP}/include/recvx-decomp-cri/cri/mwlib/include"
    "SHELL:-idirafter ${DECOMP}/include/recvx-decomp-cri/cri/mwlib/ee/include"
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/pc_prelude.h"
    -std=gnu11
    -fno-strict-aliasing
    -fwrapv
    -fsigned-char             # EE char is signed
    -ffp-contract=off         # no FMA fusion: keep the PS2's operation order
    -msse2 -mfpmath=sse       # IEEE single precision, no x87 extended intermediates
    -fno-tree-vectorize
    -ftrivial-auto-var-init=zero  # deterministic stand-in for MWCC's stale registers (docs/dev/uninitialized-locals.txt)
)
# the decomp is compiled as is, without warnings (-w), but a call to an undeclared function stays an
# error: without a prototype a float argument goes as a double and a char/float return is read as an
# int. -w also hides -Werror=..., so this relies on GCC 14's permerror (no -fpermissive here; the other
# permerrors the decomp needs are switched off one by one). GCC 13 (CI) only warns: the ctest
# decomp_declarations checks it there. Missing returns and uninitialized locals are listed by
# pc/tools/decomp_warnings.py. The port's own glue gets warnings.
set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-w;-Wno-incompatible-pointer-types;-Wno-int-conversion;-Wno-return-mismatch")
set_source_files_properties(${GLUE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wno-unused-function")
if(WIN32)
    # GCC (not MSVC) bitfield layout, as on Linux and the EE
    target_compile_options(recvx_game PRIVATE -mno-ms-bitfields)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # the executable is non-PIE (fixed address window); position-independent code only costs
    # get_pc_thunk calls on x86-32
    target_compile_options(recvx_game PRIVATE -fno-pie)
endif()

# ---------------------------------------------------------------- platform layer
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(recvx_platform OBJECT ${PLATFORM_SOURCES})
# xxHash (BSD-2-Clause, header only): XXH3, the hash PCSX2 names its texture replacements by (src/gs/hdpack.c)
include(FetchContent)
FetchContent_Declare(xxhash GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git GIT_TAG v0.8.3 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
# bcdec (MIT / Unlicense, header only): BC1-BC7 decoding for the replacements' content check
FetchContent_Declare(bcdec GIT_REPOSITORY https://github.com/iOrange/bcdec.git GIT_TAG 80859ed3b7afb1c527a2a99d70c61457bea72d0c SOURCE_SUBDIR none)
FetchContent_MakeAvailable(xxhash bcdec)
target_include_directories(recvx_platform PRIVATE ${xxhash_SOURCE_DIR} ${bcdec_SOURCE_DIR})
set_property(SOURCE src/host/host.c src/host/overlay.cpp APPEND PROPERTY COMPILE_DEFINITIONS "RECVX_VERSION=\"${RECVX_VERSION}\"")
target_include_directories(recvx_platform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
)
target_compile_options(recvx_platform PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    -fno-strict-aliasing -fwrapv -ffp-contract=off -msse2 -mfpmath=sse
    -Wall -Wno-unused-function
)
target_link_libraries(recvx_platform PRIVATE SDL3::SDL3 imgui)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(recvx_platform PRIVATE -fno-pie)
endif()
if(WIN32)
    target_compile_options(recvx_platform PRIVATE $<$<COMPILE_LANGUAGE:C>:-mno-ms-bitfields>)
endif()

# ---------------------------------------------------------------- executable
set(RECVX_LAYOUT_PAD 0 CACHE STRING "test aid: bytes of padding before all code, data and bss (pc/tools/layout_check.sh)")
if(RECVX_LAYOUT_PAD)
    set(LAYOUT_PAD_SRC tests/layout_pad.c)
    set_source_files_properties(tests/layout_pad.c PROPERTIES COMPILE_DEFINITIONS RECVX_LAYOUT_PAD=${RECVX_LAYOUT_PAD})
endif()
if(WIN32)
    enable_language(RC)
    set(RECVX_RES res/recvx.rc)   # the program icon
    set_source_files_properties(res/recvx.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/recvx-enhanced.ico)
endif()
add_executable(recvx ${LAYOUT_PAD_SRC} ${RECVX_RES} $<TARGET_OBJECTS:recvx_game> $<TARGET_OBJECTS:recvx_platform>
               $<$<TARGET_EXISTS:glibc_compat>:$<TARGET_OBJECTS:glibc_compat>>)
target_link_libraries(recvx PRIVATE imgui SDL3::SDL3 m)
if(WIN32)
    target_link_libraries(recvx PRIVATE pthread shell32 dwmapi)   # clock_gettime (winpthreads); borderless corners (settings.c)
    # Same address window as Linux: fixed image base, no ASLR relocation; large-address-aware
    # gives the 32-bit exe 4 GB on 64-bit Windows (Linux already uses addresses above 2 GB). -static: libstdc++,
    # libgcc and winpthread go into the exe, only system DLLs (UCRT, opengl32, ...) remain.
    target_link_options(recvx PRIVATE -static -Wl,--image-base,0x30000000 -Wl,--disable-dynamicbase -Wl,--large-address-aware
                        -Wl,--stack,8388608)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Place the image inside the PS2 address window (see docs/ARCHITECTURE.md, "Data model and memory").
    target_link_options(recvx PRIVATE -no-pie -Wl,-Ttext-segment=0x30000000 -static-libgcc -static-libstdc++)
    set_target_properties(recvx PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

# ---------------------------------------------------------------- tests
add_executable(test_vu tests/test_vu.c src/ps2/vu.c)
target_compile_options(test_vu PRIVATE -std=gnu11 -msse2 -mfpmath=sse -ffp-contract=off)
target_link_libraries(test_vu PRIVATE m)
add_test(NAME vu COMMAND test_vu)
# the same against the plain C (non-SSE) native paths, as an arm64 build would compile them
add_executable(test_vu_portable tests/test_vu.c src/ps2/vu.c)
target_compile_definitions(test_vu_portable PRIVATE RECVX_VU_PORTABLE)
target_compile_options(test_vu_portable PRIVATE -std=gnu11 -msse2 -mfpmath=sse -ffp-contract=off)
target_link_libraries(test_vu_portable PRIVATE m)
add_test(NAME vu_portable COMMAND test_vu_portable)

# decodes every movie on the disc (needs RECVX_ISO; skipped otherwise)
add_executable(test_movies tests/test_movies.c src/movie/mpeg2.c src/movie/pss.c)
target_compile_options(test_movies PRIVATE -std=gnu11 -O2)
target_link_libraries(test_movies PRIVATE m)
add_test(NAME movies COMMAND test_movies)
set_tests_properties(movies PROPERTIES SKIP_RETURN_CODE 77)

# calls to undeclared functions in the decomp: GCC 14+ stops the build on them, GCC 13 only warns (and -w hides it)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_test(NAME decomp_declarations COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/decomp_warnings.py --declarations ${CMAKE_BINARY_DIR})
endif()

# control bindings (pc/src/input/bindings.c): defaults, rebinding, ini, a virtual SDL gamepad
add_executable(test_bindings tests/test_bindings.c src/input/bindings.c)
target_compile_options(test_bindings PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=gnu11>)
target_link_libraries(test_bindings PRIVATE SDL3::SDL3)
add_test(NAME bindings COMMAND test_bindings)

# modern controls' direction math (pc/src/input/modern.c): camera + stick -> angle, the cut hold
add_executable(test_modern tests/test_modern.c src/input/modern.c)
target_compile_options(test_modern PRIVATE -std=gnu11)
target_link_libraries(test_modern PRIVATE m)
add_test(NAME modern COMMAND test_modern)

# pure rules of the PC hooks (pc/include/pc_rules.h, src/gs/native.h): added enemies' helper code and holders,
# dual weapons' second target, hor+ margin meshes and clipping, warp music, "Unlock all";
# with the game's flags and types
add_executable(test_rules tests/test_rules.c)
target_include_directories(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,INCLUDE_DIRECTORIES>)
target_compile_options(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,COMPILE_OPTIONS>)
target_link_libraries(test_rules PRIVATE m)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(test_rules PRIVATE -no-pie)   # (the game's flags include -fno-pie)
endif()
add_test(NAME rules COMMAND test_rules)

# every recvx.ini key has a row in the settings table (F1 and OPTION > ENHANCED SETTINGS) or is listed
add_test(NAME settings COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_settings.cmake)

# native texture queue (pc/src/gs/textures.c): superseded uploads never written, the rest only when read
add_executable(test_ntex tests/test_ntex.c src/gs/textures.c)
target_compile_options(test_ntex PRIVATE -std=gnu11)
add_test(NAME ntex COMMAND test_ntex)

# "You appear as" list (pc/src/host/actors.c): event doubles hidden, indices kept
add_executable(test_appear tests/test_appear.c src/host/actors.c)
add_test(NAME appear COMMAND test_appear)

# enemy variants (pc/src/host/variants.c): the built-in table, the mods/enemies.ini reader
add_executable(test_variants tests/test_variants.c src/host/variants.c src/host/actors.c src/host/fileio.c)
add_test(NAME variants COMMAND test_variants)

# UTF-8 file paths and replace-on-rename saving (pc/src/host/fileio.c)
add_executable(test_fileio tests/test_fileio.c src/host/fileio.c)
target_compile_options(test_fileio PRIVATE -std=gnu11)
add_test(NAME fileio COMMAND test_fileio)

# native save files (pc/src/host/savestore.c)
add_executable(test_savestore tests/test_savestore.c src/host/savestore.c src/host/fileio.c)
target_compile_options(test_savestore PRIVATE -std=gnu11)
target_link_libraries(test_savestore PRIVATE SDL3::SDL3)
add_test(NAME savestore COMMAND test_savestore)
