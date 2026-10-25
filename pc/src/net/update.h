/* Update check (docs/USER_GUIDE.md, Gameplay "Check for updates"): once per start a background
   thread asks the project's update server for the newest version number with one plain HTTP GET
   (nothing else is sent; retried twice if it fails); the title shows it below the installed
   version (lobby_glue.c title_version). Nothing is kept. The pure parts below are checked by
   pc/tests/test_update.c. */
#ifndef RECVX_UPDATE_H
#define RECVX_UPDATE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The project's update server, http:// only. It will become a domain name: the host is resolved
   (getaddrinfo), so a move changes only this line. Test aid: RECVX_UPDATE_URL_TEST replaces it. */
#define RECVX_UPDATE_URL "http://85.214.181.87:1979/"

enum { UPD_OFF, UPD_CHECKING, UPD_UNKNOWN, UPD_KNOWN };
void update_start(void);                    /* main thread: at start, and the setting's applier (one check per run) */
int  update_newest(char* out, int cap);     /* any thread: UPD_*; UPD_KNOWN fills out ("v0.7.0") */
const char* update_installed(void);         /* "v0.6.9": RECVX_VERSION's release part */

/* "http://host[:port][/path]": host (letters, digits, '.', '-'), port (80) and path ("/"); 0 if malformed or not http:// */
static inline int upd_parse_url(const char* url, char* host, int hcap, int* port, char* path, int pcap)
{
    const char* h = url + 7;
    size_t n = strcspn(h, ":/"), i;
    if (strncmp(url, "http://", 7) || !n || n >= (size_t)hcap) return 0;
    for (i = 0; i < n; i++)
        if (!((h[i] >= 'a' && h[i] <= 'z') || (h[i] >= 'A' && h[i] <= 'Z') || (h[i] >= '0' && h[i] <= '9') || h[i] == '.' || h[i] == '-')) return 0;
    memcpy(host, h, n);
    host[n] = 0;
    *port = 80;
    h += n;
    if (*h == ':') {
        long v = 0;
        for (h++, n = 0; *h >= '0' && *h <= '9' && n < 6; h++, n++) v = v * 10 + (*h - '0');
        if (!n || v < 1 || v > 65535 || (*h && *h != '/')) return 0;
        *port = (int)v;
    }
    for (i = 0; h[i]; i++) if (h[i] <= ' ' || h[i] >= 127) return 0;   /* (it goes into the request line) */
    return snprintf(path, pcap, "%s", *h ? h : "/") < pcap;
}

/* "v<digits>.<digits>.<digits>", optionally "-..." or "+..." of up to 15 letters, digits, '.', '-', '+'
   (e.g. v0.7.0-rc1): 1 and the three numbers */
static inline int upd_parse_version(const char* s, unsigned n[3])
{
    int i, d;
    if (*s++ != 'v') return 0;
    for (i = 0; i < 3; i++) {
        if (i && *s++ != '.') return 0;
        for (n[i] = 0, d = 0; *s >= '0' && *s <= '9'; s++, d++) {
            if (d == 9) return 0;
            n[i] = n[i] * 10 + (unsigned)(*s - '0');
        }
        if (!d) return 0;
    }
    if (*s && *s != '-' && *s != '+') return 0;
    for (i = 0; s[i]; i++)
        if (i >= 16 || !((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || strchr(".-+", s[i])))
            return 0;
    return 1;
}

/* newest is a later release than installed (part by part, numerically: v0.6.10 > v0.6.9; a suffix
   does not count); 0 when either is not a version */
static inline int upd_newer(const char* newest, const char* installed)
{
    unsigned a[3], b[3];
    int i;
    if (!upd_parse_version(newest, a) || !upd_parse_version(installed, b)) return 0;
    for (i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
    return 0;
}

/* an HTTP response (len bytes): status 200 and a body that is, trimmed, a version
   (upd_parse_version): 1 and the version in ver; else 0 (an error page, junk, a cut answer) */
static inline int upd_parse_response(const char* r, int len, char* ver, int cap)
{
    unsigned v[3];
    const char *e = r + len, *b = NULL, *p;
    if (len < 12 || strncmp(r, "HTTP/1.", 7) || r[8] != ' ' || strncmp(r + 9, "200", 3) || (len > 12 && r[12] != ' ' && r[12] != '\r' && r[12] != '\n')) return 0;
    for (p = r; p < e - 1 && !b; p++)
        if (p[0] == '\n' && p[1] == '\n') b = p + 2;
        else if (p[0] == '\n' && p[1] == '\r' && p + 2 < e && p[2] == '\n') b = p + 3;
    if (!b) return 0;
    while (b < e && (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n')) b++;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    if (e - b >= cap || memchr(b, 0, (size_t)(e - b))) return 0;
    memcpy(ver, b, (size_t)(e - b));
    ver[e - b] = 0;
    if (upd_parse_version(ver, v)) return 1;
    ver[0] = 0;
    return 0;
}

#endif
