# Changelog

## Unreleased

### Removed



### Changes

- **Move while aiming: a fuller walk** (after aikatore's recvx-enhanced-port): the hips walk with the
  legs now (their sway and bob carried up to a steady aim, instead of the legs alone under still
  hips), the walks start where the game's walks start, and the pace is 60% of a walk (was 65%). The
  hips still face the aim when walking sideways; upstream's turn of the hips is left out.
- **Ambient occlusion fades in fog**: the shading now thins out with the fog at each point instead
  of showing at full strength through it (dark outlines of distant objects in foggy rooms), with
  either fog style.
- **Enhanced fog** (GRAPHICS, Fog: Original / Enhanced; off by default): the fog is worked out per
  pixel from the game's own fog table (no stretched or banded fog on large surfaces), clears more
  gently near you, keeps less of its tint up close and takes on the light where it hangs; at the
  far distance it stays the original, so room edges stay hidden.
  With it, **Fog light glow** (the room's lights lift and tint the fog around them), on by default
  and switchable.
- **Fixed: ambient occlusion under water** (submarine dock, fixed camera): the wavy under-water views
  no longer get AO, which showed a dark outline of Claire beside her (flickering on High), as the
  distortion moves the picture but not the depth AO is worked out from.
- **Fixed: flickering elevator door in third person** (Antarctic Base pipe corridor): an unused dark
  copy of the door, which no camera of the room shows, is no longer drawn.
- **Always have the knife** (GAMEPLAY, experimental, off by default; shown while Quick knife is on):
  the quick knife works without the combat knife in the bag, so the knife can go into the item box
  and its space is free. The bag's knife stays a normal item to equip from the inventory.
- **Ambient occlusion Low / High** (GRAPHICS): Low works it out once per game frame and keeps it for
  the in-between frames, about a quarter of the cost with Display-rate frames (the shading at fast-
  moving edges trails slightly); High works it out for every frame shown, as before. A saved "on"
  stays High.
- **Update notice**: once a day at start the game checks GitHub for a newer release and, when
  there is one, says so on the title screen and in F1, with a button to open the download page
  (Check for updates, GAMEPLAY, on by default; one anonymous request, nothing downloaded by itself).
- **Faster drawing with the graphics options on**: the game's main shader is compiled in variants
  holding only what a draw uses (alpha test, texture, texture clamp, fog, per-pixel lighting), so
  most draws skip the lighting work and opaque ones let the GPU skip hidden pixels; the picture is
  the same. About 5% less GPU time per image at 4K in rain-lit rooms. After the renderer pass of
  aikatore's recvx-enhanced-port. The first start after installing (or a driver update) compiles them
  once (about 1.5 s); later starts take them from the driver's cache.
- **New defaults**: Display mode Borderless fullscreen (was Windowed), Show frame rate off and FSR on;
  `--size` now starts windowed at that size. Saved settings are kept.
- **EXIT GAME** in the title menu, below the game modes, in the menu's own lettering: it asks
  YES / NO (NO selected) and then closes the game as closing the window does.
- **GRAPHICS settings page** (F1 and ENHANCED SETTINGS), next to DISPLAY: anisotropic filtering,
  dithering, post-processing and lighting moved there from DISPLAY, which keeps the window,
  resolution, widescreen, picture calibration, HDR and renderer settings.
- **FSR upscaling** (DISPLAY, on by default): AMD FSR 1 upscales a picture drawn below the
  window's resolution with sharp edges instead of a plain stretch; Auto Internal resolution then
  stays at or below the window where FSR has at most 1.5x to make up (4x instead of 5x at 2160p)
  for a higher frame rate, and keeps the nearest scale otherwise (2x at 720p and 800p).
- **Faster rendering at high resolutions**: in-between frames upload about a third of the vertex
  data (shared strip vertices sent once, a smaller vertex), ambient occlusion works from a
  half-size depth copy, and keeping text out of bloom copies only what is drawn over after the
  first text instead of the whole picture. At 4K with every effect on, in-between frames take
  about 30% less render-thread time and 15-30% less GPU time, with the same picture.
- **Post-processing** (GRAPHICS, off by default): Bloom lets fire, lamps, flashes and other bright
  parts glow onto their surroundings with a slightly warmer picture (on or off; the glow only in
  play, not on the title screen, menus, movies or text), Colour grading (a filmic tone curve
  easing highlights and deepening the near-blacks, teal shadows and warm highlights) and Ambient
  occlusion (contact shadows in creases and under characters and objects). Works with SDR
  and HDR output and with in-between frames; at the neutral values the picture is unchanged.
- **Lighting** (GRAPHICS): a moodier default look. Light falloff Smooth (default) lets lamps, fires
  and other point lights fade out over the outer quarter of their range instead of cutting off, so
  characters and objects no longer darken at once when leaving a light. Lighting picks how bright
  the rooms are while lamps, fires and the flashlight keep their strength: Original (the PS2's),
  Balanced (default; main and ambient light 75%), Horror (50% and 25%) and Nightmare (5%, the fog
  thinned to 10%). Soft shading lets light reach further round the shadow side for softer shading
  of low-polygon shapes. Light falloff Original with Lighting Original gives the PS2's lighting.
- **Anisotropic filtering** (GRAPHICS, 16x by default): floors and walls seen at a shallow angle
  stay sharp into the distance without shimmering; textures with cut-outs are left as they are.
- **Dithering** (GRAPHICS, on by default): smooths colour banding in dark gradients.
- **Per-pixel lighting** (GRAPHICS -> LIGHTING, off by default, native 3D renderer): the game's own
  lights computed for every pixel instead of at the corners of each polygon, so light pools and the
  falloff of lamps and fires show across large floors and walls, and shading on characters is
  smooth.
- **Surface materials**: a picker in the F2 window gives a texture a material (metal, tile, glass,
  wood, ...) by clicking a surface in the game picture; its surfaces then catch highlights from the
  lights and the flashlight (Material shine, GRAPHICS -> LIGHTING). Saved to `mods/materials.ini`,
  which can be shared (docs/modding/MATERIALS.md).
- **Wet surfaces** (GRAPHICS -> LIGHTING, off by default): where it rains, floors are wet: lamps,
  fires, moonlight and the flashlight glint on them, breaking up across the grime.
- **Flashlight** (L, the gamepad's left stick click; rebindable): a flashlight Claire and Chris
  switch on and off in play. It lights the floor ahead where they face, along the aim while
  aiming, and glows faintly round them; Flashlight in GRAPHICS -> LIGHTING (on by default) makes it available.
  It runs on a battery (4 minutes, recharging in 3 while off): low,
  the beam dims and flickers, empty it goes out; a battery icon above the ammo counter shows the
  charge while it is in use.
  Needs the native 3D renderer. The left stick click no longer passes the PS2's L3 (which the game
  does not use) by default.
- Release downloads are named `cvx-port-VER-linux-i686.tar.gz` / `cvx-port-VER-windows-x86.zip`
  and come with the `mods/textures/` folder for HD texture packs (with a short README).
- **HDR output** (DISPLAY, off by default): on an HDR display the game's highlights are expanded
  into HDR by the port itself (inverse tone mapping), without RTX HDR, Auto HDR or an injector; the
  F1 overlay stays at paper white. Paper white, Peak brightness (Auto: from the system and the
  display) and Highlights are adjustable. Presented through Direct3D 11 on Windows and Vulkan
  (HDR10) on Linux Wayland desktops with HDR; the renderer stays OpenGL. Takes effect at the next
  start.
- **Ammo counter** (GAMEPLAY, on by default): the equipped weapon's magazine and its size, such
  as `12/15`, in the bottom right corner in the game's font. Red when empty.
- **Weapon shortcuts** (GAMEPLAY, off by default): 1 / 2 or the d-pad's left / right switch to the
  previous / next weapon in the bag, without the inventory. In play the d-pad's left / right no
  longer turn (the left stick does), up / down still walk; menus and aiming keep the whole d-pad.
  L2 / R2 moved from 1 / 2 to 9 / 0.
- **Display-rate frames** no longer bring the game to a near halt in heavy scenes on slower
  machines (the Antarctic Base pool room with the power on): an in-between image that would not
  be finished before the next game frame is due is left out, so the game keeps its speed and its
  own frames are shown. Matching a frame with the one before is also quicker where one object is
  drawn hundreds of times (the carousel's lights). When an in-between image is left out, the
  game frame itself is shown instead of nothing (on high refresh rate displays it was never shown).
- **Fewer draw calls in busy scenes**: the game sets a model's texture again before each of its
  parts; the port now only ends a batch of drawing when the texture or its palette actually
  changes. Facing the pool in the Antarctic Base pool room that is 233 instead of 1,294 draws a
  frame (about half the time on Linux; on Windows, whose OpenGL drivers cost more per draw, it
  froze the picture there with display-rate frames on). The picture is unchanged.
- **Move while aiming** (THIRD PERSON, Modern gamepad, on by default): the left stick walks slowly
  any way while aiming and firing, the legs walking (or stepping back) under the aim.
- **Fixed camera controls** start as Classic (tank), as the original; Modern (camera-relative) is
  still in CONTROLS. A `recvx.ini` that has a choice keeps it.
- **Fog in third person**: the third-person camera uses 75% of Fog intensity (100% there is 75%):
  close behind Claire the full fog looked too thick in many rooms. The fixed cameras keep the value.
- **Crash report**: every crash writes `recvx-crash.log` (the version, a backtrace and the last
  messages) next to `recvx.log`, also without `--log`; on Linux also for a stack overflow.
- **Smoother Motion under water**: the under-water views of the submarine dock are smooth too (they
  stayed at 30 FPS with the fixed camera).
- Smoother Motion takes less time a frame in rooms with many enemies.
- ENHANCED SETTINGS -> CONTROLS shows the gamepad's bindings first (the device row switches to the
  keyboard and mouse).
- "You appear as" is saved as `appear_char` in `recvx.ini` (it was `coop_char`): choose your model
  again once after updating.
- The gamepad's left stick click is L3 again by default (it was the trade request).

### Fixes

- **Windows: much smoother in busy rooms** (also with every graphics option off): the renderer
  asked the graphics driver for each texture's mip levels on every textured draw (for anisotropic
  filtering, even when it was off). NVIDIA's driver on Windows runs threaded and answers such a
  question only after everything queued before it, which held up the renderer hundreds of times
  per image: the Guillotine room ran at about 80 images a second on an RTX 5060 instead of 130.
  The levels are now kept with the textures; the picture is the same.
- Linux: under Wayland with NVIDIA's 32-bit driver the game failed to start ("unable to create an
  EGL window surface"); it now falls back to X11 (XWayland) by itself.

- **Sub machine gun reload crash**: reloading the sub machine gun, with the Reload button or by
  firing on an empty magazine, no longer crashes the game. It has no reload animation, so the ammo
  goes in at once and you keep aiming.
- **Grenade launcher aim with another Character Model**: in third person the grenade launcher no longer
  points up while aiming with another model; it points at the crosshair.
- **Side pack reload**: ammo in the side pack's last two slots now reloads the weapon when it runs
  empty.
- **Lighter and lockpick**: taking the lighter or Claire's lockpick out of the item box (or picking
  up a lighter) no longer replaces the other one, which was lost.
- **Albinoid crash**: an adult albinoid or Monster Steve no longer crashes the game while a young
  albinoid holds you.
- **Spawn crash**: an enemy spawned (F2) while changing weapons no longer crashes the game.
- **Start-up memory error**: game memory is reserved first, and when another program holds its
  address the error names it (usually an overlay or antivirus).

