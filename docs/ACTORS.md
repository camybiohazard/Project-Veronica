# Actors

How players and enemies are loaded, spawned, animated and drawn, and how the port shows any
model as the player ("You appear as"). Paths are relative to `decomp/src/ps2/veronica/prog/`
unless noted; types are in `decomp/include/ps2/veronica/prog/types.h`. Related:
[ENEMIES.md](ENEMIES.md) (room enemies, budgets, F2 spawner, enemy options),
[THIRDPERSON.md](THIRDPERSON.md),
[modding/ROOM_FORMAT.md](modding/ROOM_FORMAT.md).

## Summary

* All characters and enemies are `BH_PWORK` (0x580 bytes): the player is `ply` (`plp`), enemies
  `ene[128]`. A model is an `ML_WORK`: a flat array of `NJS_CNK_OBJECT` nodes in depth-first
  order plus one `O_WORK` (matrix) per node. Motions (`MN_WORK`) are per-frame keys, one track
  per node, bound to the skeleton **purely by node index**.
* The four player files (Claire, Chris, Steve, Wesker; Claire's second costume) share **the same
  22-node hierarchy**. Their motions carry root position and rotations only, so each body keeps its
  own bone lengths and any of the meshes plays any other's motions.
* Enemy models, motions and textures are **not** in global files: each room file carries copies
  for its own records. Other types are loaded from another room file (the F2 spawner,
  [ENEMIES.md](ENEMIES.md) "Enemy spawner (F2)").
* Any enemy or NPC model can be shown as the player ("You appear as"): a display actor drawn in
  place of the player, retargeted from the player's skeleton.

## Memory and load pipeline

### Main arena

* `main.c njUserInit`: `freemem = syMalloc(12845056)` (12.25 MB). Vertex/view buffers live at
  the top; `sys->endp = njpmemp`.
* `system.c bhFirstGameStart`: `sys->obwp` (32 `O_WRK` objects, `[0..3]` reserved for the
  player's weapons/hair) and `sys->itwp` (32 `O_WRK` room items).
* `bhSysCallMonitor` mode 1 (game start / load): reads sys file 1 (common data), then
  `bhInitPlayer` allocates the **persistent player buffers** and sets `sys->mempb = sys->memp`.
  Everything above `mempb` is per room and discarded at every room change.
* Allocators: `pwksub.c bhGetFreeMemory` (64-byte aligned bump from `sys->memp`, refuses only
  beyond `freemem + 12.25 MB`) and `eneset.c bhEne_CallocWork` (refuses beyond
  `mempb + 0xC40000`); on the PC both limits grow by `PC_ARENA_EXTRA`. **Neither checks
  `sys->endp`**: an overrun writes into the vertex buffers. Nothing is freed before the next room.

Player buffers (`player.c bhInitPlayer`):

| Pointer | Size | Content |
|---|---|---|
| `sys->plmdlp` | 131072 | player model block (all sub-models) + their `O_WORK` arrays |
| `sys->wrmdlp` / `wlmdlp` | 32768 each | right / left hand weapon model |
| `sys->plmthp` | 12288 | 512 `MN_WORK` (motion headers), `plp->mnwPb` |
| `sys->plbmtp` | 393216 | base motion data |
| `sys->plwmtp` | 65536 | weapon motion data (motions 100+) |
| `sys->plzmtp` | 8192 | per-motion forward translation (`player.c bhGetTransZ`) |
| `sys->plexwp` / `plhdwp` / `pletcp` | 124 / 124 / 32768 | `plp->exp0`, `exp1` (head/eye), `exp3` |

### Room load

`system.c bhSysCallMonitor` mode 4: `rm_SRRC.rdx` (stage, room, case) is read to `endp - size`
and `Expand`ed to `sys->memp`; `room.c bhSetRoom` realizes the room model, **every enemy
record's model** (`bhSetEneMdl`), objects and items and builds the texture upload list (max 256,
one `bhSetMemPvpTexture` per frame); `bhFinishRoom` adds enemy motion sets (`bhSetEneMtn`), the
room motion set (`sys->rmthp`), `O_WORK`s and the player placement. `sys->memp` is left at the
start of the texture section, which is reused after upload. A story character switch
(`cb_flg & 0x80`, op 137 `bhPlayerChangeSet`) reads the new part and weapon files here. Then
`bhInitEvent` starts the room script and the room's sound bank is requested. The largest
expanded room file is 8.9 MB.

### Textures

* `ps2_texture.c bhSetMemPvpTexture(texlist, data, offset)` walks `PLI`/`TIM2` chunks and calls
  `njLoadTexture` per texture. Entries are keyed by the TIM2 global index (`Gindex`) + CLUT bank:
  the same `Gindex` loaded twice is shared and refcounted, which is why two instances of a type
  coexist.
* `Ps2TextureMalloc` copies the TIM2 into the texture heap (`Ps2_tex_buff`), so source buffers may
  be freed after upload. Table `main.c tbuf`. A full table or heap calls `exit(0)`. PS2: 10 MB,
  256 entries; PC: 64 MB, 1024 entries (`PC_TEX_HEAP_SIZE`, `PC_TEX_ENTRIES`).
* Palettes: each `PLI` entry carries a fixed CLUT bank id (`bk_id`, 255 = allocate from
  `sys->ef_pbnk`). Loading a model into a room it was not authored for can overwrite another
  asset's palette (wrong colours); not observed so far.
* Release: `bhReleaseMainTexture` (door) releases the room model, the textures of every **room
  record's** enemy (owners with `mdl[j].flg & 0x200`), objects and items. Anything loaded outside
  that list (spawned foreign enemies, display actors) is released by its loader.
  `njReleaseTextureAll` (title, game over) resets everything.

## Models and skeletons

### Structures

* `ML_WORK` (0x18): `flg` (`0x80` = chunk model; `0x200` = this instance owns the textures),
  `obj_num`, `datP`, `objP` (first `NJS_CNK_OBJECT`), `texP` (`NJS_TEXLIST`), `owP`
  (`O_WORK[obj_num]`).
* `NJS_CNK_OBJECT` (0x34): `evalflags, model, pos[3], ang[3], scl[3], child, sibling`. `pos/ang`
  are the **live pose**: motions overwrite them every frame.
* `O_WORK` (0x50): `flg` + world matrix `mtx[16]`. `flg` bits: `0x1` matrix set externally
  (`bhCalcTree` keeps it), `0x2` motion does not write this node's rotation (head look, aiming),
  `0x4` node has no motion track, `0x8` blend in YZX order (player upper arms 7 and 11).
* `BH_PWORK`: `mdl[16]` (sub-models), `mdl_n`, `mlwP` (the one drawn/animated), `skp[16]` (skin
  per sub-model), `mbp[16]`/`txp[16]` (copies of `objP`/`texP`), `mnwP` (current motion set) /
  `mnwPb` (own set), `mtn_no`, `frm_no`, link fields `lkwkp`, `lkono`, `lox/loy/loz`,
  `clp_jno[8]` (view clipping nodes), `cpcl` (hit capsules). `O_WRK` (objects, weapons, effects)
  has the same layout up to `0x3E4`, so the model functions take either.

### Binary format and hierarchy

`binfunc.c bhMlbBinRealize(bin, mlw)`: header `char[3]` + status byte, `u16[2]` header size,
`u16[3]` `obj_num`, `int[2]` texlist offset, `int[3]` object offset (relative to `bin + header
size`, `-1` = none). Objects are **one contiguous array**, relocated in place (a buffer can be
realized only once). `bhMnbBinRealize`: `NJS_MOTION` and per node an `NJS_MDATA2` pair (position
keys / rotation keys), plus optional per-frame attributes (`atrP` -> `mtn_attr`).

`MdlPut.c bhCalcModel`: base matrix = translate(`px..`) * rotateXYZ(`ax..`), or with `flg & 0x80`
the linked parent's node matrix `lkwkp->mlwP->owP[lkono].mtx` * translate(`lox..`) * rotate.
`bhCalcTree` walks the **flat array in order** (push on sibling, pop without child), so array
order is depth-first tree order and parents are implicit.

### Drawing

`MdlPut.c bhPutModel`: chunk models with skin data go through `njplus.c npCalcSkin` (or
`npCalcSkinFM` with `mdflg & 0x100`), which blends skinned vertices from the node matrices, then
`bhJumpCnkDraw[draw_tp]` draws each node's model with `owP[i].mtx`; mirror rooms
(`st_flg & 0x100`) add a mirror pass. Skin data is converted once against the model's own bind
pose (`npSkinConvert`), so it belongs to its model. `game.c bhAllDrawModel` draws room, enemy
objects, items, effects, enemies, then the player (unless `stflg & 0x1000000`). The shadow is a
blob, not model-dependent.

### Player part file

Sys AFS partition, file `ply_id + 10 + costume*4`, read by `dread.c bhReadPlayerData` (input at
`sys->memp`):

```
u32 size; model block      -> copied to sys->plmdlp
    repeated: u32 n, n bytes (an MLB, or "SKIN" skin data for the next MLB); u32 -1
u32 size; motion block     -> copied to sys->plbmtp, realized into sys->plmthp[0..]
    repeated: u32 n, n bytes (an MNB; n = 0 means an empty slot); u32 -1
u32 size; z-translation    -> copied to sys->plzmtp
per sub-model with a texlist: u32 size (bit31 = 32-byte align), PLI/TIM2 data -> bhSetMemPvpTexture
```

`O_WORK` arrays are placed behind the model data inside `plmdlp`; `sys->hd_pos = objP[5].pos`.
The loader does **not** clear `plp->skp[]`.

| File | Who | Sub-models (`mdl[i]`: nodes) | Motion slots / used |
|---|---|---|---|
| 10 | Claire (`ply_id` 0) | 0: 22 skinned body, 1/2: texlist only, 3: 22 untextured, 4: 4 skinned (hair), 5: 1, 6: 2, 7: 5 skinned | 111 / 60 |
| 11 | Chris (1) | 0: 22 skinned, 1/2: texlists, 3: 22 untextured | 105 / 55 |
| 12 | Steve (2) | as Chris | 105 / 56 |
| 13 | Wesker (3) | as Chris + 4: 1 node (sunglasses) | 106 / 55 |
| 14 | Claire, costume 1 | 0..3 as above, 4: 4 skinned (hair) | 107 / 60 |
| 15..19 | other costumes | 11-byte placeholders | |

* `mdl[0]` (drawn): **one skinned mesh on node 1**; the other nodes are bones without geometry.
  `mdl[1]`, `mdl[2]`: texlists for eye blink (`plp->mlwP->texP = plp->txp[PlyEyeTab[..]]`).
* `mdl[3]`: untextured copy with a rigid mesh per node, for line/ray hits against the player
  (`bhCollisionCheckL2MDL`).
* `mdl[4]`: Claire's hair / Wesker's piece: object `sys->obwp[2]` linked to head node 5 and
  simulated by `player.c bhCalcHair` (only `ply_id` 0 and 3).
* `mdl[6]`/`mdl[7]` (Claire): used by the moth egg (id 27: `plp->mdl[6]`, `plp->skp[7]`, player
  motions 93..95).

### The player skeleton

Identical in files 10..14:

```
0 -> 1 -> 2 -> 3 -> 4 -> 5                spine, neck, head
               3 -> 6 -> 7 -> 8 -> 9      right arm  (6, 10 are siblings of 4)
               3 -> 10 -> 11 -> 12 -> 13  left arm
          1 -> 14 -> 15 -> 16 -> 17       leg        (14, 18 are siblings of 2)
          1 -> 18 -> 19 -> 20 -> 21       leg
```

| Node | Role |
|---|---|
| 0 | root (root motion position, `clp_jno`) |
| 1 | pelvis, carries the body mesh |
| 2, 3 | spine, chest |
| 4 | neck (procedural look) |
| 5 | head (`hd_pos`, camera look-at, hair link) |
| 6..9 | right arm: shoulder, upper arm, forearm, hand (right weapon `lkono = 9`) |
| 10..13 | left arm (left weapon `lkono = 13`) |
| 14..17, 18..21 | legs (`PlyLegRoute`); 17 and 21 = feet (footstep sound/splash) |

`PlyFlip` (`player.c`) is the left/right map for mirrored motions (`mtn_md & 2`). Bone offsets
differ slightly per character; `PlyInfo[ply_id]` holds collision radius/height (Claire 16.5,
others 17.5).

### Attachments

* Weapons: `sys->obwp[0]` (right, node 9) and `[1]` (left, node 13), `flg 0x81` (linked),
  `weapon.c bhSetWeapon`; grip angles `WpnAngTab`, muzzle positions `WpnTab`.
* Enemies link to the player or to each other the same way (`flg & 0x80`, `lkwkp`, `lkono`).
* View clipping uses `clp_jno` (player 0, 5, 9, 13).
* Hard-coded player node numbers appear in ~40 places (`player.c`, `pl_evt.c`, `cut.c`,
  `weapon.c`, enemy grab code): 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 17, 19, 21.

### What makes two models animation-compatible

1. **Same node order and parentage**: motions address node `k` by position `k`.
2. **Node count >= track count**: `SetMtn*` writes `objP[k]` for every track with no bound check
   (memory corruption otherwise). Player motions have 17..22 tracks.
3. Similar bind orientation per joint (rotations are absolute local Euler angles). Proportions may
   differ: player motions carry no position keys below the root.
4. For the player: nodes 5, 9, 13, 17, 21 must be head, hands and feet, and the file should
   provide `mdl[1..3]`.

## Animation

### Playback

`Motion.c bhSetMotion(ew, add, mode, flip_tbl)`, once per 30 Hz tick, followed by
`bhCalcModel`:

* `frm_no` is 16.16 fixed point; `add` (`mtn_add`, usually 65536) advances it; wrap returns 1
  (end) or -1 (start). `mode & 1` ping-pongs.
* Track writers: *Fast* (default: root position, rotations for all nodes), *Normal*
  (`flg & 0x10`: position keys for every node that has them), *Slow* (`flg & 0x20` or `mode & 8`:
  linear interpolation by the fraction of `frm_no`).
* Blending ("hokan"): while `hokan_count > 0`, each node moves towards the target by
  `(65536 - hokan_rate) / 65536` per tick. Changing motion = set `mtn_no`, `frm_no = 0`,
  `hokan_count`, `hokan_rate`.
* `mode & 2` mirrors through `flip_tbl`; `atrP[frame]` -> `mtn_attr` (per-frame events).

### Where motion sets come from

| Set | Pointer | Source | Used for |
|---|---|---|---|
| Player base | `plp->mnwPb` = `sys->plmthp` | part file | walk/run/aim/damage/... |
| Player weapon | `plp->mnwP[100..]` | weapon file `wpnr_no + ply_id*30 + 20` (`dread.c bhReadWeaponData`) | stances and firing; weapon 0 files (and Claire's weapon 1) have no motions |
| Room | `sys->rmthp` | room file `hed02` after the enemy entries | event acting of the player and of NPCs |
| Enemy | `ep->mnwPb`, shared per type `sys->emtp[id]` | room file, one entry per record | enemy actions |
| Player in a grab | `plp->mnwP = ep->mnwP` | inside the enemy's set | the player is animated from the enemy's set, chosen by `ply_id` (zombie `{400, 425, 450, 425}`, bat 30..38, ...) |

### Player motion selection

`player.c bhControlPlayer` -> `bhCtrPly_mode0[mode0]` -> handlers `bhCPM2_act_*`. Locomotion
numbers come from `PlMtnAct[wpntp][dmlvl][k]` (`wpntp` 0 below weapon 10 else 1; `dmlvl` health
level fine/caution/danger; e.g. standing = 42/43/44); weapon stances `PlMtnWpn = {100, 104, 109,
114, 101}`. Numbers are shared by all four characters.

Enemy code sets `ep->mtn_no`, `hokan_*`, `mtn_add` and calls `bhSetMotion` itself. The first
record of a type in a room carries the set, later records reuse it; children often share the
parent's set.

## Player mesh replacement

| Option | Result |
|---|---|
| A. Another character's part file as mesh, own motions kept | works for any combination of the five player files; what "You appear as" uses for player files (`appear_glue.c self_load`) |
| B. Load another character's file while `ply_id` stays | that character's mesh **and** motions, the played character's logic and weapon files |
| C. Change `sys->ply_id` | full character switch (as `bhPlayerChangeSet`); Wesker (3) overruns 3-row enemy grab tables (`en02/en07/en13/en23 mtn[3][..]`) |
| D. Enemy/NPC mesh as player | "You appear as" (display actor, below) |
| E. Custom mesh | a new chunk model + `SKIN` weights against the 22 bones, `mdl[1..3]`, TIM2 textures with unused `Gindex`/CLUT banks; no tool exists |

### Option A

`appear_glue.c self_load` (our player with a player file as "You appear as"):

1. Save the original's `mdl[]`, `skp[]`, `mbp[]`, `txp[]`; clear `plp->skp[]` (the loader would
   apply stale skin pointers).
2. Read the part file and run the game's loader (`player_model_read`, the
   `bhReadPlayerData` layout) with the model going into a buffer of its own (`self_mdl`, 128 KB)
   and the motions into a sink; restore `plp->mnwP`/`mnwPb`, so the original motions (incl. 100+)
   stay. The original model data in `sys->plmdlp` and its textures stay loaded.
