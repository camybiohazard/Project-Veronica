/*
 * Host entry point: configuration, SDL window/GL context, event pump, and the
 * hand-off into the original game's main() (renamed recvx_game_main).
 */
#include "ps2_platform.h"
#include "../gs/stream.h"
#include "prof.h"
#include "tps.h"
#include "menu_mouse.h"
#include "bindings.h"
#include "settings.h"
#include "../render/hdr.h"
void render_loop(void);
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

int recvx_game_main(int argc, char* argv[]);
void audio_init(void);
void gs_renderer_init(void);

int pc_screen_filter_mode(void) { return g_config.screen_filter; }
/* "Skip door animations" (decomp system.c bhSysCallDoordemo): 0 the animation, 1 instant (the
   load screen), 2 the animation until a key, mouse button or gamepad button is pressed */
int pc_skip_door_demo(void) { return g_config.skip_doors == 1; }
/* "Fast loading" (decomp system.c bhSysCallMonitor/bhSysCallSndMonitor, sdfunc.c pc_load_sound_pack_now) */
int pc_fast_loading(void) { return g_config.fast_loading; }
static SDL_AtomicInt presses;   /* key, mouse and gamepad button presses (host_pump_events) */
int pc_door_skip_pressed(int start)
{
    static int at;   /* game thread */
    int n = SDL_GetAtomicInt(&presses);
    if (start) at = n;   /* the demo begins: the press that opened the door is older */
    return g_config.skip_doors == 2 && n != at;
}
int pc_intro_pressed(int start)
{
    static int at;   /* game thread */
    int n = SDL_GetAtomicInt(&presses);
    if (start) at = n;   /* a screen begins: older presses (and held keys) do not count */
    return n != at;
}
int pc_quick_knife_on(void) { return g_config.quick_knife; }
int pc_fog_enhanced(void) { return g_config.fog_style == 1; }
int pc_knife_always_on(void) { return g_config.knife_always; }
int pc_weapon_shortcuts_on(void) { return g_config.weapon_shortcuts; }
int pc_head_blowoff_on(void) { return g_config.head_blowoff; }
int pc_ammo_hud_on(void) { return g_config.ammo_hud; }
int pc_auto_stairs_on(void) { return g_config.auto_stairs; }
/* event.c bhPlItemCheck, ps2_McSaveFile.c mcWriteStartSaveFile: typewriters save without an ink ribbon and use none */
int pc_no_ink_ribbon(void) { return g_config.no_ink_ribbon; }
/* the game tests a few joints of a model against the screen; with a wide or moving free
   camera that drops models that are partly visible, so third person never culls */
extern int tps_cam_active;
int pc_model_cull(void) { return g_config.model_cull && !tps_cam_active; }
int pc_frame_blend(void) { return g_config.frame_blend < 0 ? !g_config.interpolate : g_config.frame_blend; }
int pc_battle_unlocked(void) { return g_config.battle_unlocked; }
int pc_difficulty_select(void) { return g_config.difficulty_select; }
int pc_skip_intro(void) { return g_config.skip_intro; }
int pc_movie_zoom(void) { return g_config.movie_zoom; }
int pc_light_falloff(void) { return g_config.light_falloff; }
/* Soft shading and Wet surfaces on: their strength, % */
#define SOFT_ON 20
#define WET_ON 100
int pc_light_soft(void) { return g_config.light_soft ? SOFT_ON : 0; }
/* the lighting mode (Lighting): the main (parallel) lights and the ambient light, % of the original;
   Nightmare also thins the fog (pc_fog_scale), whose colour would light the dark scene */
static const int light_modes[4][2] = { { 100, 100 }, { 75, 75 }, { 50, 25 }, { 5, 5 } };
#define NIGHTMARE_FOG 10
int pc_light_main(void) { return light_modes[g_config.lighting & 3][0]; }
int pc_light_ambient(void) { return light_modes[g_config.lighting & 3][1]; }
int pc_native_3d(void);
int pc_flashlight_lit(void);   /* light_glue.c: per-pixel lighting while the flashlight shines */
int pc_wet_active(void);       /* light_glue.c: wet surfaces while it rains */
int mat_present(void);        /* gs/materials.c: mods/materials.ini has entries (loaded by the render thread) */
int pc_light_pixel(void) { return (g_config.light_pixel || pc_flashlight_lit() || pc_wet_active() || (g_config.mat_shine > 0 && mat_present())) && pc_native_3d(); }
int pc_light_wet(void) { return g_config.wet ? WET_ON : 0; }
/* the flashlight (Flashlight on): brightness %, range (game units, about 10 cm), half the beam's
   angle (degrees), minutes a full battery lasts and minutes from empty to full while off */
