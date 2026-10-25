# User guide

How to start the game, configure it and play. Building: [BUILDING.md](BUILDING.md).

## First start

You need your own disc image of *Resident Evil Code: Veronica X* (PS2, NTSC-U, **SLUS-20184**)
as an `.iso` file. Other regions and releases are not supported. The image is read in place;
nothing is extracted or copied.

- Release downloads: unpack the archive and run `recvx.exe` (Windows) or `./recvx` (Linux).
  Settings, saves (`save/`), `mods/` and logs live next to the program, wherever it is started
  from; a `save/` folder in the current directory, or `--save-dir`, takes precedence.
- Start `recvx` (Linux) or `recvx.exe` (Windows). The first start opens a file selector (on
  Linux the desktop portal or `zenity`); pick the image. The choice is saved in `recvx.ini`
  (`iso = ...`) and the selector only comes back if that file disappears.
- Alternatively pass the image on the command line (`recvx /path/to/game.iso`), set
  `RECVX_ISO=/path/to/game.iso`, or on Windows drop the ISO onto the exe. An image given this way
  always wins over the saved one.
- The Windows build is a console program: the console window shows the log.

### Command-line options

| Option | Effect |
|---|---|
| `--save-dir DIR` | where saves and `recvx.ini` go (default `./save`) |
| `--size WxH` | window size (default 1280x960) |
| `--fullscreen` | start in fullscreen, of the kind chosen in Display mode (borderless unless you chose exclusive) |
| `--borderless`, `--exclusive`, `--windowed` | start in that display mode |
| `--enhanced` / `--vanilla` | display-rate frames on / off |
| `--skip-doors` | room transitions without door, stairs and ladder animations |
| `--skip-intro` | boot straight into the title menu |
| `--third-person` | start with the over-the-shoulder camera ([THIRDPERSON.md](THIRDPERSON.md)) |
| `--log[=FILE]` | copy all output to a log file (default `recvx.log`), with a backtrace on a crash |
| `--profile[=FILE]`, `--profile-samples=FILE` | performance measurements ([ARCHITECTURE.md](ARCHITECTURE.md#profiling-and-logging)) |
| `--quiet` | less console output |
| `--version` | print the version and exit (also the first line of the log and the F1 window title) |

Command-line options override the saved settings for that run.

## Saves and settings files

Everything lives in the save directory (`./save` relative to the current working directory,
or `--save-dir`):

| Path | Contents |
|---|---|
| `save/saves/` | your saves as files (the default save system, see below) |
| `save/saves/system.dat` | the game's system data: OPTION settings, unlocked extras, best times |
| `save/card0/` | memory card slot 1 (the game's own save files, used by the Memory card save system); slot 2 is empty |
| `save/recvx.ini` | all settings, key bindings and the disc image path |

`recvx.ini` and the saves are written atomically (a crash never leaves a half-written file). Two
copies of the game with different save directories keep separate settings and saves.

### Saves

The typewriter's save screen and LOAD GAME look as on the PS2, but by default (**Save system:
Native files**) they list save files instead of the memory card's 15 slots, without the memory
card screens ("which memory card slot", "checking memory card", "not enough space"); the list's
heading reads "Load Game" or "Save Game" instead of "MEMORY CARD slot 1":

- Each save is its own file in `save/saves`, named by its number and what the menu shows, e.g.
  `001_Claire_Prison-front.sav`, `002_Chris_Ant-Base-CR.sav`, ... `1000_...` There is no limit.
  The list scrolls through all of them; saving offers an empty entry with the next number at the
  end of the list (the cursor starts there), or you overwrite an existing save as before. Loading
  starts on the save written last.
- A save keeps its difficulty (ENHANCED SETTINGS GAMEPLAY "Difficulty select"): the list shows
  Easy saves in green and Very Easy saves in red, Normal in white, with either save system.
- The file name is only for you: renaming the part after the number does no harm. Deleting a file
  deletes that save. Back them up by copying the folder.
- A file is the memory card's save file with a 64-byte header in front (game, time, play time,
  character, room), so a save can be turned back into a card save by dropping the first 64 bytes
  ([ARCHITECTURE.md](ARCHITECTURE.md#save-system)).
- **First start with native files**: your memory card saves are copied into `save/saves` (slot n
  becomes save number n) together with the system data; the log says how many. The memory card files are kept as they are.
- **Save system: Memory card (original)** (`save_system = 1`) brings back the emulated memory card
  with its screens and 15 slots, using `save/card0` exactly as before. The setting applies from the
  next save or load screen. Saves made in one system are not seen by the other; **Import memory
  card saves** (F1 / ENHANCED SETTINGS GAMEPLAY) copies the card's saves into the save files again (ones
  already there are skipped).

### Back to the title

During a game, the OPTION screen (Select) has **RETURN TO TITLE** on DEFAULT's row, above ENHANCED
SETTINGS: select DEFAULT and press right, or click it. It asks "Return to the title screen?" with
NO selected, so a stray press does nothing; choose YES (left/right, then confirm) to go back to the
title menu, as after a game over. Anything not saved is lost; changed options are kept. Cancel or a
right click answers NO. It works in the Battle Game too. The OPTION screen of the title menu does
not have it.

### Skipping the ending

The ending movie and the credits can be skipped, which the PS2 version does not allow: any key,
mouse button or gamepad button fades the ending movie out, and another press does the same for the
credits. Only a press made while the movie is on screen counts, so a button still held from the last
fight skips nothing. The results screen, what it unlocks (Battle Game, the rocket launcher for an A
rank), the save of the system data and the return to the title come as after watching to the end.
The total time still counts the skipped part, so the rank is the same either way.

## Settings

All PC settings are available in two places that edit the same values:

- **OPTION -> ENHANCED SETTINGS**: in the game's own OPTION screen (title menu, or Select in
  game) next to EXIT: select EXIT and press right (or click it). Drawn in the game's style, with
  the pages DISPLAY, GAMEPLAY, CONTROLS, THIRD PERSON, AUDIO, APPEARANCE and EXTRAS. L1/R1
  (LB/RB, keyboard Q/W, as the page row names them) switch pages from anywhere, both ways round
  the seven, as do left/right on the page row; up/down (or the mouse wheel) choose a setting, left/right
  change it, cancel or a right click goes back. Long pages scroll; the bottom lines explain the
  selected setting, or why it cannot change right now. "(next start)" marks a setting that takes
  effect when the game starts again.
- **F1**: the settings overlay, in the same order (sections Display, Gameplay, Controls, Third
  person, Audio, Appearance, Performance). The cheat settings live in their own window (F2); F1
  shows a few of them.

Every setting of the table below is in both. Changes apply at once and are saved shortly after.
The defaults of a first start are the PC-friendly ones: enhanced mode, widescreen, native
renderer, skipped intro and door animations (Skip door animations: Instant) and fast loading. Turn these off for a presentation closer to the PS2.

### Display

| Setting | ini key | Values (default in bold) |
|---|---|---|
| **Window** | | |
| Display mode | `fullscreen` (0 windowed, 1 fullscreen), `fullscreen_exclusive` (0 borderless, 1 exclusive) | **Windowed**, Borderless fullscreen, Exclusive fullscreen. Borderless: a window covering the whole screen the game window is on, at the desktop's resolution; switching to other programs (Alt+Tab) is instant and the game stays on screen, and other windows and popups (volume, notifications, overlays) show over it. Under Windows it is a borderless window one pixel taller (or wider) than the screen, as an exactly screen-sized one is switched to exclusive mode by the graphics driver (ini-only `borderless_extra`, **1**: the pixels past the screen, 0 for exactly the screen). Exclusive: that screen switches to the Fullscreen resolution; on Alt+Tab the game minimises and the desktop gets its own resolution back (under Wayland the screen keeps its mode and the game is scaled to it, without minimising). F11 switches between windowed and the fullscreen kind chosen last. Leaving fullscreen puts the window back where it was, at its size |
| Fullscreen resolution | `fullscreen_w`, `fullscreen_h`, `fullscreen_hz` (all 0: Desktop) | exclusive fullscreen only (the row is hidden otherwise): **Desktop** (the desktop's resolution and refresh rate) or one of the screen's modes, e.g. 1920 x 1080, 144 Hz. A mode the screen lacks (another monitor) becomes the closest one it has |
| Window size | `window_w`, `window_h` | windowed only: presets 1280x960 to 3840x2160 (incl. 3440x1440), or any size by resizing the window |
| VSync | `vsync` | **on**, off |
| Frame rate limit | `fps_limit` | **Off (display rate)**, 30, 60, 90, 120, 144. Caps the images shown per second, in-between frames included; the game itself always runs at 30 |
| Show frame rate | `show_fps` | **on**: counter at the top right |
| **Picture** | | |
| Aspect ratio | `aspect` | 4:3 (original), Stretch to window, **Widescreen (hor+)**: 3D fills the window, 2D and menus stay 4:3 in the middle; with the fixed cameras, room parts the original hides for a camera angle are drawn in the side margins (except walls the camera looks through), while the 4:3 middle stays exactly the original (the OPTION screen's picture strips run to the window's edges; what the inventory, item box, files and map keep off-screen stays hidden) |
| Internal resolution | `scale` | **Auto** (follows the window) or 1x-8x of 640x448 |
| Display-rate frames | `interpolate` | "Enhanced mode": in-between frames blended from the last two game frames, up to the display's refresh rate. The game logic stays at its original 30 FPS, so gameplay is unchanged. **On**; off is "vanilla" |
| Frame blend (trail) | `frame_blend` | the PS2's motion trail (each frame mixed over the previous one). Auto, **Off**, On; Auto is on in vanilla and off with display-rate frames |
| **Widescreen** | | |
| Cutscene bars, Camera vignette, Screen effects | `cinema_bars`, `screen_filter`, `screen_effects` | Original (4:3), **Stretch to width**, Off: how these full-screen layers behave in widescreen. Screen effects also covers the tint of the sniper rifle's and linear launcher's scope (its reticle stays round in the middle) and the screen of Chris's oil pressure puzzle, which Off shows 4:3 instead of hiding |
| Movie zoom | `movie_zoom` | **off**; on: in widescreen the movies are enlarged, never stretched, until the black bars above and below them leave the screen or the picture reaches the window's sides. Nothing of the picture is cut off: on 16:9 thin bars stay (the movies are wider than 16:9), on ultrawide the movie fills the height. No effect at 4:3 |
| **Fog and brightness** | | |
| Fog intensity | `fog_intensity` | 0% to 100%, **100%** (as on the PS2): how thick the game's distance fog is, including the black fog many rooms use as darkness (lower values let the unlit cell at the start show more of the room outside). The fog everywhere becomes that of something the value's % as far away: 50% doubles the fog's distance (the fog density x 2.00 of fmil95's PS2 patch), 0% removes it. Applies at once |
| Brightness | `brightness` | -50 to +50, **0**: lifts or lowers the dark end of the picture |
| Contrast | `contrast` | 50% to 150%, **100%**: spreads or narrows the tones around mid-grey |
| Gamma | `gamma` | 0.5 to 2.0, **1.0** (in the ini in tenths, 5 to 20): higher brightens the mid-tones, black and white stay |
| Reset picture calibration | | brightness, contrast and gamma back to neutral |
| **Renderer** | | |
| Native 3D renderer | `native_3d` | **on**: 3D models projected and clipped by the GPU instead of the emulated VU1/GS path |
| Native textures | `native_tex` | **on**: textures uploaded once instead of on every switch |
| Cull off-screen models | `model_cull` | **on** (as on the PS2); off stops characters, enemies and objects popping at the screen edges. It does not affect the room itself (walls, floors, props): see Aspect ratio |
| **HD textures** | | see [HD texture packs](#hd-texture-packs) |
| HD textures | `hd_textures` | **on**: a PCSX2 HD texture pack in `mods/textures` replaces the textures it covers (after a restart). Without a pack nothing changes |
| HD textures: font | `hd_textures_font` | **off**: the game's font stays; on: the pack's font too |
| HD textures: UI | `hd_textures_ui` | **on**: also replacements that show something else than the original, such as redesigned menus, title and inventory (the port's own menu entries keep the original letters); off: only replacements of the original picture |

Brightness, contrast and gamma change the game's picture (menus and movies included), not the F1
overlay; at the neutral values the picture is exactly the original. While one of them is chosen,
both menus show a test pattern: a black strip and a white strip with five grey squares each (in F1
in the setting's tooltip). Turn Brightness until the left square on the black strip is barely
visible, and Contrast until the right square on the white strip is barely visible.

### Gameplay

| Setting | ini key | Effect |
|---|---|---|
| Skip intro | `skip_intro` | on: boot straight into the title menu (next start). Off: the warning screen, the logos and the intro movie play, and any key, mouse button or gamepad button skips the one on screen (a press during the Capcom logo's fade-out skips the movie, as Start does on the PS2); holding a button does not skip the next one. On the title's PRESS START any key, click or gamepad button opens the menu |
| Skip door animations | `skip_doors` | the door, stairs, ladder and lift animations between rooms. Off (0): always played, as the original; **Instant** (1): the game's load screen instead (no animation, so no door sounds); Skippable (2): the animation plays, and any key, mouse button or gamepad button pressed while it runs ends it and the next room follows at once. The press that opened the door does not count; a press made while the next room is still loading ends the animation as soon as the room is ready. |
| Fast loading | `fast_loading` | **on**: a room change, a new game or a loaded save no longer waits for what the PS2's disc and sound chip needed: the next room's sounds and textures are set up at once, so the load screen ends almost as soon as the screen is black (without the "Now Loading..." text), and with Skippable doors a press ends the animation that much sooner. The room's rain, smoke and sparks still get their usual head start before it appears. Off: the original loading times. |
| Battle Game unlocked | `battle_unlocked` | **on**: the title menu offers BATTLE GAME without a finished playthrough, and its character select offers all five characters at once (Claire, Claire in her other costume, Chris, Steve, Wesker). Saves and system data are not changed: characters and records you earn are still saved as in the original, and the Linear Launcher still needs an A rank with all five. Off: as the original, BATTLE GAME only after a cleared game and the characters you have earned |
| Difficulty select | `difficulty_select` | **on**: NEW GAME opens the difficulty menu of the European and Japanese versions (MODE SELECT: NORMAL, EASY, VERY EASY; its pictures are on the US disc too). NORMAL is preselected, so confirming twice starts the original game; Cancel goes back to the title menu. **Easy**: the item box starts with handgun bullets, bow gun arrows and powder, first aid sprays and ink ribbons, and ammo pickups hold more (handgun bullets 30 instead of 15, for example); your health and most enemies as on Normal. **Very Easy**: twice the health, healing items heal twice as much, enemies with fewer hit points (zombies about half), ammo pickups as Easy, an ink ribbon in your bag that is never used up, and an item box with the bow gun, grenade launcher, shotgun, magnum and assault rifle, their ammo and first aid sprays. Only Normal has the ranking at the end and its A-rank reward. A save keeps its difficulty. Off: as the US original, NEW GAME always starts on Normal |
| Quick knife | `quick_knife` | on: hold the quick-knife key to draw the combat knife without opening the inventory; release to get the previous weapon back |
| **Saves** | | |
| Save system | `save_system` | **Native files** (0): every save a file in `save/saves`, no limit, no memory card screens; Memory card (original) (1): see [Saves](#saves) |
| Import memory card saves | (action) | copies the memory card's saves into the save files |
| No ink ribbons needed | `no_ink_ribbon` | **off** (original); on: typewriters save without an ink ribbon and use none up |
| **Enemies** | | |
| Extra enemies | `enemy_extra` | **Off**, up to 1-3 more per room, of the types the room already has |
| Rearranged enemies, Enemy randomizer, Enemy seed | `enemy_place`, `enemy_random`, `enemy_seed` | optional enemy changes; bosses stay as they are ([ENEMIES.md](ENEMIES.md#enemy-options)) |
| Randomizer variety | `enemy_variety` | **Wild** (1): any enemy kind that fits the spot, a different one for each enemy of a room where possible, Tyrants, Giant Black Widows and bats included; Mild (0): two common kinds per room |
| Randomize event enemies | `enemy_random_events` | **on**: enemies a room shows later become other kinds too, and the ones a scene animates get another look of their own kind (the scene still plays); off: only free-roaming enemies change |
| Auto-climb stairs | `auto_stairs` | **on**: walking or running into a staircase climbs it, up or down, as the action button would, with every control scheme and in third person. It starts only while you walk towards the stairs (facing the way they go, within 45 degrees), not while you stand in front of them or walk past along their edge. Where a press would answer a door, item or examine spot first, that spot still needs the button; so do the steps onto or off a ledge, so you never drop off one by accident. Some stairs with an event (such as one in the Palace) need the button the first time; off: the button as in the original |
| Head blow-offs | `head_blowoff` | **on**: a zombie whose head the game bursts (the magnum, or the shotgun aimed up close) falls without it, on every zombie look; off: as the original, which shows the burst and the blood but leaves the head on ([ENEMIES.md](ENEMIES.md#head-blow-offs)) |

### Controls

Input source and Gamepad (see [Input source](#input-source-per-window)), **Vibration strength**
(`rumble_strength`, 0-100 %, **100**; see [Vibration](#vibration)), **Mouse in menus**
(`menu_mouse`, **on**: point and click in the inventory, item box, map, files, OPTION and these
menus, the typewriter's save screen and LOAD GAME (with either save system: pointing at a save
selects it, a click picks it, the wheel or a click on the arrows scrolls, YES / NO and the memory
cards too) and the game's questions such as "Use an ink ribbon?"; right click goes back; drag to
rotate and wheel to zoom when examining an item), **Fixed
camera controls** (`fixed_cam_controls`: Classic (tank) 0, **Modern (camera-relative)** 1; see
[Modern controls](#modern-controls-fixed-cameras)), then the bindings ([Rebinding](#rebinding)).

### Third person

Over-the-shoulder camera with mouse/keyboard or twin-stick gamepad controls and free aim,
toggled with **F5** (`tps`). The page holds field of view, camera distance and height, shoulder
side, mouse sensitivity, invert Y, crosshair, the game's auto-aim and the gamepad settings
([Third person with a gamepad](#third-person-with-a-gamepad)). Settings worth knowing:

- **Mouse/keyboard camera** (`tps_free_camera`): **B** (default) keeps the camera behind the
  character and the mouse turns it; **A** lets the mouse orbit the camera freely and W/A/S/D walk
  relative to the camera. This is unrelated to the game's KEY CONFIG types.
- **Light and fog** (`tps_atmos`): Per cut (original) or Smooth (fades over a second when the
  room's camera cut changes).
- **Gamepad controls** (`tps_pad_controls`): **Modern (twin-stick)** (default) or Classic (tank
  controls through the game's buttons, the camera behind). In Classic the weapon is raised with the
  game's aim button (R1); the right stick, the left stick and the d-pad then move the crosshair
  (with "Game auto-aim" on, the game's lock-on instead). Letting go of aim puts the camera back
  behind the character.
- **Precise hits** (`tps_precise_hits`, on by default): with free aim a shot hits an enemy only where
  the crosshair meets its body (head, torso, arms, legs as it moves, crawls or lies); a shot beside it
  misses. A head hit does half again the damage (zombies, Cerberus, Bandersnatch, Tyrant, Alexia's
  first form, Steve), and a zombie's head bursts (magnum, shotgun, sniper rifle) only from a hit on
  the head. The shotgun keeps a narrow spread; grenades, the bow gun, the
  rocket and linear launchers and the knife hit as in the game. Off: the game's wide hit test, which
  also hits an enemy the crosshair is beside. Not with "Game auto-aim" on the mouse (the lock-on
  needs the game's test) or with the fixed cameras.

Details and limits: [THIRDPERSON.md](THIRDPERSON.md).

### Audio

Music and voice (`music_volume`) and Sound effects (`sfx_volume`), 0-100%.

### Appearance

Your model (see [ACTORS.md](ACTORS.md)):

| Setting | ini key | Effect |
|---|---|---|
| You appear as | `appear_char` | **As played**, or another player file, story cast member, creature or zombie; at the end of the list the cutscene models of Claire, Chris, Steve and Wesker (Claire with her swinging ponytail, Wesker with his sunglasses) |
| Use gender-matched animations | `appear_gender_anim` | **on**: a man shown while you play Claire moves with Chris's motions, a woman shown while you play a man with Claire's |
| Swapped model in cutscenes | `appear_in_cutscenes` | **on**; off keeps the real Claire or Chris in cutscenes, with their own hands, faces and voice, and shows the swapped model only while you play |
| Talking faces in cutscenes (experimental) | `appear_cutscene_face` | **off**; on: the swapped model moves its mouth with the lines in cutscenes |

## Controls

Keyboard defaults (all rebindable):

| Key | PS2 button |
|---|---|
| Arrow keys | D-pad |
| Z / X / A / S | Cross / Circle / Square / Triangle |
| Q / W | L1 / R1 |
| 1 / 2 | L2 / R2 |
| Enter / Backspace | Start / Select |
| Space (hold) | Quick knife |
| R | Reload ([Reload](#reload)) |

Any gamepad SDL supports works as a DualShock 2 (south/east/west/north = Cross/Circle/Square/
Triangle, shoulders L1/R1, left trigger L2, right stick click R3). The right trigger is the
quick knife, since that button has no game action; the left stick click is L3.
Aim (R1 / RB) + Triangle (Y) reloads ([Reload](#reload)).
The game's own key types (OPTION -> KEY CONFIG) still decide what each PS2 button does.

### Vibration

A gamepad with rumble vibrates as the DualShock 2 did: when you fire, when enemies grab, bite or
hit you, at heavy footsteps and impacts, and in scripted moments such as explosions. Turn it on or
off with the game's own **OPTION -> VIBRATION** (saved with the game's options); with a rumble
gamepad connected at the first start it is on. Without one (keyboard and mouse only, or a pad
without motors) VIBRATION is greyed out and set to off, as on a PS2 pad without motors: connect the
gamepad and turn it on again there. **Vibration strength** (CONTROLS) scales it. The rumble stops
while the window has no focus (unless the input source is Gamepad only), and each window rumbles
only its own gamepad.

Third person (keyboard and mouse): W/S/A/D move and turn, Shift run, E action, Q cancel,
Tab inventory, M map, right mouse aim, left mouse fire, R reload ([Reload](#reload)). The sniper
rifle and the linear launcher aim through their scope: the mouse turns it (at the aiming
sensitivity, slower while zoomed in), Shift held zooms in, left mouse fires, letting go of right
mouse leaves it. With a gamepad see [Third person with a gamepad](#third-person-with-a-gamepad).

### Controls by scheme

The default bindings with the game's KEY CONFIG type A (the other types move the game's actions
between the PS2 buttons; the third-person keys and the Modern gamepad's buttons stay). Gamepad as
Xbox / PlayStation; keyboard in brackets.

| Action | Fixed cameras (Classic or Modern) | Third person, keyboard and mouse | Third person, gamepad Classic | Third person, gamepad Modern |
|---|---|---|---|---|
| Move | d-pad, left stick (arrows): tank, or where it points on screen with Modern | W/S/A/D (camera A: towards them on screen) | d-pad, left stick: tank | left stick, seen from the camera; d-pad: tank |
| Run | B / Circle held (X) | Shift | B / Circle | X / Square, or the stick pushed far |
| Quick turn | back + run; Modern: the Quick turn row (unbound) | S + Shift | back + run | d-pad down, or stick back + run |
| Aim | RB / R1 (W) | right mouse | RB / R1 | LT / L2 |
| Aim while aiming | d-pad, left stick (arrows): turn, up / down | the mouse | right stick, left stick, d-pad | right stick |
| Fire | A / Cross (Z) while aiming | left mouse | A / Cross | RT / R2 |
| Reload | RB + Y (R) | R | RB + Y | RB / R1 |
| Quick knife (hold) | RT / R2 (Space) | Space | RT / R2 | LB / L1 |
| Action, confirm | A / Cross (Z) | E | A / Cross | A / Cross |
| Cancel | B / Circle (X) | Q | B / Circle | B / Circle |
| Inventory | Menu / Start, right stick click (Enter) | Tab | Menu / Start | Y / Triangle |
| Map | Y / Triangle (S) | M | Y / Triangle | View / Back |
| OPTION | View / Back (Backspace) | Backspace | View / Back | Menu / Start |
| Scope zoom | LB / L1 held (Q) | Shift held | LB / L1 held | right stick click, or X held |
| Stairs | walk into them (Auto-climb stairs) or action | as fixed | as fixed | as fixed |

Menus, messages, the inventory's screens and events use the game's buttons with every scheme
(confirm A / Cross (Z), cancel B / Circle (X), L1 / R1 (Q / W) for the map's zoom and the pages of
ENHANCED SETTINGS); with the keyboard in third person the third-person keys (W/A/S/D, E, Q, Tab,
M) also work in the inventory, map, item box and messages, while the title menus and OPTION take
the game's keys only.

### Third person with a gamepad

THIRD PERSON page: **Gamepad controls** = **Modern (twin-stick)**, the default (ini
`tps_pad_controls = 1`; 0 = Classic, the tank controls of the fixed cameras with the camera
behind). As in the Resident Evil 2 and 4 remakes:

| Gamepad (Xbox / PlayStation) | Action | ini |
|---|---|---|
| Left stick | walk where it points seen from the camera; pushed far it runs | |
| Right stick | turn the camera; while aiming, aim (the crosshair; the sniper rifle's and linear launcher's scope too, slower while zoomed in) | |
| LT / L2 (hold) | aim: the character faces the camera's direction, the camera moves to the shoulder | `pad_tps_aim` |
| RT / R2 | fire while aiming; otherwise action | `pad_tps_fire` |
| A / Cross | action: doors, items, examine, confirm | `pad_tps_action` |
| B / Circle | cancel | `pad_tps_cancel` |
| X / Square (hold) | run | `pad_tps_run` |
| Y / Triangle | inventory | `pad_tps_inventory` |
| View / Share (Back) | map | `pad_tps_map` |
| LB / L1 (hold) | quick knife: knife drawn and raised, RT slashes, the weapon before on release | `pad_tps_knife` |
| RB / R1 | reload ([Reload](#reload)) | `pad_tps_reload` |
| D-pad down | quick turn (the camera swings round with the character) | `pad_tps_quickturn` |
| Right stick click | camera behind the character; in the scope: zoom in / out | `pad_tps_recenter` |
| Left stick pulled back + run | quick turn | |
| Menu / Options (Start) | options (the OPTION screen; View / Share is the map here) | `pad_tps_options` |
| D-pad up / left / right | as before: tank turn and walk; while aiming they move the crosshair | |

The sniper rifle and the linear launcher aim through their scope: hold LT, aim with the right stick
(the left stick and d-pad turn it as in the original), click the right stick to zoom in or out, RT
fires, RB reloads, let go of LT to leave it.

Reload sits on RB as in Resident Evil 4 and the RE4 remake's type C (the remakes' other types use
X / Square, which runs here as in the game's type A); the quick turn, which the stick gesture also
does, moved from RB to the d-pad's down. A `recvx.ini` from before the reload button that still
has the quick turn on RB gets the new default.

- **Moving**: the character turns quickly towards the stick's direction and walks; pointing more
  than 100 degrees away turns it on the spot first. After a quick turn or "camera behind" the
  held stick keeps its meaning until it turns or is let go, so you keep running the way you
  were going.
- **Aiming** is free aim, exactly as with the mouse (the same crosshair, shot and pose). The left
  stick and the d-pad move the crosshair too while aiming. With
  **Game auto-aim** on, the gamepad gets a gentle aim assist instead of the lock-on: near an
  enemy the aim slows down and drifts slightly towards it while you move the stick.
- **Menus, messages, events**: the gamepad works as the game's KEY CONFIG type sets it (as in
  Classic); the third-person buttons only play while the third-person camera does. A button held
  through the change (the one that opened the inventory, say) does nothing until let go. The
  buttons that open the inventory (Y), the map (View) and OPTION (Start) also close them, as the
  game's own buttons do.
- **Mouse and keyboard together** (input source Auto): the camera follows the device used last;
  the gamepad's right stick turns it freely, the mouse as set in "Mouse/keyboard camera".
- Settings (THIRD PERSON page, GAMEPAD): stick camera speed X / Y (`tps_pad_sens_x`,
  `tps_pad_sens_y`, 100%: 180 / 120 degrees a second fully pushed), stick aiming speed
  (`tps_pad_aim_sens`, 50% of that), invert Y (`tps_pad_invert_y`), left and right stick dead
  zones (`tps_pad_dead_move` 20%, `tps_pad_dead_look` 15%). The right stick's response is finer
  near the centre.
- The third-person gamepad buttons are their own bindings: rebinding one (CONTROLS, Gamepad,
  "Third person (gamepad, Modern)") leaves the fixed-camera binding of the same button alone.

### Reload

Reloads the equipped weapon from the ammo in the inventory without opening it, as the game does
by itself when you fire with an empty magazine: the same motion, sound and magazine size, and it
takes the first ammo in the inventory that reloads that weapon, as combining them in the
inventory would (a magazine that fills up leaves the rest in the stack; a stack used up leaves
the inventory). It works whenever the magazine is not full and matching ammo is there: while
aiming (also through the sniper rifle's scope) and with the weapon lowered while standing,
walking or running (the weapon comes up for the reload and goes down again; holding aim keeps
it up). Without matching ammo, with a full magazine, unlimited ammo, during events, menus,
doors or when grabbed it does nothing. Weapons the game does not reload by itself, the grenade
launcher (its rounds are combined in the inventory) and the knife, are left alone; the bow gun
reloads the kind of arrows it holds. The sniper rifle and linear launcher reload only through
their scope (their reload ends in it).

Defaults:

| Mode | Keyboard | Gamepad |
|---|---|---|
| Fixed cameras | R | Aim + Triangle: hold R1 / RB, press Triangle / Y |
| Third person, Classic gamepad | R | Aim + Triangle (as the fixed cameras) |
| Third person, Modern gamepad | R | RB / R1 |

The game rows' "Reload" (`key_reload` / `pad_reload`) works in every mode, the third-person
"Reload" (`key_tps_reload` / `pad_tps_reload`) in third person. R is no game key in any key type.
The gamepad has no free button, so its reload shares Triangle: while the weapon is raised
Triangle does nothing in any key type (it is the map, or the options in type D, which open only
with the weapon lowered).

**Aim + button**: a reload binding may share a button with another action of its set (the game
rows, or the third-person rows). Then it is a combination: pressed while aim is held it reloads,
and the other action does not see that press at all, also when aim is let go before the button;
pressed without aim the button does its own action, as before. Aim and the button pressed
together count as Aim + button; the reload follows as soon as the weapon is up. In menus, events
and the inventory the button always does its own action. CONTROLS shows such a binding as
"Aim + Triangle" (or the key's name); a reload binding no other action uses works on its own, also
with the weapon lowered. Bound to the aim button itself it reloads on every aim.

A `recvx.ini` from before this (no `bindings_version` line) whose reload rows are still `none`,
their default then, gets these defaults.

### Modern controls (fixed cameras)

F1 -> Controls or ENHANCED SETTINGS -> CONTROLS: **Fixed camera controls** = Modern
(camera-relative), ini `fixed_cam_controls = 1`, the default on a first start (an existing
`recvx.ini` keeps its choice); Classic (tank controls) is `fixed_cam_controls = 0`. Like the
alternate controls of the Resident Evil HD remasters:

- The **left stick** or the **d-pad** (arrow keys on the keyboard) points where the character
  walks on screen; it turns there quickly (180 degrees in about 0.3 s) and walks. Pushed far
  (70%) the stick runs; the run button runs as before (on the keyboard and d-pad, which have no
  half push). Pointing more than 100 degrees away from where the character faces turns it on the
  spot first.
- **Camera cuts**: while the direction is held through a cut, it keeps meaning what it meant in
  the previous camera, so the character keeps going the same way. The new camera takes over when
  the direction turns by more than 35 degrees or is let go. A cut during stairs, a ladder,
  climbing over or pushing does not keep the old camera: back in control, the stick follows the
  camera on screen.
- **Aiming** works as in Classic: holding the aim button stops, the stick or d-pad turns and aims
  up / down, the game's auto-aim and the quick knife are unchanged. Letting go of aim returns to
  modern movement.
- **Quick turn**: bind the "Quick turn" row (unbound by default; it holds back, then run). While
  it is held the stick does not walk.
- Stairs, steps and ladders still start with the action button; while climbing or pushing a box
  the direction only counts as "forward" when it points roughly where the character faces.
- Third person keeps its own controls; this option only applies to the fixed cameras.

Hotkeys:

| Key | Action |
|---|---|
| F1 | settings overlay |
| F2 | cheat menu |
| F5 | third-person camera on/off |
| F11 | windowed / fullscreen (borderless or exclusive, as chosen in Display mode) |
| Page Up, F / Page Down | noclip fly up / down (when flying is on; R reloads now) |

### Rebinding

F1 -> Controls (tabs Keyboard / mouse and Gamepad: click a binding, then press the key; right
click clears) or OPTION -> ENHANCED SETTINGS -> CONTROLS (DEVICE picks the set). Each row has two
slots. While waiting for a key, Esc cancels and Delete clears. A key already used by another row
is swapped to that row; Reload instead shares it (Aim + button, [Reload](#reload)). Gamepad buttons are named after the connected pad (Xbox, PlayStation,
Nintendo or generic).

Bindings are saved in `recvx.ini` as `key_<row>` and `pad_<row>` (second slot `<row>_2`):

```ini
key_cross = Z
key_tps_run = Left Shift
key_tps_run_2 = Right Shift
key_tps_aim = Mouse Right
pad_cross = south
pad_l2 = +lefttrigger
pad_quickturn = -lefty
```

Keys use SDL key names (`Z`, `Return`, `Left Shift`, `PageUp`), mouse buttons
`Mouse Left/Middle/Right/X1/X2`. Gamepad inputs are positional (`south` = Xbox A = PlayStation
Cross, `east`, `west`, `north`, `back`, `start`, `guide`, `leftshoulder`, `rightshoulder`,
`leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `misc1`, `paddle1`-`paddle4`,
`touchpad`) or an axis half (`+lefttrigger`, `-leftx`, `+lefty`, ...). `none` unbinds.
Extra rows: "Quick turn" (unbound by default: back, then run), "Reload" (R, Aim + Triangle on
the gamepad, [Reload](#reload)), the third-person keys, the
third-person gamepad set (`pad_tps_*`, [Third person with a gamepad](#third-person-with-a-gamepad))
and the hotkeys.

### Input source (per window)

F1 -> Controls or ENHANCED SETTINGS -> CONTROLS:

| Setting | ini key | Values |
|---|---|---|
| Input source | `input_source` | **Auto** (keyboard/mouse and gamepad, while the window has focus), **Keyboard/mouse only** (gamepads ignored), **Gamepad only** (keys and mouse no longer play; the gamepad keeps working when another window has focus) |
| Gamepad | `input_pad` | **Any** (the first connected pad) or **1-4**: the pad the system numbers so (Xbox: its slot light; PlayStation and Switch pads: their player lights) |

With Gamepad only, the hotkeys and the overlay windows still respond to keyboard and mouse, so
F1 always gets you back. If the game runs through Steam with Steam Input
enabled, Steam may send its virtual pad only to the focused window: turn Steam Input off for it.

## Cheats and debug tools (F2)

The F2 window (also the EXTRAS page of ENHANCED SETTINGS) is meant for testing and fun:

- infinite health, heal now, infinite ammo;
- noclip: Through walls, or Walls + fly (fly with the noclip keys, or R2/L2);
- unlock all: locks act as if you carried their key, without it entering your inventory. Switching it on or off takes effect at once, also in the room you are in.
  Examining a locked door, a card reader or the spot where an emblem, plate or jewel goes uses
  the key there, as the item screen's Use would (or the game asks "Will you use ...?" as it does
  when you carry the key); a spot already done is examined as usual. Turned on inside a room,
  some doors notice it only once you enter the room again. If the game then asks whether to
  discard the key, nothing is removed. Doors the story keeps shut (locked from the other side,
  opened by a cutscene) stay shut, and the prison's metal detector room is left out: every other
  item check (the detector, the typewriters' ink ribbon, weapons and ammo, the item box) uses
  what you really carry;
- add any item to the inventory, and view the inventory;
- spawn an enemy of any type in front of you ([ENEMIES.md](ENEMIES.md)); custom enemies follow the
  game's types at the end of the list, marked "[C] ": Mutated Steve (Steve's monster form that can
  be killed: his own moves, the Tyrant's death) and those of `mods/enemies.ini`;
- teleport to any room through the game's door transition: choose a **location** (an area of the
  game; Claire's and Chris's halves are listed separately, Chris's are the last three), a
  **room** and an **entry** (where you arrive). Rooms carry the save menu's name where the game
  has one (read from your disc, e.g. "Prison: front"), every other room a name of ours after its
  area and what is there ("Prison: Graveyard", "Military Training Facility: Pool"; rooms that only
  play a cutscene start with "Cutscene:"). Each entry says which room's door leads there
  ("Entry 1: from Prison: Metal detector room"; "+1" when there are more, F2 lists them on hover).
  The EXTRAS page leaves the area out of these names ("Metal detector room") where the location
  already says it, so they fit.
  Rooms you added in the mods folder show as "Custom room RM_xxxx", or by the name tag in their
  file header. The list is read from the disc the first time you open F2 or
  the EXTRAS page (a few seconds; until then the plain numbers show). In F2, typing in the open
  Room list searches every location, and **Raw ids** brings back the stage/room/position numbers.
  A room may not work in every story state (wrong case, missing scripts); an unlisted number may
  crash.

Actions chosen in ENHANCED SETTINGS run when the menu closes.

## HD texture packs

The game can use an HD texture pack made for the PCSX2 emulator (SLUS-20184, the US release), as
the pack comes:

1. Put it into the folder `textures` inside the `mods` folder (next to `save/`): the game's folder
   from PCSX2's `textures` directory (`mods/textures/SLUS-20184/replacements/...`), only its
   `replacements` folder, or the files themselves all work. A folder elsewhere:
   `hd_textures_dir = <path>` in `recvx.ini`, or `--hd-textures <path>`.
   With several packs, name the folders `1_...`, `2_...`: on the same texture the first one in
   name order wins.
2. Start the game. The log says `hd textures: N replacements in <folder>`.

Rooms show the replacements from their first visible frame; the first time a room is entered its
replacements are also checked against the original pictures, which can make that room take a
moment longer to appear. Textures that first show up later (another camera angle, an enemy, an
effect) are loaded in the background without stopping the game, and switch to HD a moment after
they appear. Settings (Display): **HD textures** (on), **HD textures: font** (off)
and **HD textures: UI** (on).

- Packs made on another disc version (for example a translation) or with redesigned menus
  change text and layouts there. With **HD textures: UI** on (the default) those replacements are
  used too; off, only replacements that show the same picture as the original are (the log names
  each one skipped). Broken replacements (transparent where the original shows something) are
  never used. The port's own menu entries (ENHANCED SETTINGS, RETURN TO TITLE) always
  keep the original letters.
- The game's font is kept unless **HD textures: font** is on.
- Packs usually come without mipmaps, as PCSX2 uses them: very large textures shimmer a little in
  the distance.
- Checked files are remembered in `save/hd_textures.txt`; delete it after changing the pack.
- Memory: up to 1 GB of loaded replacements stays on the graphics card, the least recently used
  are dropped. Details: [HD_PACK.md](HD_PACK.md).

## Mods

Files in the `mods` folder (next to where you start the game, or `RECVX_MODS=DIR`) replace disc
files without touching the image: `mods/NAME` for files opened by name (for example room files
`RM_xxxx.RDX`) and `mods/<archive>.AFS/<index>` for entries of AFS archives. The log names
every replaced file. `mods/enemies.ini` adds custom enemies (a type's moves with another look,
tougher, faster or bigger; for the spawner and the Wild randomizer), see [ENEMIES.md](ENEMIES.md)
"Enemy variants". Modding reference and tools: [modding/README.md](modding/README.md).

## Troubleshooting

| Problem | What to try |
|---|---|
| Linux: "GLIBC_2.xx not found" | the build needs glibc 2.34 or newer; build with the default `RECVX_GLIBC_COMPAT=ON` ([BUILDING.md](BUILDING.md#running-on-older-distributions-glibc)) |
| Linux: no window, no sound, or "cannot open shared object" | the 32-bit OpenGL, X11/Wayland and audio libraries are missing ([BUILDING.md](BUILDING.md#linux)) |
| No file selector on the first start | Linux needs the desktop portal or `zenity`; otherwise pass the image on the command line or via `RECVX_ISO` |
| "cannot open game disc image" | check the path; only SLUS-20184 images work |
| Low frame rate at high resolution | lower Internal resolution, set a Frame rate limit, or turn off Display-rate frames; F1 -> Performance shows where time goes |
| Gamepad not working in the second window | set Input source to Gamepad only and pick the pad number; check Steam Input |
| A crash | run with `--log` and attach `recvx.log` (it contains a backtrace) to an issue |
