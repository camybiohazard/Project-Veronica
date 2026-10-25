/* Update check (update.h): one background thread per run, at most 3 attempts. */
#include "update.h"
#include "net.h"
#include "../ps2_platform.h"
#include <SDL3/SDL.h>

void host_log(const char* fmt, ...);

static SDL_AtomicInt state = { UPD_UNKNOWN };   /* UPD_CHECKING while the thread runs, then KNOWN or UNKNOWN */
static char newest[32];                         /* written before state turns UPD_KNOWN */
static SDL_AtomicInt started;

const char* update_installed(void)
{
    static char s[24];
    if (!s[0]) snprintf(s, sizeof(s), "v%.*s", (int)strcspn(RECVX_VERSION, " "), RECVX_VERSION);
    return s;
}

static int env_off(void) { const char* e = SDL_getenv("RECVX_UPDATE_CHECK"); return e && !strcmp(e, "0"); }

static int ask(void)
{
    static const int wait_s[3] = { 0, 5, 30 };
    const char* url = SDL_getenv("RECVX_UPDATE_URL_TEST");
    char host[128], hp[140], path[256], req[512], buf[4096];
    int port, i, len;
    if (!url) url = RECVX_UPDATE_URL;
    if (!upd_parse_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        host_log("update: cannot use %s (only http://host[:port]/path)\n", url);
        return 0;
    }
    snprintf(hp, sizeof(hp), port == 80 ? "%s" : "%s:%d", host, port);
    len = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: recvx-enhanced-port/%s\r\n\r\n", path, hp, update_installed() + 1);
    for (i = 0; i < 3; i++) {
        NetAddr a;
        const char* why = NULL;
        int n;
        SDL_Delay(wait_s[i] * 1000);
        if (!g_config.update_check) return 0;   /* turned off meanwhile */
        if (!net_resolve(host, port, &a)) why = "cannot resolve the server's name";
        else if ((n = net_tcp_fetch(&a, req, len, buf, sizeof(buf), 5000, 10000, &why)) >= 0 && !upd_parse_response(buf, n, newest, sizeof(newest)))
            why = "the answer is not a version number";
        if (!why) {
            host_log("update: newest version %s (installed %s)\n", newest, update_installed());
            return 1;
        }
        host_log("update: attempt %d of 3: %s\n", i + 1, why);
    }
    host_log("update: no answer; not asked again until the next start\n");
    return 0;
}

static int SDLCALL check(void* ud)
{
    (void)ud;
    crash_thread_init();
    SDL_SetAtomicInt(&state, ask() ? UPD_KNOWN : UPD_UNKNOWN);
    return 0;
}

void update_start(void)
{
    SDL_Thread* t;
    if (!g_config.update_check || env_off() || SDL_getenv("RECVX_UPDATE_FAKE") || !SDL_CompareAndSwapAtomicInt(&started, 0, 1)) return;
    SDL_SetAtomicInt(&state, UPD_CHECKING);
    if ((t = SDL_CreateThread(check, "update", NULL))) SDL_DetachThread(t);   /* detached: exit does not wait for it */
    else SDL_SetAtomicInt(&state, UPD_UNKNOWN);
}

int update_newest(char* out, int cap)
{
    const char* fake = SDL_getenv("RECVX_UPDATE_FAKE");
    int s;
    if (fake) { SDL_strlcpy(out, fake, cap); return UPD_KNOWN; }
    if (!g_config.update_check || env_off()) return UPD_OFF;
    if ((s = SDL_GetAtomicInt(&state)) == UPD_KNOWN) SDL_strlcpy(out, newest, cap);   /* (SDL's atomics are full barriers) */
    return s;
}