#define FLASH_BRIGHTNESS 45
#define FLASH_RANGE 150
#define FLASH_BEAM 20
#define FLASH_BATTERY 4
#define FLASH_RECHARGE 3
int pc_flashlight_enabled(void) { return g_config.flashlight; }
void pc_flashlight_battery_config(int* life, int* recharge) { *life = FLASH_BATTERY; *recharge = FLASH_RECHARGE; }
void pc_flashlight_config(int* brightness, int* range, int* beam)
{
    *brightness = FLASH_BRIGHTNESS; *range = FLASH_RANGE; *beam = FLASH_BEAM;
}
/* fog at i % of the original: the fog density (njCalcFogPower) x 100 / i, so everything's fog is
   that of something i % as far: 50 % = x 2. 0 %: every distance reads the table's nearest
   entry, 254 or 255 of 255 (no fog). The third-person camera takes 65 % of the setting (100 % looked too foggy close
   behind the player in many rooms). Lighting Nightmare: NIGHTMARE_FOG % instead of the setting */
float pc_fog_scale(void)
{
    float i = (g_config.lighting == 3 ? NIGHTMARE_FOG : g_config.fog_intensity) * (tps_cam_active ? 0.65f : 1.0f);
    return i <= 0 ? 1e6f : 100.0f / i;
}
int pc_skip_intro_press_start(void)
{
    static int done;
    if (!g_config.skip_intro || done) return 0;
    done = 1;
    return 1;
}
int pc_config_native_tex(void) { return g_config.native_tex; }
int pc_native_3d(void)
{
    static int env = -2;
    if (env == -2) env = getenv("RECVX_NATIVE3D") ? atoi(getenv("RECVX_NATIVE3D")) != 0 : -1;
    return env >= 0 ? env : g_config.native_3d;
}

/* full-screen 2D overlays: 0 original 4:3, 1 stretched to the (widened) view, 2 off;
   fades always cover the whole view */
enum { PC_OVL_CINEMA, PC_OVL_FADE, PC_OVL_EFFECT };   /* as in pc_prelude.h */
static int overlay_stretch;
int pc_overlay_begin(int kind)
{
    int mode = kind == PC_OVL_CINEMA ? g_config.cinema_bars : kind == PC_OVL_EFFECT ? g_config.screen_effects : 1;
    overlay_stretch = mode == 1;
    return mode != 2;   /* Off: the caller skips the layer, or (the oil pressure puzzle) draws it 4:3 */
}
void pc_overlay_end(void) { overlay_stretch = 0; }
int pc_overlay_wide(void) { return overlay_stretch; }
float pc_overlay_x(float x)
{
    extern float pc_hor_scale(void);
    return overlay_stretch ? 320.0f + (x - 320.0f) * pc_hor_scale() : x;
}

HostConfig g_config = { .window_w = 1280, .window_h = 960, .fullscreen = 1, .borderless_extra = 1, .vsync = 1, .log_level = 1, .music_volume = 100, .sfx_volume = 100, .rumble = 100, .interpolate = 1, .frame_blend = 0, .aspect = 2, .fsr = 1, .fog_glow = 1, .fog_beam = 1, .flash_shadow = 1,
                       .screen_filter = 1, .cinema_bars = 1, .screen_effects = 1, .native_3d = 1, .model_cull = 1, .native_tex = 1, .hd_textures = 1, .hd_textures_ui = 1, .quick_knife = 1, .head_blowoff = 1, .ammo_hud = 1,
                       .skip_doors = 1, .fast_loading = 1, .update_check = 1, .auto_stairs = 1, .skip_intro = 1, .battle_unlocked = 1, .difficulty_select = 1, .contrast = 100, .gamma = 10, .fog_intensity = 100,
                       .light_falloff = 1, .lighting = 1,
                       .flashlight = 1, .mat_shine = 1, .dither = 1, .aniso = 4,
                       .hdr_highlights = 50 };   /* first-start defaults; recvx.ini overrides */
