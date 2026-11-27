> [!IMPORTANT]
> No game data is included. You need your own legally obtained copy of *Resident Evil Code: Veronica X* (PS2, NTSC-U, SLUS-20184).

# Project Veronica

A native PC port of *Resident Evil Code: Veronica X* (PS2) for Linux and Windows, built from the
community decompilation [recvx-decomp](https://github.com/AshfordFamily/recvx-decomp). The
original game code and its PS2 platform are compiled for the PC; the PS2 hardware
underneath (GS, VU, DMA, sound driver, disc, memory card, pad) is emulated at its boundaries, and
native paths replace the expensive parts. The game reads your own disc image in place.

## Features

### Lighting and rendering

- **Per-pixel lighting** brings out pools of light from lamps and fires across large floors and
  walls and smooths the lighting on characters. Smooth light falloff and soft shading are
  independently adjustable.
- **Four lighting modes** tune the room lighting. Original uses the PS2's main and ambient
  light, Balanced dims both, and Horror and Nightmare make local lights and the flashlight
  increasingly important. Lamps, fires, and the flashlight keep their strength in all four modes.
- **Rechargeable flashlight with screen-space shadows** lights rooms, characters, and objects.
  Characters and objects visible in the beam can cast shadows.
- **Surface materials and wet surfaces** give metal, tile, glass, and wood distinct highlights.
  In rainy areas, wet floors glint under lamps, moonlight, and the flashlight. Assign materials
  to textures with the in-game picker. [Material guide](docs/modding/MATERIALS.md)
- **Enhanced fog and post-processing** include per-pixel fog with local light glow and a visible
  flashlight beam, plus optional ambient occlusion, bloom, and colour grading.
- **Display options** include widescreen and ultrawide, up to 8× internal resolution, SMAA,
  AMD FSR 1 upscaling, and HDR output on supported displays. Display-rate interpolation smooths
  motion while game logic runs at its original 30 FPS. [Display settings](docs/USER_GUIDE.md#display)

### Gameplay and modding

- **Camera and controls** include an optional over-the-shoulder mode with free aim and optional
  screen-relative movement for fixed cameras. Keyboard, mouse, and gamepad controls are rebindable.
- **Model swaps and enemy options** let you play as other characters and creatures, adjust
  enemy placements, and spawn enemies.
- **Individual save files** are managed through the game's own save screens. The emulated
  memory card is also available.
- **Mods** include PCSX2 HD texture packs, replacement room files and archive entries, and
  tools for building custom rooms. [HD texture packs](#hd-texture-packs)
- **In-game settings and tools** are available through Enhanced Settings in the OPTION menu,
  the F1 settings overlay, and the F2 cheats and debug overlay.

## Requirements

- Your own disc image of *Resident Evil Code: Veronica X*, PS2 NTSC-U, **SLUS-20184** (`.iso`).
- **Linux**: an x86 PC; the game is a 32-bit (i686) program and needs glibc 2.34 or newer plus
  the 32-bit (multilib) OpenGL, X11/Wayland and audio libraries.
- **Windows**: Windows 10 or later (32-bit executable, runs on 64-bit Windows).
- A GPU with OpenGL 3.3.

## Download

Prebuilt Linux and Windows builds are on the
[Releases](https://github.com/camybiohazard/Project-Veronica/releases) page (no game data
included). Unpack and run `recvx.exe` (Windows) or `./recvx` (Linux); see
[docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## HD texture packs

HD texture packs made for the PCSX2 emulator (for SLUS-20184, the US release) work without
conversion. None are included; use a pack you have.

1. Create a `textures` folder inside the `mods` folder, which sits next to the game (beside
   `recvx.exe` / `recvx` and the `save` folder): `mods/textures/`.
2. Put the pack in there as it comes. Any of these layouts works:
   - `mods/textures/SLUS-20184/replacements/...` (the folder from PCSX2's `textures` directory)
   - `mods/textures/replacements/...`
   - `mods/textures/<pack name>/...`, or the `.dds` files themselves
3. Start the game. The log says `hd textures: N replacements in <folder>`.

**Several packs and load order:** put each pack in its own folder and number the folders, for
example `mods/textures/1_addon/` and `mods/textures/2_base/`. When two packs replace the same
texture, the one in the folder that comes first by name wins, so `1_...` overrides `2_...`.

Settings (F1 or ENHANCED SETTINGS, DISPLAY): **HD textures** turns packs on or off, **HD
textures: UI** also uses redesigned menus, title and inventory (on), **HD textures: font** uses
the pack's font (off). A pack in another folder: `hd_textures_dir = <path>` in `recvx.ini` or
`--hd-textures <path>`. Details: [docs/USER_GUIDE.md](docs/USER_GUIDE.md#hd-texture-packs).

## Building from source

```sh
git clone --recursive https://github.com/camybiohazard/Project-Veronica.git
cd Project-Veronica
scripts/build-linux.sh      # or scripts/build-windows.sh (mingw-w64 cross-build)
scripts/run-linux.sh
```

`--recursive` fetches the decompilation and its header submodules; the decomp's large compiler
submodule is skipped by design (the PC build does not use it). On the first start a file
selector asks for the disc image; the choice is remembered. The Windows build is cross-compiled
with mingw-w64: see [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

| Document | For |
|---|---|
| [docs/USER_GUIDE.md](docs/USER_GUIDE.md) | players: first start, settings, controls, cheats, saves, troubleshooting |
| [docs/BUILDING.md](docs/BUILDING.md) | building on Linux and for Windows, CMake options, tests |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | developers: how the port works, renderer, determinism, test aids |
| [docs/THIRDPERSON.md](docs/THIRDPERSON.md) | third-person mode internals |
| [docs/ACTORS.md](docs/ACTORS.md) | models, skeletons, animation, model swaps |
| [docs/ENEMIES.md](docs/ENEMIES.md) | enemy data, spawner, enemy options |
| [docs/HD_PACK.md](docs/HD_PACK.md) | how PCSX2 HD texture packs are matched and loaded |
| [docs/modding/](docs/modding/README.md) | room format and building custom rooms |
| [CHANGELOG.md](CHANGELOG.md) | feature history |
| [CONTRIBUTING.md](CONTRIBUTING.md) | how to contribute |

## Credits

- **[recvx-decomp](https://github.com/AshfordFamily/recvx-decomp)** by AshfordFamily and its
  contributors: the decompilation this port is built on (MIT).
- Libraries: [SDL 3](https://github.com/libsdl-org/SDL) (zlib),
  [Dear ImGui](https://github.com/ocornut/imgui) (MIT),
  [miniupnpc and libnatpmp](https://github.com/miniupnp) (BSD-3-Clause),
  [Monocypher](https://github.com/LoupVaillant/Monocypher) (BSD-2-Clause or CC0). Versions and
  details: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

The code in this repository is released under the [MIT License](LICENSE). The decompilation in
`decomp/` is MIT-licensed by its authors; third-party components keep their own licenses (see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).

## Legal

This is an unofficial fan project, not affiliated with or endorsed by Capcom, Sony Interactive
Entertainment, Sega or CRI Middleware. *Resident Evil* and *Code: Veronica* are trademarks of
Capcom; PlayStation is a trademark of Sony Interactive Entertainment. No game assets are
distributed: you must supply your own legally obtained copy of the game. Files you generate from
your copy (for example modded rooms) contain game-derived data and must not be shared.