3. Sub-models only the original has (Claire's 5..7: the moth egg takes `mdl[6]`, `skp[7]`) keep
   the original's; `owP[7]`/`owP[11]` get `flg |= 0x8` as in `bhSetPlayer`; the hair/sunglasses
   object is set up for the new mesh. Putting the original back copies the saved `ML_WORK`s.

* When the game reloads the player (game start, load, continue, story switch) the mesh is applied
  again at the next frame in control. The original is put back before the game lets go of the
  player: when a game ends (`appear_textures_released`) and before a character switch (the story's,
  each Battle Game room) releases the player's textures (`system.c bhSysCallMonitor`,
  `appear_player_unload`). The game then releases the original's textures and we the mesh's; the
  original's texture lists are gone after that (the new load overwrites them).
* Keeping `sys->ply_id` keeps everything else: `PlyInfo` size, weapons (they sit in the new hand
  node 9), inventory, enemy grab motions, room event motions. The voice follows the model shown
  ("Voices").
* The loader never clears `mdl[]` entries beyond the new `mdl_n`; stale entries must not be used.
* Cutscenes: op 52 `ObjLinkSetPly` re-attaches `sys->obwp[2]` at Claire's ponytail offset for
  every character, and 8 room motions (rooms 9090, 9370) carry position keys below the root.
  `appear_glue.c self_keep` puts the head part's own link fields and the mesh's own node offsets
  1..21 back every tick.
