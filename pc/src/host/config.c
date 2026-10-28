/* User settings: <save_dir>/recvx.ini, "key = value" lines. */
#include "../ps2_platform.h"
#include "actors.h"
#include "tps.h"
#include "menu_mouse.h"
#include "enemies.h"
#include "bindings.h"
#include "modern.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const struct { const char* key; int* val; int lo, hi; } keys[] = {
    { "window_w", &g_config.window_w, 320, 16384 },
    { "window_h", &g_config.window_h, 240, 16384 },
    { "fullscreen", &g_config.fullscreen, 0, 1 },
    { "fullscreen_exclusive", &g_config.fullscreen_exclusive, 0, 1 },   /* 0 borderless, 1 exclusive */
    { "fullscreen_w", &g_config.fullscreen_w, 0, 16384 },   /* exclusive: 0 = the desktop's resolution and refresh */
    { "fullscreen_h", &g_config.fullscreen_h, 0, 16384 },
    { "fullscreen_hz", &g_config.fullscreen_hz, 0, 1000 },
    { "borderless_extra", &g_config.borderless_extra, 0, 16 },   /* Windows borderless: 1 px past the monitor, 0 = exactly it */
    { "vsync", &g_config.vsync, 0, 1 },
    { "fps_limit", &g_config.fps_limit, 0, 5 },
    { "scale", &g_config.scale, 0, 8 },
    { "aspect", &g_config.aspect, 0, 2 },
    { "show_fps", &g_config.show_fps, 0, 1 },
    { "music_volume", &g_config.music_volume, 0, 100 },
    { "sfx_volume", &g_config.sfx_volume, 0, 100 },
    { "rumble_strength", &g_config.rumble, 0, 100 },
    { "screen_filter", &g_config.screen_filter, 0, 2 },
    { "cinema_bars", &g_config.cinema_bars, 0, 2 },
    { "screen_effects", &g_config.screen_effects, 0, 2 },
    { "movie_zoom", &g_config.movie_zoom, 0, 1 },
    { "fog_intensity", &g_config.fog_intensity, 0, 100 },
    { "brightness", &g_config.brightness, -50, 50 },
    { "contrast", &g_config.contrast, 50, 150 },
    { "gamma", &g_config.gamma, 5, 20 },
    { "skip_doors", &g_config.skip_doors, 0, 2 },     /* 0 off, 1 instant, 2 skippable */
    { "fast_loading", &g_config.fast_loading, 0, 1 },
    { "quick_knife", &g_config.quick_knife, 0, 1 },
    { "auto_stairs", &g_config.auto_stairs, 0, 1 },
    { "head_blowoff", &g_config.head_blowoff, 0, 1 },
    { "ammo_hud", &g_config.ammo_hud, 0, 1 },
    { "model_cull", &g_config.model_cull, 0, 1 },
    { "skip_intro", &g_config.skip_intro, 0, 1 },
    { "battle_unlocked", &g_config.battle_unlocked, 0, 1 },
    { "difficulty_select", &g_config.difficulty_select, 0, 1 },
    { "no_ink_ribbon", &g_config.no_ink_ribbon, 0, 1 },   /* typewriters save without an ink ribbon */
    { "save_system", &g_config.save_system, 0, 1 },     /* 0 native files, 1 memory card */
    { "native_3d", &g_config.native_3d, 0, 1 },
    { "native_tex", &g_config.native_tex, 0, 1 },
    { "hd_textures", &g_config.hd_textures, 0, 1 },
    { "hd_textures_font", &g_config.hd_textures_font, 0, 1 },
    { "hd_textures_ui", &g_config.hd_textures_ui, 0, 1 },
    { "interpolate", &g_config.interpolate, 0, 1 },
    { "frame_blend", &g_config.frame_blend, -1, 1 },
    { "appear_char", &g_appear_chr, 0, APPEAR_CHR_MAX },   /* "You appear as" */
    { "appear_gender_anim", &g_appear_anim, 0, 1 },
    { "appear_cutscene_face", &g_appear_face, 0, 1 },
    { "appear_in_cutscenes", &g_appear_cutscene, 0, 1 },
    { "menu_mouse", (int*)&g_menu_mouse.on, 0, 1 },
    { "input_source", &g_input_source, 0, 2 },            /* INPUT_*: Auto, Keyboard/mouse only, Gamepad only */
    { "input_pad", &g_input_pad, 0, 4 },                  /* 0 any gamepad, n player n's (bindings.c) */
    { "fixed_cam_controls", &g_modern_on, 0, 1 },        /* fixed cameras: 0 classic (tank), 1 modern (camera-relative) */
    { "enemy_extra", &g_enemy_opts.extra, 0, 3 },        /* enemy options (docs/ENEMIES.md "Enemy options") */
    { "enemy_place", &g_enemy_opts.place, 0, 1 },
    { "enemy_random", &g_enemy_opts.rand, 0, 1 },
    { "enemy_seed", &g_enemy_opts.seed, 0, 0x7FFFFFFF },
    { "enemy_variety", &g_enemy_opts.variety, 0, 1 },    /* 0 Mild, 1 Wild */
    { "enemy_random_events", &g_enemy_opts.events, 0, 1 },
    { "tps", (int*)&g_tps.on, 0, 1 },
    { "tps_fov", &g_tps.fov, 30, 130 },
    { "tps_aim_fov", &g_tps.aim_fov, 20, 130 },
    { "tps_distance", &g_tps.dist, 0, 600 },
    { "tps_aim_distance", &g_tps.aim_dist, 0, 600 },
    { "tps_height", &g_tps.height, 0, 300 },
    { "tps_shoulder", &g_tps.shoulder, 0, 200 },
    { "tps_left_shoulder", &g_tps.left, 0, 1 },
    { "tps_sens_x", &g_tps.sens_x, 1, 1000 },
    { "tps_sens_y", &g_tps.sens_y, 1, 1000 },
    { "tps_aim_sens", &g_tps.aim_sens, 1, 300 },
    { "tps_invert_y", &g_tps.invert_y, 0, 1 },
    { "tps_crosshair", &g_tps.crosshair, 0, 1 },
    { "tps_auto_aim", &g_tps.auto_aim, 0, 1 },
    { "tps_precise_hits", &g_tps.precise, 0, 1 },
    { "tps_atmos", &g_tps.atmos, 0, 1 },
    { "tps_free_camera", &g_tps.free_cam, 0, 1 },
    { "tps_key", &g_tps.key, 1, 511 },
    { "tps_pad_controls", &g_tps.pad_modern, 0, 1 },     /* third-person gamepad: 0 classic (tank), 1 modern (twin-stick) */
    { "tps_pad_sens_x", &g_tps.pad_sens_x, 1, 1000 },
    { "tps_pad_sens_y", &g_tps.pad_sens_y, 1, 1000 },
    { "tps_pad_aim_sens", &g_tps.pad_aim_sens, 1, 300 },
    { "tps_pad_invert_y", &g_tps.pad_invert_y, 0, 1 },
    { "tps_pad_dead_move", &g_tps.pad_dead_move, 0, 90 },
    { "tps_pad_dead_look", &g_tps.pad_dead_look, 0, 90 },
};

