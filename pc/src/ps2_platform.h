/* Internal interfaces of the PC platform layer (not seen by decomp code). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ps2sdk_shim.h"
#include "ps2_hw.h"
#include "host/fileio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* host (pc/src/host) */
enum { ASPECT_4_3, ASPECT_STRETCH, ASPECT_WIDE };
typedef struct {
    char iso_path[1024];
    char save_dir[1024];
    int  window_w, window_h;
    int  fullscreen;         /* F11 flips it; the kind: fullscreen_exclusive */
    int  fullscreen_exclusive;   /* 0 borderless (the desktop's size), 1 exclusive (a display mode, settings.c) */
    int  fullscreen_w, fullscreen_h, fullscreen_hz;   /* exclusive: the mode (Hz rounded); 0 = the desktop's */
    int  borderless_extra;   /* Windows borderless: pixels the window reaches past the monitor (settings.c), 0 = exactly it */
    int  vsync;
    int  fps_limit;          /* presents per second: 0 display rate, else index into render.c fps_limits (30/60/90/120/144) */
    int  log_level;
    /* user settings (F1 overlay, saved in <save_dir>/recvx.ini) */
    int  scale;              /* internal resolution multiplier, 0 = auto */
    int  fsr;                /* FSR 1 upscaling of the internal resolution to the window (gs_gl.c fsr_upscale), 0/1; auto scale then stays at or below the window */
    int  aspect;             /* ASPECT_* */
    int  show_fps;
    int  music_volume;       /* 0-100 */
    int  sfx_volume;         /* 0-100 */
    int  rumble;             /* gamepad vibration strength 0-100 % (input/pad.c scePadSetActDirect) */
    int  interpolate;        /* enhanced mode: display-rate in-between frames of the 30 FPS game */
    int  frame_blend;        /* PS2_swap's 25% previous-frame blend (motion trail): 1 on, 0 off, -1 auto (off in enhanced) */
    int  screen_filter;      /* camera filter (vignette): 0 original 4:3, 1 stretched to the view, 2 off */
    int  cinema_bars;        /* cutscene letterbox bars: same modes */
    int  fog_style;          /* 0 Original, 1 Enhanced: per pixel, reshaped (pc/glue/fog_glue.c, gs_gl.c fs_src) */
    int  fog_glow;           /* Enhanced fog: the point lights glow in it */
    int  fog_beam;           /* ... and the flashlight's beam shows in it */
    int  fog_intensity;      /* fog at this % of the original, 0..100 (pc_fog_scale); 100 original, 0 none */
    int  movie_zoom;         /* movies scaled evenly until the letterbox bars leave the screen (movie_glue.c vbrank_draw) */
    int  screen_effects;     /* full-screen effect overlays (noise/scanline layers): same modes */
    int  brightness;         /* picture calibration in the present pass (gs_gl.c draw_display): -50..50, 0 neutral */
    int  contrast;           /* 50..150 %, 100 neutral */
    int  gamma;              /* tenths, 5..20, 10 neutral */
    int  bloom;              /* post-processing in the present pass (gs_gl.c draw_display): bloom and its warmth (BLOOM_*), 0/1 */
    int  grading;            /* colour grading (gs_gl.c present_fs): the tone curve and split toning (GRADE_*), 0/1 */
    int  ao;                 /* ambient occlusion (gs_gl.c ao_pass, AO_*): 0 off, 1 High (every image), 2 Low (once per game frame) */
    int  light_falloff;      /* vertex lighting (pc/glue/light_glue.c): point lights 0 end at their range at a step, 1 fade out over its outer quarter */
    int  lighting;           /* lighting mode (host.c pc_light_main, pc_light_ambient, pc_fog_scale): 0 original, 1 balanced, 2 horror, 3 nightmare */
    int  light_pixel;        /* per-pixel lighting of the chunk models (light_glue.c, gs_gl.c fs_src), with the native 3D renderer */
    int  wet;                /* wet surfaces in rain (light_glue.c, gs_gl.c fs_src): 0/1, on at WET_ON % */
    int  dither;             /* dithering of the GS's 8 bits and the output (gs_gl.c fs_src, present_fs): 0/1 */
    int  smaa;               /* anti-aliasing: 0 off, 1 SMAA (gs_gl.c smaa_run) */
    int  aniso;              /* anisotropic filtering (gs_gl.c aniso_filter): 0 off, else 2^aniso samples */
    int  mat_shine;          /* surface materials' highlights (gs/materials.c, mods/materials.ini): 0/1 */
    int  flashlight;              /* the flashlight (light_glue.c; host.c FLASH_*): 0/1, off: the button does nothing */
    int  flash_shadow;            /* ... and what it lights casts shadows (gs_gl.c flash_shadow) */
    int  light_soft;         /* 0/1: light wraps round the shadow side (N.L + w) / (1 + w), w = SOFT_ON % / 100; 0 original */
    int  hdr;                /* Windows: HDR output through Direct3D 11 (render/hdr_win.c), at the next start */
    int  hdr_paper_white;    /* nits of the game's white (and the overlay), 0 = Windows' SDR content brightness */
    int  hdr_peak;           /* nits of the brightest highlight, 0 = the display's peak */
    int  hdr_highlights;     /* % of the room between paper white and peak the expanded highlights use */
    int  skip_doors;         /* door/stairs/ladder animations: 0 played, 1 instant (load screen), 2 played, a press skips */
    int  update_check;       /* Check for updates (host/update.c): the latest GitHub release, once a day at start */
    int  update_last;        /* ini-only: when it last asked (Unix time) */
    int  update_latest;      /* ini-only: the latest release's version then, 1.2.3 as 1002003 */
    int  fast_loading;       /* room loads: sound packs and texture lists at once instead of spread over frames (pc_fast_loading) */
    int  quick_knife;        /* holding the quick-knife key draws the knife (pc/glue/knife_glue.c) */
    int  knife_always;       /* the quick knife without the bag's knife: always at hand (knife_glue.c HIDDEN) */
    int  weapon_shortcuts;   /* 1 / 2 and the d-pad's left / right cycle the bag's weapons in play (pc/glue/weapon_glue.c) */
    int  auto_stairs;        /* walking into stairs climbs them without the action button (pc/glue/stairs_glue.c) */
    int  ammo_hud;           /* the equipped weapon's magazine in the bottom right corner (pc/glue/hud_glue.c) */
    int  head_blowoff;       /* a zombie whose head the game destroyed is drawn without it (pc/glue/variant_glue.c pc_zombie_head) */
    int  model_cull;         /* skip actors/objects whose test points are off-screen (as the PS2) */
    int  skip_intro;         /* boot straight into the title menu */
    int  battle_unlocked;    /* the title menu offers BATTLE GAME without a cleared game (pc/glue/title_glue.c) */
    int  difficulty_select;  /* NEW GAME asks for the difficulty as the PAL/JP versions (adv.c CheckButton) */
    int  no_ink_ribbon;      /* typewriters save without an ink ribbon and use none (pc_no_ink_ribbon) */
    int  save_system;        /* 0 native save files (<save_dir>/saves, default), 1 the memory card (<save_dir>/card0) */
    int  native_3d;
    int  native_tex;         /* rendering rework: TIM2 textures uploaded once, bound afterwards (pc/glue/texture_glue.c) */          /* rendering rework: chunk models drawn by the GPU (pc/glue/native_glue.c) */         /* room transitions (doors, stairs, ladders) without their animation */
    int  hd_textures;        /* PCSX2 HD texture pack from mods/textures or hd_textures_dir (pc/src/gs/hdpack.c) */
    int  hd_textures_font;   /* the pack may replace the game's font pages too (default 0: the original font) */
    int  hd_textures_ui;     /* the pack's replacements that differ from the original (redesigned menus) are used too (default 1) */
    char hd_textures_dir[1024];   /* ini hd_textures_dir, --hd-textures DIR: the pack folder instead of mods/textures */
} HostConfig;
extern HostConfig g_config;
void config_load(void);
void config_save(void);