SDL_Window* g_window;
SDL_GLContext g_gl;

void host_log(const char* fmt, ...)
{
    extern void log_keep(const char* fmt, va_list ap);
    va_list ap;
    va_start(ap, fmt);
    log_keep(fmt, ap);   /* the last messages, for the crash report (log.c) */
    va_end(ap);
    if (g_config.log_level <= 0) return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void host_fatal(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "fatal: %s\n", buf);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "recvx", buf, g_window);
    exit(1);
}

void host_pump_events(void)
{
    SDL_Event ev;
    bind_test_frame();
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_GAMEPAD_ADDED || ev.type == SDL_EVENT_GAMEPAD_REMOVED) { extern void pad_devices_changed(void); pad_devices_changed(); }
        if (bind_event(&ev)) continue;   /* a "press a key" of the settings */
        if (overlay_event(&ev)) continue;
        if ((ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) || ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            SDL_AddAtomicInt(&presses, 1);
        tps_input_event(&ev);
        menu_mouse_event(&ev);
        if (bind_hotkey(&ev, B_TPS_TOGGLE)) {   /* a key, mouse button or gamepad input */
            g_tps.on = !g_tps.on;
            settings_save_later();
        }
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            exit(0);
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:   /* moved to another monitor: its modes for Fullscreen resolution */
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:   /* the desktop resized: a borderless window covers it again */
            settings_display_modes();
            break;
        case SDL_EVENT_KEY_DOWN:
            if (bind_hotkey(&ev, B_FULLSCREEN)) {
                g_config.fullscreen = !g_config.fullscreen;   /* of the kind chosen last (Display mode) */
                settings_apply_display();
                settings_save_later();
            }
            break;
        }
    }
    settings_poll();
}

/* the title menu's EXIT GAME (game thread): the main thread ends the program as when the window is closed */
void host_request_quit(void)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

#ifndef _WIN32
/* debugging aid: RECVX_WATCHDOG=<seconds> prints a backtrace and exits (headless runs) */
static void watchdog(int sig)
{
    void* frames[64];
    int n = backtrace(frames, 64);
    (void)sig;
    fprintf(stderr, "watchdog: backtrace\n");
    backtrace_symbols_fd(frames, n, 2);
    if (getenv("RECVX_PROFILE")) exit(3);   /* run atexit: profile dump */
    _exit(3);
}
#endif

static void usage(void)
{
    fprintf(stderr,
        "cvx-port " RECVX_VERSION "\n"
        "usage: recvx [options] <game disc image (.iso)>\n"
        "  --save-dir <dir>   settings and memory card directory (default: ./save if it exists,\n"
        "                     else save/ next to the program, which also holds mods/ and logs)\n"
        "  --size WxH         start windowed at that size (default: borderless fullscreen; windowed 1280x960)\n"
        "  --fullscreen       start in fullscreen (borderless, or exclusive if chosen in the settings)\n"
        "  --borderless / --exclusive / --windowed   start in that display mode\n"
        "  --enhanced         display-rate frames (60+ FPS) blended from the 30 FPS game (default)\n"
        "  --vanilla          original 30 FPS presentation\n"
        "  --skip-doors       room transitions (doors, stairs, ladders) without their animation (default;\n"
        "                     skip_doors in recvx.ini: 0 played, 1 instant, 2 played until a key is pressed)\n"
        "  --skip-intro       boot straight into the title menu (default on)\n"
        "  --hd-textures DIR  PCSX2 HD texture pack folder (default mods/textures)\n"
        "  --third-person     over-the-shoulder camera, mouse/keyboard controls (toggle: F5)\n"
        "  --quiet\n"
        "  --version          print the version and exit\n"
        "  --log[=file]       copy all output to a log file (default recvx.log), crash backtraces included\n"
        "  --profile[=file]   per-frame timing CSV (default recvx_profile.csv): logic, vu0, vu1, gs,\n"
        "                     interp, present, wait; also shown live in the F1 overlay\n"
        "  --profile-samples=file  sampling CPU profiler (summarise with pc/tools/profile.py)\n"
        "F1 opens the settings overlay; settings are saved in <save dir>/recvx.ini (options set there,\n"
        "e.g. skip_doors = 0 or skip_intro = 0, override the defaults).\n"
        "The image can also be given with the RECVX_ISO environment variable. Without one, a file\n"
        "selector asks for it on the first start and recvx.ini remembers it (key iso).\n");
}