* Changing the mesh in game: `bhFixPosition` (motions that plant a foot) moves the player by the
  difference between last tick's toe matrix and the new pose's; `self_carry_pose` gives the new
  model the old one's node angles and root offset and computes its matrices at the player's base
  matrix, so the player does not jump.

## Enemies

Room records, spawning and budgets: [ENEMIES.md](ENEMIES.md).

### Data flow

Room file records (`ETTY_WORK`/`EGG_WORK` 0x24: `flg`, `id` (type), `type` (variant), `flr_no`,
`mdlver` (model version), `wrk_no` (slot), position, angles, `hide[4]`) are realized by
`room.c bhSetRoom` -> `bhSetEneMdl` -> `eneset.c bhSetEnemy` (first free slot). Model chunks:
`"MASK"` -> face mask `mskp`, `"SKIN"` -> `skp`, else an MLB into `ep->mdl[]`. Textures are shared
per `et_lp[id][mdlver][k]`; the first record owns them (`flg |= 0x200`). `bhFinishRoom` adds the
motion set (`sys->emtp[id]`), `O_WORK`s and `bhInitMask` for ids > 90.

### Enemy types

`bhJumpEnemy` (100 entries, `eneset.c`); code in `enNN.c` unless noted, names from the
`// ENEMY:` comments:

| id | Name | id | Name |
|---|---|---|---|
| 0 | exploding static object, wall collider (`eneset.c bhEne00`) | 17 | Monster Steve |
| 1 | Zombie (16 variants; parts as helpers 31..38) | 18 | Alexia's Tentacle |
| 2 | Gulp Worm | 19 | Tyrant T-078 |
| 3 | Black Widow (helpers 31, babies 24) | 21, 22 | Albinoid Infant, Adult |
| 4 | Cerberus | 23 | Giant Black Widow |
| 5 | Hunter/Sweeper | 24 | Baby Black Widow |
| 6 | Moth (lays 27) | 25 | Ceiling Trap |
| 7 | Bat | 26 | Anatomist Zombie |
| 8 | Fish | 27 | Moth Egg (on the player; player model/motions) |
| 9 | Bandersnatch | 28 | empty (`subpl.c`) |
| 10, 20, 71 | Ant A, B, C | 29 | Tentacle |
| 11 | Spotter | 30 | Alexia's Baby |
| 12, 13, 14 | Alexia first, second (spawns 18), third form | 53 | Nosferatu's core (`en15.c bhEne53`) |
| 15 | Nosferatu | 54, 55 | Armored Knight A (sword), B (lance) |
| 16 | Alfred Ashford | 56 | scripted object (`eneset.c bhEne56`) |

31..52, 57..70, 72..99: `subpl.c bhSubpl`, scripted NPC actors (room-script motions, no AI);
41..48 and 91..98 get player-like clip tables, ids > 90 face masks and lip sync.

Slots 31..38 are overwritten at run time with helper functions (`bhEne_SetCallFunc`): 31 by ids
2, 3, 5, 6, 13, 23 (17: 31, 32), 31..38 by id 1. On the PS2 the table is global and never
restored; the PC runs each part with its creator's function in rooms with added enemies
([ENEMIES.md](ENEMIES.md) "Budgets on the PC"). Ids 18, 27, 28 never appear in room data.

### Cloning a type the room already loaded

`bhSetEnemy` on a copy of an existing record of the type, then share its `mdl[]` (clear
`flg & 0x200`), give each sub-model its own `owP` (`bhEne_CallocWork`), copy
`skp`/`mbp`/`txp`/`mskp`, set `mnwP = mnwPb = sys->emtp[id]` and `bhCalcModel`; Init runs on the
next tick. Children share `objP` the same way (`en03.c`). Foreign types go through the spawner's
loader ([ENEMIES.md](ENEMIES.md) "Enemy spawner (F2)").

### Items and flags (for randomizers)