/* overlay (host/overlay.cpp) */
void overlay_init(void* window, void* gl_context);
int  overlay_event(const void* sdl_event);   /* 1 if the overlay consumed it */
void overlay_render(void);

/* gs_gl.c */
float gs_wanted_scale(void);
void  gs_set_scale(float scale);
void  gs_update_widescreen(void);   /* apply g_config.aspect / window aspect (between frames) */
float gs_picture(float c);          /* a 0..1 grey through brightness/contrast/gamma, as the present shader */
void host_pump_events(void);
void host_present(void);
void host_fatal(const char* fmt, ...);
void host_log(const char* fmt, ...);
void crash_thread_init(void);   /* log.c: first thing in a thread that may crash (the crash report's signal stack) */

/* kernel.c */
void ps2_poll_interrupts(void);
void ps2_wait_vblank(void);
uint64_t ps2_vblank_count(void);

/* memory.c */
#define PS2_SPR_BASE 0x70000000u
#define PS2_SPR_SIZE 0x4000u
void  ps2_memory_init(void);
void* ps2_dma_ptr(uint32_t addr);

/* dma.c / vif.c / gs */
void ps2_vif1_transfer(const uint32_t* data, uint32_t qwc);
void ps2_gif_transfer(int path, const uint32_t* data, uint32_t qwc);

/* cdvd.c */
int  cdvd_open_image(const char* path);
int  cdvd_read_sectors(uint32_t lsn, uint32_t count, void* dst);
int  cdvd_find_file(const char* path, uint32_t* lsn, uint32_t* size);

#ifdef __cplusplus
}
#endif
void ps2_vif1_transfer_words(const uint32_t* w, uint32_t n);