static char* trim(char* s)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == ' ') s[--n] = 0;
    return s;
}

static char ini_hd_dir[1024];

static void path(char* out, size_t n) { SDL_snprintf(out, n, "%s/recvx.ini", g_config.save_dir); }

void config_load(void)
{
    char p[1100], line[1100], k[64], val[128];
    int v;
    FILE* f;
    path(p, sizeof(p));
    if (!(f = pc_fopen(p, "r"))) return;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, " iso = %1023[^\r\n]", g_config.iso_path) == 1) continue;
        else if (sscanf(line, " hd_textures_dir = %1023[^\r\n]", ini_hd_dir) == 1) { SDL_strlcpy(g_config.hd_textures_dir, ini_hd_dir, sizeof(ini_hd_dir)); continue; }
        else if (sscanf(line, " %63[a-z_0-9] = %127[^\r\n]", k, val) == 2 && bind_ini_line(k, trim(val))) continue;
        else if (sscanf(line, " %63[a-z_0-9] = %d", k, &v) == 2)
            for (size_t i = 0; i < SDL_arraysize(keys); i++)
                if (!strcmp(k, keys[i].key) && v >= keys[i].lo && v <= keys[i].hi) *keys[i].val = v;
    fclose(f);
    g_appear_chr = appear_canon(g_appear_chr);   /* an event double no longer listed: its listed twin (actors.c) */
}

static int write_ini(FILE* f, void* ud)
{
    (void)ud;
    for (size_t i = 0; i < SDL_arraysize(keys); i++) fprintf(f, "%s = %d\n", keys[i].key, *keys[i].val);
    if (g_config.iso_path[0]) fprintf(f, "iso = %s\n", g_config.iso_path);
    if (ini_hd_dir[0]) fprintf(f, "hd_textures_dir = %s\n", ini_hd_dir);   /* not --hd-textures */
    bind_ini_write(f);
    return 0;
}

/* Main thread only (settings_poll, and main() before the game thread starts); everyone else
   asks with settings_save_later(). Written to a temporary file and renamed over recvx.ini, so a
   crash or full disk never leaves a truncated file. */
void config_save(void)
{
    char p[1100];
    SDL_CreateDirectory(g_config.save_dir);
    path(p, sizeof(p));
    if (pc_save_file(p, "w", write_ini, NULL) != 0) host_log("config: cannot write %s\n", p);
}