* Items: room records `rom->itmp` -> `sys->itwp[i]` (`objitm.c bhSetItem`, `id` = inventory id
  0..155). Pickup: an `etc` atari of type 4 whose `prm0` is the item slot (`sys->sb_id =
  itwp[prm0].id`). Ops 14 `bhItmCk` / 35 `bhItmSetCk` bind the `it_flg` bit to the trigger, not the
  id, so changing `itwp[i].id` after load keeps persistence (the 3D model stays the record's).
* Locks are room-script checks (ops 16 `bhUseItemCheck`, 17 `bhPlItemCheck`, 77
  `bhPlItemBlockCk`, `ky_flg`); `map.c ItmDat` lists 22 key items. Flags (`SYS_WORK`): `ev_flg`
  story, `it_flg` items, `ed_flg` dead enemies, `ky_flg` locks, `mp_flg` map, `ic_flg` item checks.

## You appear as

"You appear as" (F1 and ENHANCED SETTINGS -> APPEARANCE; ini `appear_char`) shows
our player as one of the player files or one of 62 models found in the room files: the story
cast, the creatures, the zombie variants (`pc/src/host/actors.c`). It applies in play and
in cutscenes to the event double of the character played
(unless "Swapped model in cutscenes" is off, see "Cutscene doubles").

### The list

The menus list one entry per distinct look: "As played", the five player files, 50 catalogue
models and, last, the player characters' four cutscene models (60 entries instead of 68). They share
`actors.c` `appear_count`, `appear_at`, `appear_pos`, `appear_step`, `appear_name`, `appear_canon`;
the list is filtered and ordered for display only, so catalogue indices, `g_appear_chr`
and the cutscene doubles are unaffected. A hidden value in `recvx.ini` is
mapped to its listed twin when the file is read. Test: ctest `appear` (`pc/tests/test_appear.c`).

Left out of the menus (`twins[]`):

| Hidden (catalogue index) | Shown instead | Why |
|---|---|---|
| Chris (event model) 2 | Chris | same look |
| Steve (event model) 4 | Steve | same look |
| Wesker (event models) 6, 7 | Wesker | same look; the doubles wear the sunglasses as a separate prop |
| Alfred Ashford 2 (9), (event model) 11 | Alfred Ashford (8) | same uniform |
| Tyrant 2 (18) | Tyrant (17) | one extra blood stain |
| Bandersnatch 2 (23) | Bandersnatch (22) | only its long arm (event piece) |

Kept although related: "Alfred Ashford (wounded)", "Hunter (Sweeper)" and every zombie variant.
Order: As played, the player files, the cast, the creatures, the zombies, then the cutscene models
of Claire (0), Chris (1), Steve (3) and Wesker (5) (`tail[]`): listed next to the player files,
each would follow the same character's file. They are the event doubles' meshes: no hands (the
unarmed pieces are drawn, as for the rest of the cast), and no head part of their own (in
cutscenes the scripts link the player's `sys->obwp[2]` to the double); the display actor brings
Claire's ponytail and Wesker's sunglasses itself ("Head parts" below). Earlier versions hid them as
twins of the player files; their stored values stayed valid.

### Head parts

Claire's cutscene model is shown with her ponytail, Wesker's (and his hidden event models) with his
sunglasses, wherever a display actor shows them: our player and the
cutscene double ("Swapped model in cutscenes" on) (`actor_glue.c load_head`,
`head_pose`, `head_draw`).

* **Source**: sub-model 4 of the character's part file (`lkmtab`: 1211 the ponytail, 1212 the
  sunglasses; Claire's costume 0, file 10, and Wesker's, file 13), read when the model is loaded,
  whatever character is played: only its skin and model chunks are kept (Claire 1.4 KB, Wesker
  2.3 KB, into the display slot's own buffer) and only its texture block is uploaded (Claire 128 KB,
  Wesker 5 KB, one texture entry; refused like a model under the margins of "Loading"). The
  player's own model and `sys->obwp` are not touched; the part file goes through the loader's
  `pack` buffer.
* **Set up** as `bhSetPlayer` does (`player_hair`: offsets on head node 5, Claire's skin), in an
  `O_WRK` of the display slot, linked to the display actor's work (all four cast models have their
  head at node 5, which `bhObjClpn` reads). Per pose its position follows the model's head; the
  ponytail runs the game's own simulation (`bhObjClpn`, `bhCalcHair`: the same joints, from the
  display actor's retargeted angles) once per tick (our player: the pose after
  control; the double: `actor_double_tick`; of two doubles shown the first steps it, the other
  shows the same swing), in work of its own (`exp0`/`exp3`, 20 KB per slot,
  flag `0x100000`), so it never takes `bhGetFreeMemory` (the earlier crash: memory under a pending
  weapon read). Drawn right after the display actor.
* **Memory**: per display slot (3) 34 KB of static buffers (103 KB in all, not the game's arena); per shown head part one texture
  entry and the texture above; one part-file read (about 1.2 MB, the existing buffer) per model
  change. Released with the model (`unload`, `njReleaseTextureAll`: game over, the title).
* **The played character's own cutscene model** (Claire's while Claire is played): the doubles
  stay the game's (see "Cutscene doubles"), and while one acts `sys->obwp[2]` is the character's
  own head part linked by the script (`self_cine_hair`, as with "Swapped model in cutscenes" off,
  `actor_double_kept`); in gameplay the display actor's copy is shown.
* Not covered: a script's InitPony (`flg2 & 0x2`) does not reset the copy; a teleport swings it once.

Models not in the catalogue:

| Model (id) | Why |
|---|---|
| Alexia second form (13) | the body is one of 14 linked sub-models |
| Alexia third form (14), Gulp Worm (2), Tentacle (29) | arena-sized or below the floor |
| Armored knights (54, 55) | armour pieces scattered on the floor in their idle motions |
| Alexia's baby (30) | a thin stalk in motions 0 and 1 |
| Ant A (10), Ant C (71), baby Black Widow (24), Nosferatu's core (53) | tiny, no model, or nothing visible |
| NPC 68 | a black figure without a head |
| ids 0, 16, 25, 56, 58..63, 65..67, 69, 70, 72..79, 81..85, 87..90 | props and set pieces |

### How

* **Catalogue** (`actors.c`): per model the room file and enemy record that carry it (the
  smallest room holding the model and its type's motion set), type and model version (checked at
  load), animation set (below) and idle motion. Only ids, no disc data.
* **Loading** (`pc/glue/actor_glue.c`): the room file is read from `RDX_LNK.AFS` and expanded into
  a static 12 MB buffer. The record's model chunk list (`hed01`) and its type's motion list
  (`hed02`) are copied into the display slot's buffer (1.5 MB) and realized as `bhSetEneMdl` /
  `bhSetEneMtn` do. Textures: the table (`hed04`) has, after the room model's entry, one entry per
  first (type, model version, textured sub-model) in `bhSetRoom` order; the loader replays that
  order and uploads its model's entries (a model the room already shows costs nothing). Player
  part files use `bhReadPlayerData`'s layout (one loader: `afs_read`, `player_model_read`,
  `player_hair`). Released when the model changes; refused (the character's own model stays) with
  fewer than 32 texture entries or 2 MB of heap left.
* **Display actor**: a `BH_PWORK` per slot (our player, the cutscene double, and
  two for enemy variants' models: [ENEMIES.md](ENEMIES.md) "Enemy variants", posed from each such
  enemy's skeleton, or from another enemy's (a donor) playing a borrowed motion, when it is drawn),
  drawn in place of the source (`game.c actor_put_player`,
  `eneset.c actor_put_enemy`). The source stays what the game animates, collides with and links
  weapons and effects to; it is only not drawn. The head part (`sys->obwp[2]`) is disabled while a
  catalogue model is shown.
* **Humanoid retarget** (the cast, Alexia's first form, zombies, Tyrant, Hunter, Nosferatu,
  Bandersnatch, Monster Steve, the anatomist; all with bind angles 0 and a T-pose, like the
  player): a node map is built from the rest skeleton (`rig_build`): pelvis = node 1; legs =
  chains going down (hip, knee, ankle, toe); spine = up to the first node with a sideways child
  (chest); neck/head above it; arms = sideways chains from the chest or a spine node below it
  (clavicle if the first bone is shorter than 1 unit). A child at its parent's own position (a
  zero-length bone, e.g. Rodrigo's and the hazmat man's shoulders) is passed through
  (`rig_next`); a sideways chain that ends further up or down than out (tentacle, tail) stays
  unmapped. The source skeleton is mapped the same way. Every tick, per mapped node: model world
  rotation = rest correction x source world rotation; positions come from the model's own bone
  offsets, the root from the source's (feet on the floor). Unmapped nodes follow their parent
  rigidly. Details: "Retargeting" below.
