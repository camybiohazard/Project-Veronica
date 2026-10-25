/* update check (pc/src/net/update.h): the URL, the server's answer, the version comparison */
#include "../src/net/update.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int url(const char* u, const char* host, int port, const char* path)
{
    char h[128], p[256];
    int po;
    if (!upd_parse_url(u, h, sizeof(h), &po, p, sizeof(p))) return 0;
    return !strcmp(h, host) && po == port && !strcmp(p, path);
}

static int resp(const char* r, const char* want)   /* want NULL: refused */
{
    char v[32] = "x";
    int ok = upd_parse_response(r, (int)strlen(r), v, sizeof(v));
    return want ? ok && !strcmp(v, want) : !ok;
}

int main(void)
{
    char h[8], p[8];
    int po;
    unsigned n[3];
    /* URL */
    CHECK(url(RECVX_UPDATE_URL, "85.214.181.87", 1979, "/"));
    CHECK(url("http://updates.example.org", "updates.example.org", 80, "/"));
    CHECK(url("http://a-b.example:8080/v/latest.txt", "a-b.example", 8080, "/v/latest.txt"));
    CHECK(!url("https://example.org/", "", 0, ""));
    CHECK(!url("ftp://example.org/", "", 0, ""));
    CHECK(!url("http://", "", 0, ""));
    CHECK(!url("http://:80/", "", 0, ""));
    CHECK(!url("http://host:0/", "", 0, ""));
    CHECK(!url("http://host:65536/", "", 0, ""));
    CHECK(!url("http://host:/", "", 0, ""));
    CHECK(!url("http://host:12x/", "", 0, ""));
    CHECK(!url("http://user@host/", "", 0, ""));
    CHECK(!url("http://host/a b", "", 0, ""));
    CHECK(!url("http://host/a\r\nX: y", "", 0, ""));
    CHECK(!upd_parse_url("http://toolonghost/", h, sizeof(h), &po, p, sizeof(p)));
    CHECK(!upd_parse_url("http://h/longer-path", h, sizeof(h), &po, p, sizeof(p)));
    /* versions */
    CHECK(upd_parse_version("v0.6.9", n) && n[0] == 0 && n[1] == 6 && n[2] == 9);
    CHECK(upd_parse_version("v10.20.300", n) && n[0] == 10 && n[1] == 20 && n[2] == 300);
    CHECK(upd_parse_version("v0.7.0-rc1", n) && n[1] == 7);
    CHECK(upd_parse_version("v1.0.0+build.5", n));
    CHECK(!upd_parse_version("0.6.9", n));
    CHECK(!upd_parse_version("v0.6", n));
    CHECK(!upd_parse_version("v0.6.", n));
    CHECK(!upd_parse_version("v0..9", n));
    CHECK(!upd_parse_version("v0.6.9.1", n));
    CHECK(!upd_parse_version("v0.6.9 beta", n));
    CHECK(!upd_parse_version("v0.6.9-<script>", n));
    CHECK(!upd_parse_version("v0.6.9-abcdefghijklmnopq", n));   /* suffix too long */
    CHECK(!upd_parse_version("v1234567890.0.0", n));
    CHECK(!upd_parse_version("", n));
    /* comparison */
    CHECK(upd_newer("v0.7.0", "v0.6.9"));
    CHECK(upd_newer("v0.6.10", "v0.6.9"));
    CHECK(upd_newer("v1.0.0", "v0.99.99"));
    CHECK(!upd_newer("v0.6.9", "v0.6.9"));
    CHECK(!upd_newer("v0.6.9", "v0.6.10"));
    CHECK(!upd_newer("v0.6.8", "v0.6.9"));
    CHECK(!upd_newer("v0.7.0-rc1", "v0.7.0"));
    CHECK(!upd_newer("unknown", "v0.6.9"));
    CHECK(!upd_newer("v0.7.0", "garbage"));
    /* answers */
    CHECK(resp("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nv0.6.9", "v0.6.9"));
    CHECK(resp("HTTP/1.0 200\r\n\r\nv0.6.9\n", "v0.6.9"));
    CHECK(resp("HTTP/1.1 200 OK\nServer: x\n\n  v0.7.0-rc1 \r\n\r\n", "v0.7.0-rc1"));
    CHECK(resp("HTTP/1.0 404 Not Found\r\n\r\nv0.6.9", NULL));
    CHECK(resp("HTTP/1.0 2000 OK\r\n\r\nv0.6.9", NULL));
    CHECK(resp("HTTP/1.0 301 Moved\r\nLocation: https://x/\r\n\r\n", NULL));
    CHECK(resp("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body>v0.6.9</body></html>", NULL));
    CHECK(resp("HTTP/1.0 200 OK\r\n\r\n", NULL));
    CHECK(resp("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n", NULL));   /* cut before the body */
    CHECK(resp("HTTP/1.0 200 OK\r\n\r\nv0.6.9\nv0.7.0", NULL));
    CHECK(resp("SSH-2.0-OpenSSH_9.6\r\n", NULL));
    CHECK(resp("v0.6.9", NULL));
    CHECK(resp("", NULL));
    {
        static const char nul[] = "HTTP/1.0 200 OK\r\n\r\nv0.6\0.9";
        char v[32];
        CHECK(!upd_parse_response(nul, (int)sizeof(nul) - 1, v, sizeof(v)));
    }
    {
        char big[5000], v[8];
        memset(big, 'v', sizeof(big));
        memcpy(big, "HTTP/1.0 200 OK\r\n\r\n", 19);
        CHECK(!upd_parse_response(big, (int)sizeof(big), v, sizeof(v)));
        CHECK(!upd_parse_response("HTTP/1.0 200 OK\r\n\r\nv10.20.30", 28, v, sizeof(v)));   /* longer than ver's room */
    }
    if (fails) { printf("%d failed\n", fails); return 1; }
    printf("update: all passed\n");
    return 0;
}