static char log_path[1024], profile_path[1024];
static int iso_given;   /* image from the command line / RECVX_ISO: no file selector */
static int save_given;  /* --save-dir */
static char start_dir[1024];   /* where the program was started: relative image paths are taken from here */

static void iso_from(const char* s)
{
    int absolute = s[0] == '/' || s[0] == '\\' || (s[0] && s[1] == ':');
    if (absolute || !start_dir[0]) SDL_strlcpy(g_config.iso_path, s, sizeof(g_config.iso_path));
    else SDL_snprintf(g_config.iso_path, sizeof(g_config.iso_path), "%s%s", start_dir, s);
    iso_given = 1;
}

/* Started from somewhere else (a file manager, a shortcut, another directory): the program's own
   folder holds save/, mods/ and the logs, so the release binaries need no start script. A save
   folder in the current directory or an explicit --save-dir keeps the current directory (several
   instances with their own folders, test setups). */
static void home_dir(void)
{
    const char* base;
    if (save_given || SDL_GetPathInfo("save", NULL)) return;
    if (!(base = SDL_GetBasePath())) return;
    pc_chdir(base);
}

static void parse_args(int argc, char** argv)
{
    const char* env = SDL_getenv("RECVX_ISO");   /* UTF-8 on Windows too */
    if (env) iso_from(env);
    SDL_strlcpy(g_config.save_dir, "save", sizeof(g_config.save_dir));
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) { SDL_strlcpy(g_config.save_dir, argv[++i], sizeof(g_config.save_dir)); save_given = 1; }
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {   /* a window of that size (the default display mode is borderless fullscreen) */
            sscanf(argv[++i], "%dx%d", &g_config.window_w, &g_config.window_h);
            g_config.fullscreen = 0;
        }
        else if (!strcmp(argv[i], "--fullscreen")) g_config.fullscreen = 1;   /* the kind of recvx.ini (borderless by default) */
        else if (!strcmp(argv[i], "--borderless")) { g_config.fullscreen = 1; g_config.fullscreen_exclusive = 0; }
        else if (!strcmp(argv[i], "--exclusive")) { g_config.fullscreen = 1; g_config.fullscreen_exclusive = 1; }
        else if (!strcmp(argv[i], "--windowed")) g_config.fullscreen = 0;
        else if (!strcmp(argv[i], "--enhanced")) g_config.interpolate = 1;
        else if (!strcmp(argv[i], "--vanilla")) g_config.interpolate = 0;
        else if (!strcmp(argv[i], "--skip-doors")) g_config.skip_doors = 1;
        else if (!strcmp(argv[i], "--skip-intro")) g_config.skip_intro = 1;
        else if (!strcmp(argv[i], "--hd-textures") && i + 1 < argc) { SDL_strlcpy(g_config.hd_textures_dir, argv[++i], sizeof(g_config.hd_textures_dir)); }
        else if (!strcmp(argv[i], "--third-person")) g_tps.on = 1;
        else if (!strcmp(argv[i], "--quiet")) g_config.log_level = 0;
        else if (!strncmp(argv[i], "--log", 5) && (argv[i][5] == 0 || argv[i][5] == '='))
            SDL_strlcpy(log_path, argv[i][5] ? argv[i] + 6 : "recvx.log", sizeof(log_path));
        else if (!strncmp(argv[i], "--profile-samples=", 18)) SDL_setenv_unsafe("RECVX_PROFILE", argv[i] + 18, 1);
        else if (!strncmp(argv[i], "--profile", 9) && (argv[i][9] == 0 || argv[i][9] == '='))
            SDL_strlcpy(profile_path, argv[i][9] ? argv[i] + 10 : "recvx_profile.csv", sizeof(profile_path));
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); exit(0); }
        else if (!strcmp(argv[i], "--version")) { printf("cvx-port " RECVX_VERSION "\n"); exit(0); }
        else if (argv[i][0] != '-') iso_from(argv[i]);
        else { usage(); exit(1); }
    }
}