* **Joint blending**: the meshes are skinned (`npCalcSkin` blends a vertex near a joint between
  the node's matrix and that matrix with the node's angles undone), so each node's angles are set
  to its local rotation (world x parent world^T, decomposed as `njRotateXYZ` builds it).
* **Other models**: their own idle motion (motion 0) looped, gliding with the player (bat, moth,
  spiders, Cerberus, albinoids, fish, Spotter, ant).
* **When it runs**: `game.c` calls `appear_control` (`pc/glue/appear_glue.c`) while `appear_on`
  (an appearance other than the character played is chosen or still shown); with "As played"
  nothing runs.
* **Game over**: `njReleaseTextureAll` unloads the display actors; `appear_textures_released`
  clears the "player in control" state, so the appearance returns only once the player is back.

### Gender-matched animations

Setting "Use gender-matched animations" (APPEARANCE page, ini `appear_gender_anim`, default 1): a man
shown while Claire is played moves with **Chris's** motions; a woman shown while a man is played
moves with **Claire's**. Gameplay stays the played character's.

The catalogue's `anim` field picks the set (`appear_motion_chr(anim, ply_id)`; player files by
their file, `appear_part_anim`):

| `anim` | Models |
|---|---|
| `ANIM_MALE` | Chris, Steve, Wesker (all files and models), Alfred (all), Rodrigo, the hazmat man, Nosferatu, Tyrant, Monster Steve, Hunter, Bandersnatch, the anatomist, every zombie |
| `ANIM_FEMALE` | Claire (player files, cutscene model), Alexia Ashford, Alexia's first form |
| `ANIM_IDLE` (own idle) | Cerberus, albinoids, Black Widows, moth, bat, fish, Spotter, ant |

