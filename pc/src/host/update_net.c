/* The update check's request (update.c), apart from the game's headers (ps2_platform.h and
   windows.h both declare CreateThread): GitHub's latest release of the project as JSON. Windows:
   WinHTTP; Linux: the system's curl (a 32-bit libcurl is rarely installed), 8 s at most. */
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#define UPDATE_HOST "api.github.com"
#define UPDATE_PATH "/repos/camybiohazard/Project-Veronica/releases/latest"   /* drafts and pre-releases left out */

int update_fetch(char* buf, int n);

/* the answer's body into buf (n bytes at most); its length, 0 on any failure */
#ifdef _WIN32
int update_fetch(char* buf, int n)
{
    HINTERNET s = NULL, c = NULL, r = NULL;
    DWORD status = 0, sz = sizeof(status), got;
    int len = 0;
    s = WinHttpOpen(L"cvx-port", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) goto done;
    WinHttpSetTimeouts(s, 5000, 5000, 5000, 8000);
    if (!(c = WinHttpConnect(s, L"" UPDATE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0))) goto done;
    if (!(r = WinHttpOpenRequest(c, L"GET", L"" UPDATE_PATH, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE))) goto done;
    if (!WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(r, NULL)) goto done;
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX) ||
        status != 200) goto done;
    while (len < n && WinHttpReadData(r, buf + len, (DWORD)(n - len), &got) && got) len += (int)got;
done:
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return status == 200 ? len : 0;
}
#else
int update_fetch(char* buf, int n)
{
    FILE* p = popen("curl -fsSL --max-time 8 -A cvx-port -H 'Accept: application/vnd.github+json' "
                    "https://" UPDATE_HOST UPDATE_PATH " 2>/dev/null", "r");
    int len = 0;
    size_t got;
    if (!p) return 0;
    while (len < n && (got = fread(buf + len, 1, (size_t)(n - len), p)) > 0) len += (int)got;
    return pclose(p) == 0 ? len : 0;
}
#endif
