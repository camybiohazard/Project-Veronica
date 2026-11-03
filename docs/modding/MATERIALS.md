# Surface materials

`mods/materials.ini` gives textures a **material class**, so lit surfaces of that texture show a
highlight from the game's lights, the flashlight and the moon (DISPLAY → LIGHTING → **Material
shine**; native 3D renderer). The game itself has no material data: classes are assigned by hand,
with the picker in the F2 window, and the file can be shared.

The release downloads come with a `mods/materials.ini` that already sets up many of the game's
textures (still a work in progress). Unpacking a newer release over an older one replaces it, so
keep a copy of your own changes.

## The file

One texture per line, `hash = class`; `#` starts a comment:

```ini
# Surface materials: texture image hash = class (docs/modding/MATERIALS.md)
2f546237 = glass
78a85bbe = tile
```

The hash is an FNV-1a hash of the texture's image data as the game uploads it (image and palette,
`pc/src/gs/textures.c`), so the same texture has the same hash in every room and every run. It
does not depend on HD texture packs: a replaced texture keeps the original's material.

| Class | Key | Highlight |
|---|---|---|
| None | `none` | none (the original) |
| Metal (rough) | `metal` | broad, in the texture's colour |
| Metal (polished) | `metal_polished` | sharp and strong, in the texture's colour |
| Wood (varnished) | `wood` | soft and faint |
| Stone / concrete | `stone` | very broad and faint |
| Tile / ceramic | `tile` | sharp |
| Glass | `glass` | very sharp and strong |
| Paint / plastic | `paint` | soft |
| Wet | `wet` | sharp, as rain-wet floors |
| Leather | `leather` | broad and faint |

The parameters (Blinn-Phong exponent, strength, metal tint) are in `pc/src/gs/materials.c`.
**Material shine** (DISPLAY -> LIGHTING) switches them all on or off.

## The picker (F2)

1. Open F2 and tick **Pick a surface with the mouse** (under *Surface materials*).
2. Click a wall, floor or object in the game picture, outside the window. The nearest textured 3D
   triangle there is taken (in the next game frame); its texture's hash is shown and every
   surface using it pulses magenta while **Highlight** is on.
3. Choose a class under **Material**: it is written to `mods/materials.ini` at once.

Only textured 3D surfaces (rooms, objects, characters, enemies) can be picked; 2D, effects and
the game's own render textures cannot. A highlight shows only where a light reflects towards the
camera, so a surface may need a light or the flashlight at the right angle to show it.

Test aid: `RECVX_MATPICK="frame:x:y:class"` picks at x, y (the game's 640 x 480 screen space)
once that frame is executed, assigns the class (its number in the table above, None = 0) and
keeps the highlight on; the log gets `matpick: texture <hash> ...`.