/* First start (or the remembered image is gone): ask for the disc image, remember it in recvx.ini. */
static void SDLCALL iso_picked(void* done, const char* const* files, int filter)
{
    (void)filter;
    if (!files) host_log("file selector: %s\n", SDL_GetError());
    else if (files[0]) SDL_strlcpy(g_config.iso_path, files[0], sizeof(g_config.iso_path));
    SDL_SetAtomicInt(done, 1);
}

static void pick_iso(void)
{
    static const SDL_DialogFileFilter filters[] = { { "PS2 disc image (*.iso)", "iso" }, { "All files", "*" } };
    static SDL_AtomicInt done;
    SDL_PropertiesID props = SDL_CreateProperties();
    g_config.iso_path[0] = 0;
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, (void*)filters);
    SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, SDL_arraysize(filters));
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Select the Resident Evil Code: Veronica X disc image (SLUS-20184)");
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, iso_picked, &done, props);
    SDL_DestroyProperties(props);
    while (!SDL_GetAtomicInt(&done)) { SDL_Event ev; SDL_WaitEventTimeout(&ev, 50); }
}

/* The game (the decomp's main loop, VU, GIF recording) runs on its own thread; the main
   thread renders (host/render.c). */
static int SDLCALL game_thread(void* ud)
{
    char* game_argv[] = { "cdrom0:\\SLUS_201.84;1", NULL };
    (void)ud;
    crash_thread_init();
    prof_thread(PROF_LOGIC);
    { extern void profile_thread(char); profile_thread('g'); }   /* RECVX_PROFILE */
    exit(recvx_game_main(1, game_argv));
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* the game's printf diagnostics */
    ps2_memory_init();   /* first: a DLL loaded later (the GPU driver's) moves aside, one loaded before can't */
    argv = pc_utf8_argv(argc, argv);   /* paths are UTF-8 everywhere (fileio.h) */
    { char* cwd = SDL_GetCurrentDirectory(); if (cwd) { SDL_strlcpy(start_dir, cwd, sizeof(start_dir)); SDL_free(cwd); } }
    parse_args(argc, argv);
    home_dir();
    { extern void crash_init(const char*); crash_init(RECVX_VERSION); }   /* recvx-crash.log on a crash, with or without --log */
    if (log_path[0]) { extern void log_start(const char*); log_start(log_path); }
    host_log("cvx-port " RECVX_VERSION "\n");
    if (profile_path[0]) { extern void prof_open_csv(const char*); prof_open_csv(profile_path); }
#ifndef _WIN32
    if (getenv("RECVX_WATCHDOG")) { signal(SIGALRM, watchdog); alarm((unsigned)atoi(getenv("RECVX_WATCHDOG"))); }
    { extern void profile_start(void); profile_start(); }
#endif
    config_load();
    parse_args(argc, argv);   /* command line overrides the saved settings */
    { extern void variants_load(void); variants_load(); }   /* mods/enemies.ini: custom enemies (variants.h) */

#ifdef __linux__
    /* HDR needs Wayland (render/hdr_vk.c) - except in gamescope, whose Wayland offers no colour
       management: its WSI layer gives X11 windows HDR. SDL_VIDEO_DRIVER still decides. */
    if (g_config.hdr) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, SDL_getenv("GAMESCOPE_WAYLAND_DISPLAY") ? "x11" : "wayland,x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
        host_fatal("SDL_Init: %s", SDL_GetError());
    bind_apply_source();   /* Gamepad only: gamepads also in the background */
    { extern void update_start(void); update_start(); }   /* Check for updates: GitHub's latest release, in the background */

    if (!iso_given && (!g_config.iso_path[0] || !cdvd_open_image(g_config.iso_path))) {
        pick_iso();
        if (!g_config.iso_path[0]) { usage(); host_fatal("no game disc image selected (or start with: recvx <image.iso>)"); }
        if (!cdvd_open_image(g_config.iso_path)) host_fatal("cannot open game disc image '%s'", g_config.iso_path);
        config_save();
    } else {
        if (!g_config.iso_path[0]) { usage(); host_fatal("no game disc image given"); }
        if (!cdvd_open_image(g_config.iso_path)) host_fatal("cannot open game disc image '%s'", g_config.iso_path);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    int have_window = 0;
#ifdef _WIN32
    if (g_config.hdr) {
        /* HDR output (render/hdr_win.c): Direct3D 11 presents to the window, which therefore has no
           OpenGL pixel format; the context lives on a hidden window of its own and draws only into
           framebuffers. Without HDR (no interop, no scRGB swapchain): the usual OpenGL window, the
           same context made current on it. */
        SDL_Window* gl_window = SDL_CreateWindow("recvx OpenGL", 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        g_window = SDL_CreateWindow("Resident Evil Code: Veronica X", g_config.window_w, g_config.window_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        if (!gl_window || !g_window) host_fatal("SDL_CreateWindow: %s", SDL_GetError());
        g_gl = SDL_GL_CreateContext(gl_window);
        if (!g_gl) host_fatal("SDL_GL_CreateContext: %s", SDL_GetError());
        gs_renderer_init();
        if (!hdr_init(g_window)) {
            SDL_DestroyWindow(g_window);
            g_window = SDL_CreateWindow("Resident Evil Code: Veronica X", g_config.window_w, g_config.window_h,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
            if (!g_window) host_fatal("SDL_CreateWindow: %s", SDL_GetError());
            if (!SDL_GL_MakeCurrent(g_window, g_gl)) host_fatal("SDL_GL_MakeCurrent: %s", SDL_GetError());
            SDL_DestroyWindow(gl_window);
            SDL_GL_SetSwapInterval(g_config.vsync ? 1 : 0);
        }
        have_window = 1;
    }
#elif defined(__linux__)
    if (g_config.hdr) {
        /* HDR output (render/hdr_vk.c): Vulkan presents to the window (Wayland, asked for above); the
           OpenGL context has no window at all (EGL device platform), as it draws only into
           framebuffers. Without HDR: SDL's video restarted with its usual driver (X11 under most
           desktops; some drivers cannot make Wayland OpenGL windows) and the usual OpenGL window. */
        int ok = 0;
        g_window = SDL_CreateWindow("Resident Evil Code: Veronica X", g_config.window_w, g_config.window_h,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        if (!g_window) host_log("hdr: unavailable (no Vulkan window: %s), SDR output\n", SDL_GetError());
        else if (hdr_gl_headless()) {
            gs_renderer_init();
            ok = hdr_init(g_window);
        } else host_log("hdr: unavailable (no headless OpenGL context), SDR output\n");
        if (!ok) {
            if (g_window) SDL_DestroyWindow(g_window);
            g_window = NULL;
            hdr_gl_headless_release();
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) host_fatal("SDL_InitSubSystem: %s", SDL_GetError());
        }
        have_window = ok;
    }
#endif
    if (have_window) {
        settings_display_modes();
        settings_apply_display();
        SDL_ShowWindow(g_window);
    } else {
        g_window = SDL_CreateWindow("Resident Evil Code: Veronica X", g_config.window_w, g_config.window_h,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        if (!g_window && !SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland")) {
            /* NVIDIA's 32-bit EGL makes no Wayland window surfaces: X11 (XWayland) instead */
            host_log("display: no OpenGL window under Wayland (%s), using X11\n", SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) host_fatal("SDL_InitSubSystem: %s", SDL_GetError());
            g_window = SDL_CreateWindow("Resident Evil Code: Veronica X", g_config.window_w, g_config.window_h,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        }
        if (!g_window) host_fatal("SDL_CreateWindow: %s", SDL_GetError());
        settings_display_modes();
        settings_apply_display();   /* hidden: fullscreen (and its mode) are taken when it is shown */
        SDL_ShowWindow(g_window);
        g_gl = SDL_GL_CreateContext(g_window);
        if (!g_gl) host_fatal("SDL_GL_CreateContext: %s", SDL_GetError());
        SDL_GL_SetSwapInterval(g_config.vsync ? 1 : 0);
        gs_renderer_init();
    }
    overlay_init(g_window, g_gl);
    audio_init();

    stream_init();
    if (!SDL_CreateThread(game_thread, "game", NULL)) host_fatal("cannot start the game thread: %s", SDL_GetError());
    render_loop();   /* the main thread renders and presents; never returns */
    return 0;
}