* **Motion source** (`actor_glue.c`): for display slot 0 (our player) a hidden copy
  of the source's 22-node skeleton with its own motion set: part file `10 + chr` and weapon file
  `wpn + chr * 30 + 20` (only the weapon file is re-read on a weapon change), 500 KB per slot.
  Every tick after the source's motion the copy plays the same motion number at the source's
  phase, with its `mtn_md` and flip table, blending whenever the source blends; neck and head
  take the source's angles; the third-person aim bend applies too (`tps_bend`). A slot the other
  character lacks (Claire's 90..97) and room-set acting play the source's own motion. In a grab
  `appear_grab_motion` picks the other character's motion of the enemy's set (zombie
  400/425/450/425, Gulp Worm 21/29, Black Widow 14 * `ply_id`, Cerberus 100/105, bat 30/33/36,
  Bandersnatch 100/110/120, Alexia 2 10/16, Giant Black Widow 60/66).
* **The frame**: the phase is that of the frame the source **shows** this tick (`actor_frame`), not
  `frm_no`, which its `bhSetMotion` has already advanced: `player.c bhControlPlayer` records the
  frame and blend count just before (`pc_plp_frm`, `pc_plp_hokan`). Played one frame ahead, a
  motion the game leaves on its last frame had wrapped to its first, so the copy showed the start
  of the motion for a tick: after every shot the male fire motion's recoil (the twitch after a
  handgun shot, any weapon; also the start of zombie grabs and a quick turn). A blend the game starts
  within a motion (the end of raising a weapon: `hokan_count` 1 on the same motion) is copied too.
  The recorded frame counts only while the motion and set are still the ones recorded
  (`pc_plp_mtn`, `pc_plp_set`): enemy code runs after the player and may start another motion (a
  grab, Alexia's burn), whose own `frm_no` and blend are then taken, never the old motion's frame.
* **Display**: a catalogue model is retargeted from the copy; a player-file mesh is drawn with the
  copy's matrices and angles (`actor_put_body`), and the source's nodes 5, 9, 13 get the copy's
  matrices before the objects follow, so head part and weapons sit where they are drawn.
* Not covered: cutscene doubles keep their source's motions; positions stay
  the real player's; a switch between the matched set and a fallback does not blend.

### Retargeting

Each model keeps its own skeleton: bone lengths, rest pose and proportions are its own, only the
joint rotations come from the source (`actor_glue.c rig_build`, `retarget`).

* **Rest poses**: computed once at load from the model's node tree (`rig_build`: parents and
  model-space rest positions; all bind angles are 0, so every rest orientation is the identity, as
  the player's). The source's rest pose is its current node offsets (the player files' bones).
* **Rotations**: per mapped joint, the model's rotation is the source's rotation relative to its rest
  pose, turned by a rest correction: the rotation taking the model's rest bone (towards the next
  mapped joint of the chain) onto the source's. In local terms: model local = C(node) x source local x
  C(parent)^-1, so a model whose arm rests at another angle (an arm lower or more forward, a hunched
  spine) bends at the same joints by the same amounts without being pulled to Claire's bone lengths.
  A chain end (hand, toe) takes its parent bone's correction. Node angles are written back as
  `njRotateXYZ` builds them, so the skin blends joints (`npCalcSkin`).
* **Limb roles**: an arm runs from its first node sideways from the chest to the hand: the last node,
  or the first with two or more children whose subtree is short against the arm (fingers, claws; a
  spike on the Tyrant's forearm is passed). Clavicle, shoulder and elbow are the nodes nearest to where
  the player has them along the arm (3%, 35% and 68% of its length from the chest), in order, so a
  long arm (the Bandersnatch's six joints) has its hand at its claw and an arm without clavicle
  (Hunter) maps from the shoulder. A leg of more than four joints (Monster Steve) keeps hip, toe and
  ankle at its ends and the knee at mid-leg. Joints between mapped ones follow their parent rigidly.
* **Root and feet**: the root's height over the floor is scaled by the hips' heights (crouching,
  kneeling, the walk's bob), and the body is then moved up or down so that its lowest ankle or toe is
  as far over its rest height as the source's (in proportion; at most 3 units): feet of another shape
  (a Hunter's long foot, Monster Steve's claw) stay on the floor. Only the height: room motions move
  cutscene doubles across the room with the root.
* **Post-passes**: the two-handed grip IK and the lighter IK (below), the face mask's jaw, tongue and
  eyes, the cutscene doubles' hand pieces, as before.
* **A missing hand**: what the game links to a source hand the model does not have (Nosferatu's right
  hand, the Bandersnatch's left) is shrunk to a point instead of floating where the source's hand is.
* **Free aim** ([THIRDPERSON.md](THIRDPERSON.md) "Free-aim pose"): the bend is solved on the weapon as
  drawn (`actor_aim_hand`; only while the aim pose stands still, eased, not for the knife; a solve
  that stops converging, a target a few units from the muzzle, keeps its best step), and the third-person camera's height, shoulder offset and distance scale
  with the model's head height over the character's (`actor_cam_scale`, 0.8 .. 1.8): the camera is no
  longer inside a Tyrant or Monster Steve.
* **Previous retargeting**: `RECVX_RETARGET=0` (environment, read at start) keeps the earlier mapping
  (fixed chain positions, clavicle by length), the unscaled root and the aim taken from the unbent
  hand; frames are then identical to the previous version's. The character played without a
  model never goes through this code.
* Test aid `RECVX_RIG_DUMP=1`: logs each model's rest skeleton, the source's map and skeleton, and per
  tick the source's and the model's lowest foot over the floor.

Why aiming up looked broken before: the free-aim bend took its pitch from the hand of the unbent pose
and turned the upper body about the chest, which swings a hand far from the chest (a tall model's)
off the line (a drawn barrel up to 11 degrees off a near target); the camera stayed at Claire's
height, inside tall models; the old map took Monster Steve's clavicle for his upper arm (his weapon
at his elbow, his feet 4 units over the floor) and the Bandersnatch's mid-arm for its hand; the
weapon of a hand the model lacks floated at the invisible source's hand.

### Hands, joints, lighter and aim

* **Weapon hands** (`actor_pose`): the source's hand nodes 9/13 get the model's hand position and
  the copy's hand rotation for the tick's effects, objects and drawing, so weapons sit in the
  model's hands and the muzzle flash starts there.
* **Two-handed grips** (`retarget`, `arm_ik`): when the source's left hand is near its right
  (closer than 6 units, full below 3), the model's left hand gets a target at its right hand +
  (source left - source right), solved by a two-bone IK on the model's own bone lengths (elbow
  kept in its bend plane). One-handed poses are untouched.
* **The shot** (`actor_hands_back`, game.c before `bhControlPlayer`): the game's own matrices of
  nodes 5, 9, 13 are saved before they are moved and put back before the next player control, so
  the game fires (`bhCheckGunAtari` from the muzzle at `owP[9]`), swings the knife and places the
  lighter light from the real skeleton; only the drawing uses the model's hands. Third-person aim:
  [THIRDPERSON.md](THIRDPERSON.md) "Free-aim pose".
* **Lighter**: the game holds the lighter by an arm IK after the motion (`bhArmIkMdk` on node 6
  towards a point in front of chest node 4, nodes 7..9 flagged 2). The same IK runs on the
  gender-matched copy whenever the source's arm is flagged.
* Known: the Tyrant's weapon is lost inside its big claw hand (it sits at the wrist node); in
  Chris's two-handed stance the left palm lies under the gun on every male model (the motion's
  own grip).

### Cutscene doubles

* Pre-rendered movies cannot change (the opening, the ending and 17 in-room movies started by op
  94 `MovieStart`). 142 rooms have in-engine cutscenes. In 77 of them the character played is the
  player model itself, and the appearance applies as everywhere. In the others the script hides
  the player and an **event double** acts: an enemy record of the story cast with a face mask
  (Claire 91, Chris 92 / 42, Steve 93 / 43, Wesker 97 / 49 / 47).
* `eneset.c bhDrawEnemy` calls `actor_put_enemy` instead of `bhPutModel(ep)`: a double of
  `sys->ply_id` is drawn as the chosen model (display slot `ACTOR_DOUBLE`), posed from the double's
  skeleton. The model is loaded at the hand-off while the room has such a double (player files
  too: `ACTOR_PART + n` loads part file `10 + n` as a display model). Creatures without a humanoid
  node map do not replace doubles.
* The played character's own cutscene model chosen (Claire's while Claire is played) replaces no
  double (`actor_doubles`): the double is that model already, with its face, hand pieces and the
  linked head part.
* **Hands are objects**: the cast's cutscene models have no hands; the room script links hand
  pieces (fist, open hand, holding poses) and props to the double's hand nodes and swaps them per
  gesture. `actor_double_tick` (from `appear_control`, after `bhControlEnemy`, before
  `bhControlObjItm`) poses the double's model and moves the double's hand nodes to the model's
  hand positions, so pieces and props sit on the model's wrists. The pieces stay the double's
  (Claire's gloved hands on Steve). Two doubles of the played character are each drawn in their
  own pose (`dbl_posed`).
* **Talking faces** (setting "Talking faces in cutscenes (experimental)", APPEARANCE page, ini
  `appear_cutscene_face`, default 0): the cast talks through a face mask (`face.c`, `face_bh.c`,
  the model's `MASK` chunk; `fmCnkInitContext` inserts jaw/tongue/eye pivots, 27 -> 31 nodes;
  op `bhLipSet` starts a line of phonemes from the model's face table). With the option a cast
  model shown for the double gets its own mask (`face_init`, ~55 KB in `face_mem`) and the
  double's lip state is copied onto it every tick (`face_step`). Lines past the model's face
  table are not shown; expressions are not copied. A player file chosen as the appearance is shown
  on the double as the same character's cutscene model. `face_src` copies rest meshes to 64-byte
  boundaries (the morph reads them with `lq`). A mask that does not fit leaves the model without.
* **Swapped model in cutscenes** (APPEARANCE page and F1, ini `appear_in_cutscenes`, default 1). Off: the
  doubles stay the game's own (the real Claire or Chris, their hand pieces, face mask, lip sync);
  the chosen model is shown only while the player itself is. What counts as a cutscene is the
  double: a scene (story cutscene or a short in-game event) counts while an event double of the
  character played is shown (`actor_double_acting`: an enemy record of `doubles[ply_id]`, active,
  not hidden). Scenes the player model acts in itself (the 77 rooms above, room motions on the
  player) keep the chosen model, as gameplay does. How:
  * `actor_doubles` loads no model for `ACTOR_DOUBLE`, so `actor_put_enemy` and
    `actor_double_tick` leave the doubles alone (no hand or face work at all).
  * The head part: the scripts link the player's `sys->obwp[2]` (Claire's ponytail) to the double
    (`ObjLinkSet`), and the appearance had disabled it (catalogue model) or replaced it (player
    file). `appear_glue.c self_cine_hair`, every tick before the objects follow their links, sets
    up the played character's own head part (from `self_orig` under a player file) when a double
    starts acting and the chosen model's again when none does; the script's link (work, node, x
    offset) is kept. The ponytail's work is the player's (`pletcp`, as `bhObjClpn` takes it when
    linked to the player): linked to the double it would come from `sys->memp`, which broke a
    pending disc read (weapon change). Its swing restarts at the cutscene's start.
  * Voices: `pc_player_voice` keeps the played character's voice while a double acts. Cutscene
    dialogue is streamed and never swapped.

### Voices

The player's vocal sounds follow the model shown: hit reactions, death, grab and struggle
screams, and what room scripts play as the player's voice (not cutscene dialogue, which is
streamed ADX).

**Where they come from.** Every player vocal sound is SE bank 4 (`0x400 + program`) through
`sdfunc.c CallPlayerVoice` (slot 7): damage and death (`player.c bhCPM0_damage`, `bhCPM0_die`),
enemy grabs and room scripts (`PlayGameSe4Event` type 0). Bank 4 is `CORE_<ply_id>.SPQ` (AFS key
`0xFFF0 | ply_id` in MULTSPQ1/2); the disc has exactly four (Claire, Chris, Steve, Wesker) with the
same 4 programs each. No other character has player vocal sounds.

**Mapping** (`actors.c appear_voice`, ctest `appear`):

| Shown as | Voice |
|---|---|
| As played | the character played |
| Claire (all files and doubles) | Claire (`CORE_000`) |
| Chris and his doubles | Chris (`CORE_001`) |
| Steve and his doubles, Monster Steve | Steve (`CORE_002`) |
| Wesker and his doubles | Wesker (`CORE_003`) |
| other men (male `anim`): Alfred, Rodrigo, the hazmat man, Nosferatu, Tyrant, Hunter, Bandersnatch, the anatomist, every zombie | Chris |
| women (female `anim`): Alexia, Alexia's first form | Claire |
| creatures with their own idle | the character played |

Creatures keep a human voice: enemy sounds are motion- and table-driven per type, list numbers
collide between types ([ENEMIES.md](ENEMIES.md) "Sounds of added types"), none is labelled hurt or
death, and they play through the enemy sound slots.

**How.** `CallPlayerVoice` (TARGET_PC) asks `appear_glue.c pc_player_voice(se)`. For the played
character's voice it returns 0 and the request is the game's own. Otherwise it loads that
`CORE_00n.SPQ` bank 4 as an extra bank (`snd_iop.c snd_xbank_load(0xFFF0 + n)`, the loader of the
spawner's enemy banks) and names it in the port byte (`pc_se_xbank`, bits 3..7), so the synth
plays it on the same port, slot, volume and pan. The game's bank 4 is never touched. The extra
bank is released with the room (`spawn_room_setup`) and loaded again at the next voice.

**Memory.** A voice bank is 0.7 KB header + 38..54 KB samples, one at a time, taken from the same 31 extra-bank numbers as the spawner's enemy banks.

### Limits

* Skinned areas near joints whose rest orientation differs (zombie shoulders in T vs arms down)
  stretch a little; the motions are the player's, so a body of other proportions strikes the same
  angles (a Tyrant's long arms reach further); grips sit at the model's wrist. Creatures do not scale.
* Per model: Nosferatu has no right hand (its weapon is not drawn) and its spine has no mapped lower
  node; the Bandersnatch has one arm (no two-handed grip) and no head node; the Tyrant's weapon sits
  in its claw; the hazmat man's and the zombies' torso is one bone from the hips; the camera scales by
  the neck where no head is found (the Bandersnatch).
* No creature sounds (footsteps stay the player's; the voice per "Voices").
* Lip sync only for cast models and with the option; the double's hand pieces are its own.
* A room with little texture memory left refuses the model (logged as `actor: ... not enough
  texture memory`). Wrong colours are possible where a model's CLUT bank collides with the room's.
* Test aid: `RECVX_APPEAR="frame:chr,..."` sets the appearance at a frame.

## Limits

Budgets for enemies (slots, arena, textures, sounds): [ENEMIES.md](ENEMIES.md) "Budgets on the PC".

| Resource | Limit | Where |
|---|---|---|
| Objects / items | 32 `obwp` (0..3 player), 32 `itwp` | `system.c bhFirstGameStart` |
| Effects | 512 `eff[]`, 450 effect models `sys->efm` | `main.c`, `SYS_WORK` |
| Enemy types | 100 jump entries; textures `et_lp[100][16][16]` | `eneset.c`, `room.c` |
| Motions | 512 per set; 64 tracks per motion (`NormalTbl`), at most the model's nodes | `Motion.c` |
| Sub-models | 16 per actor (`mdl[16]`) | `BH_PWORK` |
| Player buffers | model 128 KB, base motions 384 KB, weapon motions 64 KB | `bhInitPlayer` |
