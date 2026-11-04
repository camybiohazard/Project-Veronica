/* Update check (Check for updates, g_config.update_check): at start, at most once a day
   (update_last), a background thread asks GitHub for the project's latest release
   (update_net.c, GitHub's "latest release": drafts and pre-releases are left out, so test builds published as pre-releases are
   not announced) and keeps its version (update_latest, 1.2.3 as 1002003; both ini-only, written
   with the other settings). A newer one than this build (RECVX_VERSION_MAJOR/MINOR/PATCH, the
   CMake project version, which release tags follow) is shown on the title screen and in F1
   (overlay.cpp). One anonymous request, nothing about the player sent; any failure (offline, no
   curl on Linux, GitHub unreachable) is logged and ignored. Headless runs (offscreen video, the tests) and
   RECVX_NO_UPDATE_CHECK skip it. */
#include "update.h"
#include "settings.h"
#include "../ps2_platform.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void host_log(const char* fmt, ...);

#define CHECK_EVERY (24 * 3600)   /* seconds */
#define ENC(a, b, c) ((a) * 1000000 + (b) * 1000 + (c))
#define STR2(x) #x
#define STR(x) STR2(x)

static const int current = ENC(RECVX_VERSION_MAJOR, RECVX_VERSION_MINOR, RECVX_VERSION_PATCH);
const char* update_current(void) { return STR(RECVX_VERSION_MAJOR) "." STR(RECVX_VERSION_MINOR) "." STR(RECVX_VERSION_PATCH); }

/* "v1.2.3", "1.2", "1.0.8-fix": the numbers up to three parts, -1 without one */
static int enc_version(const char* s)
{
    int part[3] = { 0, 0, 0 }, k = 0, any = 0;
    if (*s == 'v' || *s == 'V') s++;
    for (; *s && k < 3; s++) {
        if (*s >= '0' && *s <= '9') { if (part[k] < 999) part[k] = part[k] * 10 + (*s - '0'); any = 1; }
        else if (*s == '.' && any) k++;
        else break;
    }
    return any ? ENC(SDL_min(part[0], 999), SDL_min(part[1], 999), SDL_min(part[2], 999)) : -1;
}

/* the release's "tag_name" from GitHub's JSON */
static int tag_name(const char* js, char* out, int n)
{
    const char* p = strstr(js, "\"tag_name\"");
    int k = 0;
    if (!p) return 0;
    p += 10;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':') p++;
    if (*p++ != '"') return 0;
    while (*p && *p != '"' && k < n - 1) out[k++] = *p++;
    out[k] = 0;
    return k > 0;
}

int update_fetch(char* buf, int n);   /* update_net.c: GitHub's answer, its length (0: none) */

static int SDLCALL worker(void* unused)
{
    static char buf[65536];
    char tag[64];
    int len, v;
    (void)unused;
    len = update_fetch(buf, (int)sizeof(buf) - 1);
    if (len <= 0) { host_log("update: no answer from GitHub (offline?)\n"); return 0; }
    buf[len] = 0;
    if (!tag_name(buf, tag, (int)sizeof(tag)) || (v = enc_version(tag)) < 0) { host_log("update: no release version in GitHub's answer\n"); return 0; }
    __atomic_store_n(&g_config.update_latest, v, __ATOMIC_RELEASE);
    __atomic_store_n(&g_config.update_last, (int)time(NULL), __ATOMIC_RELEASE);
    settings_save_later();
    host_log("update: latest release %s, this is %s%s\n", tag, update_current(), v > current ? " (an update is available)" : "");
    return 0;
}

void update_start(void)
{
    time_t now = time(NULL);
    const char* drv = SDL_GetCurrentVideoDriver();
    SDL_Thread* t;
    if (!g_config.update_check || SDL_getenv("RECVX_NO_UPDATE_CHECK")) return;
    if (drv && (!strcmp(drv, "offscreen") || !strcmp(drv, "dummy"))) return;   /* headless: tests */
    if (g_config.update_last > 0 && now >= g_config.update_last && now - g_config.update_last < CHECK_EVERY) return;   /* asked today */
    if ((t = SDL_CreateThread(worker, "update", NULL))) SDL_DetachThread(t);
}

/* the title menu ran now (title_glue.c, game thread); it is up while it ran within the last half second
   (overlay.cpp, main thread: the update notice) */
static SDL_AtomicU32 title_seen;
void update_title_seen(void) { SDL_SetAtomicU32(&title_seen, (Uint32)SDL_GetTicks() | 1u); }
int update_title_up(void)
{
    Uint32 t = SDL_GetAtomicU32(&title_seen);
    return t && (Uint32)SDL_GetTicks() - t < 500;
}

int update_newer(char* ver, int n)
{
    int v = __atomic_load_n(&g_config.update_latest, __ATOMIC_ACQUIRE);
    if (!g_config.update_check || v <= current) return 0;
    if (ver) snprintf(ver, (size_t)n, "%d.%d.%d", v / 1000000, v / 1000 % 1000, v % 1000);
    return 1;
}
