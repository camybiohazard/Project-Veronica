# HD texture packs

The port loads HD texture packs made for PCSX2 as they are: no conversion, no data of ours derived
from a pack. How to install one and the settings: [USER_GUIDE.md](USER_GUIDE.md#hd-texture-packs).
This page is about how it works (`pc/src/gs/hdpack.c`).

## How PCSX2 names a texture

From PCSX2's `GSTextureCache::HashCacheKey::Create` / `HashTextureLevel` and
`GSTextureReplacements.cpp` (file names `%llx-%llx-%08x.dds`, or `%llx-%08x` for direct colour):

- **TEX0 hash**: XXH3-64 over the 2^TW x 2^TH texels at TEX0's TBP0/TBW/PSM in GS memory. For
  PSMCT32, PSMT8 and PSMT4 textures of at least one GS block (8x8, 16x16, 32x16) the raw 256-byte
  memory blocks, block row by block row (the swizzled bytes as they lie in GS memory); otherwise
  (smaller textures and the formats that do not fill 32 bits: CT24, CT16, T8H, T4HL/HH) the
  expanded texels row by row, one byte per index for paletted formats, RGBA32 with TEXA applied
  for direct ones.
- **CLUT hash** (paletted only): XXH3-64 over the 16 or 256 palette entries as RGBA32 from the
  CLUT buffer, CSA applied (for CT32 T8 the offset clamps at the last 16-entry block, as
  `GSClut::ReadCLUT_T32_I8`).
- **bits**: PSM | TW << 6 | TH << 10; for CT24/CT16/CT16S also TEXA's TA0 << 15 | AEM << 23 |
  TA1 << 24. Bit 14 (TCC in old packs) is ignored. Region (`-r`) and mip level (`-mipN`) names
  are not used.

The game issues the same TEX0s from the same GS memory and CLUT contents as on the PS2, and the GS
memory model is exact, so the key is computed exactly: each texture is hashed when the renderer
decodes it (native textures: `textures.c ntex_lookup`, after the deferred uploads over the
texture are written into GS memory; everything else: `gs_gl.c tex_get`). A hit binds the
replacement instead of the decoded texture; the draw keeps the original texture's size for its
UVs, so the replacement covers the same 0..1 space at its own resolution.

- Palette animation: decoded textures are cached per palette, so a new palette is a new decode
  and a new key (its own replacement if the pack has one, else the original).
- Render-to-texture (render targets sampled as textures, `StoreRenderTex` captures) and movie
  frames never reach a decode, so they are never replaced (PCSX2 does not hash them either).
- Textures whose 2^TW x 2^TH area reaches past the uploaded image hash whatever else lies in GS
  memory there, as on the PS2; they match only if that memory matches too.
- The game never sets TEX1 MXL > 0, so PCSX2's mip-level hashing does not come up.

## Content check

A key match only says the pack was made from the same texture data, not that the replacement shows
the same picture. Packs made on a translated disc or with a redesigned UI replace menu, inventory
and title textures with other layouts and text, and the port builds its own entries
(ENHANCED SETTINGS, RETURN TO TITLE) from sub-rectangles of the original menu
textures. So the first time a replacement is loaded it is compared with the original texels
(taken from GS memory and the CLUT at the lookup) on a grid of at most 32 x 32 of them: each
against the replacement's texels over its area (box filter, colour weighted by alpha), decoding
only the BC blocks those cover (bcdec: BC1/2/3/7; RGBA; PNG). A result near a threshold is
repeated over all texels.

- **alpha**: PCSX2 dumps GS alpha (0x80 = 1.0) and uses a replacement's alpha byte as is, but
  pack authors often store 0..255, which doubles the strength of blended UI. The check picks what
  fits the original best: as is, scaled by 128/255, or (when the original's alpha is constant)
  the original's constant. The draw uses it (`u_tex_alpha_scale`; the constant through
  `GL_TEXTURE_SWIZZLE_A`). Different if it still differs by more than 32 on average.
- **colour** (texels visible in the original): different if it differs by more than 24 on average,
  or by more than 48 in over 8% of the texels. Upscaled textures of a large pack stayed within 21
  and 7%; translated or redesigned ones started at 19 and 13%.
- **broken**: a replacement that is (nearly) transparent where the original shows is never used.

A different replacement is logged (`hd textures: <file> does not show the original (...)`) and
used only with `hd_textures_ui` (or, for font pages, `hd_textures_font`). Verdicts are kept in
`<save>/hd_textures.txt` (verdict, alpha mode, file size, path in the pack folder), so each file
is checked once; delete it to check again.

## Tags: font and the port's menu letters

Native texture definitions carry tags set by the game thread when the TIM2 is uploaded
(`texture_glue.c`, sent with the definition in the frame stream):

- **font**: the game's font pages, the first four textures of the effect list
  (`bhSetFontTexture`, drawn by `bhDispFont`/`bhDispFontEx`: messages, item names and
  descriptions), while `sys->ss_flg & 0x40` says they are loaded. Replaced only with
  `hd_textures_font`.
- **keep**: the textures the port cuts its own menu entries from (`pc_tex_keep_original` in
  `options_glue.c`). Even with `hd_textures_ui` they get only replacements
  that show the original.

A tag that changes makes a new definition. Textures resolved from GS memory (native textures off)
carry no tags.

## Loading

The folder is indexed by file name once, at the first texture (some 6700 files: a few ms). When
several files have the same name (the same key, e.g. two packs in `1_...` and `2_...` folders), the
one whose path in the pack folder comes first wins (byte order, `\` read as `/`, so the same on
Windows and Linux; logged as `hd textures: N names in several folders, the first folder in name
order wins`); the others are never loaded. A file
is loaded when its texture is first drawn (DDS: BC1/BC2/BC3/BC7 compressed as they are,
uncompressed RGBA/BGRA; PNG through SDL):

- Three **loader threads** read it and check it (the first time); the draws keep the original
  texture meanwhile. At the start of each executed frame the render thread uploads what is read,
  up to 3 ms of it per frame (at least one file). Files read but not uploaded are capped at
  128 MB.
- **Under a screen fade that lets the picture through** (`pc_frame_fade`: the game's fade alpha
  between 0 and 255, as in a room's fade-in) a draw waits for its file instead: read on the
  render thread if no loader thread has begun it, else waited for. A room's textures are first
  drawn under its fade, so it shows its replacements from its first visible frame. The room is
  first drawn one frame earlier under the black screen; that frame already hands its files to the
  loader threads.
- **Textures first drawn later** (another camera cut, an enemy, an effect, a menu without a
  fade) show the original until their file is in, usually one to a few frames later. Loading them
  where they were drawn (up to 0.6.3) paused the render thread at the cut: measured in nine
  rooms, up to 53 ms per cut with the files in the page cache and up to 147 ms from a cold disk
  on the first visit (NVMe; slower disks pause longer). Now a cut costs the decoding of its new
  original textures and their keys: up to 23 ms where a cut shows 35 new ones (11 ms without a
  pack), whatever the disk.
- A room's first visible frame waits for its files: in the same nine rooms up to 33 ms with the
  files in the page cache, up to 90 ms from a cold disk on the first visit (46 files, 195 MB);
  read on the render thread alone they took up to 72 and 303 ms.

Which replacements a room needs is known only when it is drawn: native texture definitions are
recorded at a TIM2's first upload, which the ordering-table walker makes when it first draws
that texture, and the key needs the draw's TEX0 (size, format, CLUT bank) and palette. So
nothing is loaded ahead of the first draw.

Loaded replacements stay in a budget of 1024 MB (`RECVX_HD_CACHE_MB`); the least recently used
are dropped and read again when needed. One log line sums it up 2 s after lookups and loads stop
(read and check times on either thread, upload on the render thread):

```
hd textures: 95 hits (4 rejected, 1 font kept), 0 misses; 80 loaded, 295 MB kept (read 40 ms, checks 136 ms, upload 29 ms, keys 4 ms)
```

Test aids: `RECVX_TRACE_HD=1` logs every lookup (`hit`/`miss` with its PCSX2 name and tags) and
every check; `RECVX_HD_DUMP=dir` writes each looked-up original as PNG under its PCSX2 name.

## Limitations

- Packs usually have no mipmaps (PCSX2 draws them the same way): 2048/4096 replacements on
  distant surfaces shimmer.
- The content check is a threshold: a replacement that changes a few small letters in a big
  texture can pass, a heavily repainted one fail.
- Menus and other 2D: with `hd_textures_ui` (on by default) redesigned menus are used; the
  textures the port cuts its own menu entries from keep the original (`NTEX_KEEP`). A layout
  that moves things the game draws at fixed coordinates can still look off; turn the option
  off to keep the original menus.
- Without a pack nothing changes (frame-identical, `pc/tools/layout_check.sh`).
