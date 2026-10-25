/*
 * HD texture packs made for PCSX2, loaded as they are (docs/HD_PACK.md).
 *
 * PCSX2 names a replacement after the texture as the GS samples it
 * (GSTextureCache::HashCacheKey::Create, GSTextureReplacements.cpp):
 *   <TEX0Hash>-<CLUTHash>-<bits>.dds   (paletted)   <TEX0Hash>-<bits>.dds   (direct colour)
 * - TEX0Hash: XXH3-64 over the 2^TW x 2^TH texels at TBP0/TBW/PSM. For PSMCT32, PSMT8 and PSMT4
 *   textures at least one GS block in size, the raw 256-byte GS memory blocks in block-row order
 *   (swizzled as in GS memory); otherwise the texels expanded row by row: one byte per index
 *   for paletted formats, RGBA32 with TEXA applied for direct ones.
 * - CLUTHash: XXH3-64 over the palette as 16/256 RGBA32 entries (CSA applied).
 * - bits: PSM | TW << 6 | TH << 10, and for CT24/CT16(S) TEXA's TA0 << 15 | AEM << 23 | TA1 << 24.
 * The game draws the same TEX0s from the same GS memory contents as on the PS2, so the key is
 * computed here exactly when a texture is decoded (native textures and GS memory decodes alike)
 * and the replacement is bound in its place, sampled over the same normalized UV space.
 *
 * A replacement is used only if it depicts the original (unless hd_textures_ui / _font say
 * otherwise): the first time, it is compared with the original on a sample of texels
 * (content_check). Packs made on other disc versions (translations) or with redesigned menus
 * differ in text and layout, and the port's own menu entries are cut from the original menu
 * textures. The check also finds the replacement's alpha range (GS 0..128 as PCSX2 dumps it, or
 * 0..255). Verdicts are kept in <save>/hd_textures.txt, so a file is checked once.
 *
 * The pack folder is indexed once by file name; files are loaded when first drawn and kept up to a
 * memory budget (least recently used ones are dropped and reloaded when needed again). Loader
 * threads read and check them; a draw waits for that only under a screen fade (see "loading").
 */
#include "gs.h"
#include "hdpack.h"
#include "textures.h"
#include "../render/gl_loader.h"
#include "../ps2_platform.h"
#include "../../include/pc_parse.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define XXH_INLINE_ALL
#include <xxhash.h>
#define BCDEC_IMPLEMENTATION
#define BCDEC_STATIC
#include <bcdec.h>

enum { V_UNKNOWN, V_OK, V_REJECTED, V_BROKEN };   /* shows the original / does not / is (nearly) all transparent there */
enum { A_RAW, A_SCALED, A_CONST };   /* replacement alpha: GS units, 0..255, the original's constant */
typedef struct { uint64_t tex, clut; uint32_t bits; } Key;
enum { J_NONE, J_QUEUED, J_RUNNING, J_DONE };   /* Entry.job (under qm) */
typedef struct {
    Key k; uint32_t name;
    int st;                 /* (render thread) 0 not loaded, 1 being read, 2 uploaded (gl), 3 not usable (the font / UI setting may allow it), 4 cannot be loaded */
    GLuint gl; uint32_t bytes; uint64_t last;
    uint8_t verdict, amode, aconst;
    uint32_t* orig; int ow, oh;   /* the original's texels, until it is checked */
    char why[64];           /* a rejection, for the log */
    uint8_t job, inq;       /* the loader threads' work on it; in their queue */
    uint8_t flags;          /* the lookup's NTEX_* tags as it was loaded */
    int8_t r;               /* read_check: the check's result, -1 none */
    void* data; size_t n;   /* the file read (DDS bytes, or a PNG as an RGBA surface), until uploaded */
    uint64_t rns, cns;      /* time it took to read and check */
} Entry;
static Entry* ents; static int nents, cents;
static char* names; static size_t nnames, cnames;
static size_t root_len;              /* names are logged and cached relative to the pack folder */
static int state;                    /* 0 not set up, 1 a pack, -1 none */
static uint32_t* map; static size_t cmap;   /* GL name -> entry + 1 | the definition's NTEX_* tags << 28 */
static uint64_t clock_, bytes;
static uint64_t cap_bytes = 1024ull << 20;
static unsigned hits, misses, loads, rejected, fonts;
static uint64_t read_ns, upload_ns, key_ns, check_ns;
static int trace;
static char cache_path[1100];

static int key_cmp(const Key* a, const Key* b)
{
    return a->tex != b->tex ? (a->tex < b->tex ? -1 : 1) : a->clut != b->clut ? (a->clut < b->clut ? -1 : 1)
         : a->bits != b->bits ? (a->bits < b->bits ? -1 : 1) : 0;
}
static const char* rel(const Entry* e) { return names + e->name + root_len; }
/* equal keys (the same texture in several folders): by the path in the pack, '\\' read as '/', so
   folders named 1_..., 2_... are in that order and lookups take the first */
static int path_cmp(const char* a, const char* b)
{
    for (;; a++, b++) {
        unsigned char x = *a == '\\' ? '/' : (unsigned char)*a, y = *b == '\\' ? '/' : (unsigned char)*b;
        if (x != y || !x) return x - y;
    }
}
static int ent_cmp(const void* a, const void* b)
{
    int c = key_cmp(&((const Entry*)a)->k, &((const Entry*)b)->k);
    return c ? c : path_cmp(rel(a), rel(b));
}
static int key_only_cmp(const void* a, const void* b) { return key_cmp(&((const Entry*)a)->k, &((const Entry*)b)->k); }

static int parse_name(const char* f, Key* k)
{
    unsigned long long a, b; unsigned int bits; char dot;
    const char* ext = strrchr(f, '.');
    if (!ext || (SDL_strcasecmp(ext, ".dds") && SDL_strcasecmp(ext, ".png"))) return 0;
    if (SDL_sscanf(f, "%llx-%llx-%x%c", &a, &b, &bits, &dot) == 4 && dot == '.') { k->tex = a; k->clut = b; }
    else if (SDL_sscanf(f, "%llx-%x%c", &a, &bits, &dot) == 3 && dot == '.') { k->tex = a; k->clut = 0; }
    else return 0;   /* region (-r) and mip level (-mipN) names are not used */
    k->bits = bits & ~(1u << 14);   /* bit 14 was TCC in older packs (TextureName::RemoveUnusedBits) */
    return 1;
}

static SDL_EnumerationResult SDLCALL scan_cb(void* ud, const char* dir, const char* f)
{
    char path[2048];
    SDL_PathInfo info;
    Key k;
    int depth = (int)(intptr_t)ud;
    SDL_snprintf(path, sizeof(path), "%s%s", dir, f);
    if (!parse_name(f, &k)) {   /* a folder: the pack's own (SLUS-20184/replacements/...) */
        if (depth < 4 && SDL_strcasecmp(f, "dumps") && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY)
            SDL_EnumerateDirectory(path, scan_cb, (void*)(intptr_t)(depth + 1));   /* PCSX2's dumps are the originals */
        return SDL_ENUM_CONTINUE;
    }
    size_t len = strlen(path) + 1;
    if (nents == cents) { cents = cents * 2 + 1024; ents = realloc(ents, (size_t)cents * sizeof(Entry)); }
    while (nnames + len > cnames) { cnames = cnames * 2 + 65536; names = realloc(names, cnames); }
    memcpy(names + nnames, path, len);
    ents[nents++] = (Entry){ .k = k, .name = (uint32_t)nnames };
    nnames += len;
    return SDL_ENUM_CONTINUE;
}

/* <save>/hd_textures.txt: "<ok|rejected> <alpha mode> <constant alpha> <file size> <file in the pack>" */
static int name_cmp(const void* a, const void* b) { return strcmp(rel(&ents[*(const int*)a]), rel(&ents[*(const int*)b])); }
static void cache_load(void)
{
    FILE* f = pc_fopen(cache_path, "r");
    char line[1200], v[16], path[1100];
    unsigned am, ac;
    unsigned long long sz;
    int* byname, n = 0;
    if (!f) return;
    byname = malloc((size_t)nents * sizeof(int));
    for (int i = 0; i < nents; i++) byname[i] = i;
    qsort(byname, (size_t)nents, sizeof(int), name_cmp);
    while (fgets(line, sizeof(line), f))
        if (SDL_sscanf(line, "%15s %u %u %llu %1099[^\r\n]", v, &am, &ac, &sz, path) == 5) {
            int lo = 0, hi = nents - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2, c = strcmp(path, rel(&ents[byname[mid]]));
                if (!c) {
                    Entry* e = &ents[byname[mid]];
                    SDL_PathInfo info;
                    if (SDL_GetPathInfo(names + e->name, &info) && info.size == sz && am <= A_CONST) {
                        e->verdict = !strcmp(v, "ok") ? V_OK : !strcmp(v, "broken") ? V_BROKEN : V_REJECTED; e->amode = (uint8_t)am; e->aconst = (uint8_t)ac;
                        if (e->verdict != V_OK) { e->st = 3; rejected++; }
                        n++;
                    }
                    break;
                }
                if (c < 0) hi = mid - 1; else lo = mid + 1;
            }
        }
    fclose(f);
    free(byname);
    host_log("hd textures: %d checked replacements from %s\n", n, cache_path);
}

static void loader_start(void);
static void setup(void)
{
    char dir[1100];
    uint64_t t0 = SDL_GetTicks();
    const char* e;
    state = -1;
    trace = SDL_getenv("RECVX_TRACE_HD") != NULL;
    if (!g_config.hd_textures) return;
    if ((e = SDL_getenv("RECVX_HD_TEXTURES"))) SDL_strlcpy(dir, e, sizeof(dir));
    else if (g_config.hd_textures_dir[0]) SDL_strlcpy(dir, g_config.hd_textures_dir, sizeof(dir));
    else SDL_snprintf(dir, sizeof(dir), "%s/textures", SDL_getenv("RECVX_MODS") ? SDL_getenv("RECVX_MODS") : "mods");
    if ((e = SDL_getenv("RECVX_HD_CACHE_MB"))) cap_bytes = (uint64_t)atoi(e) << 20;
    if (!SDL_GetPathInfo(dir, NULL)) return;
    SDL_EnumerateDirectory(dir, scan_cb, (void*)(intptr_t)0);
    if (!nents) { host_log("hd textures: no PCSX2 replacement textures in %s\n", dir); return; }
    root_len = strlen(dir);
    while (names[ents[0].name + root_len] == '/' || names[ents[0].name + root_len] == '\\') root_len++;
    qsort(ents, (size_t)nents, sizeof(Entry), ent_cmp);
    state = 1;
    host_log("hd textures: %d replacements in %s (indexed in %u ms, cache %u MB)\n", nents, dir,
             (unsigned)(SDL_GetTicks() - t0), (unsigned)(cap_bytes >> 20));
    {
        int dup = 0;
        for (int i = 1; i < nents; i++) dup += !key_cmp(&ents[i].k, &ents[i - 1].k) && (i < 2 || key_cmp(&ents[i - 1].k, &ents[i - 2].k));
        if (dup) host_log("hd textures: %d names in several folders, the first folder in name order wins\n", dup);
    }
    SDL_snprintf(cache_path, sizeof(cache_path), "%s/hd_textures.txt", g_config.save_dir);
    cache_load();
    loader_start();
}

int hdpack_active(void)
{
    if (!state) setup();
    return state > 0;
}

/* ---------------- the PCSX2 key ---------------- */

static int pal_entries(uint32_t psm) { return psm == PSMT8 || psm == PSMT8H ? 256 : psm == PSMT4 || psm == PSMT4HL || psm == PSMT4HH ? 16 : 0; }

static uint32_t expand(uint32_t psm, uint32_t v, uint64_t texa)   /* GSLocalMemory ReadTexture24/16 */
{
    uint32_t ta0 = texa & 0xFF, ta1 = (texa >> 32) & 0xFF, aem = (texa >> 15) & 1;
    if (psm == PSMCT32 || psm == PSMZ32) return v;
    if (psm == PSMCT24 || psm == PSMZ24) return (v & 0xFFFFFF) | ((aem && !(v & 0xFFFFFF)) ? 0 : ta0 << 24);
    return ((v & 0x1F) << 3) | (((v >> 5) & 0x1F) << 11) | (((v >> 10) & 0x1F) << 19) |
           ((v & 0x8000) ? ta1 << 24 : (aem && !(v & 0x7FFF)) ? 0 : ta0 << 24);
}

static int make_key(uint64_t tex0, uint64_t texa, Key* k)
{
    uint32_t tbp = tex0 & 0x3FFF, tbw = (tex0 >> 14) & 0x3F, psm = (tex0 >> 20) & 0x3F;
    uint32_t tw = (tex0 >> 26) & 0xF, th = (tex0 >> 30) & 0xF, W, H, bw, bh, n = (uint32_t)pal_entries(psm);
    int full = psm == PSMCT32 || psm == PSMT8 || psm == PSMT4 || psm == PSMZ32;   /* psm.fmsk == 0xFFFFFFFF */
    XXH3_state_t st;
    if (tw > 10 || th > 10) return 0;
    W = 1u << tw; H = 1u << th;
    if (psm == PSMT8) { bw = 16; bh = 16; }
    else if (psm == PSMT4) { bw = 32; bh = 16; }
    else if (psm == PSMCT16 || psm == PSMCT16S || psm == PSMZ16 || psm == PSMZ16S) { bw = 16; bh = 8; }
    else { bw = 8; bh = 8; }
    XXH3_64bits_reset(&st);
    if (full && W >= bw && H >= bh) {   /* the GS memory blocks as they are */
        for (uint32_t y = 0; y < H; y += bh)
            for (uint32_t x = 0; x < W; x += bw) {
                uint32_t a = gs_pixel_addr((int)psm, tbp, tbw, x, y);
                XXH3_64bits_update(&st, gs_mem + ((psm == PSMT4 ? a >> 1 : a) & ~255u), 256);
            }
    } else {   /* expanded: index bytes, or RGBA32 */
        static uint8_t row[1024 * 4];
        for (uint32_t y = 0; y < H; y++) {
            for (uint32_t x = 0; x < W; x++) {
                uint32_t v = gs_read_pixel((int)psm, tbp, tbw, x, y);
                if (n) row[x] = (uint8_t)v;
                else { v = expand(psm, v, texa); memcpy(row + x * 4, &v, 4); }
            }
            XXH3_64bits_update(&st, row, n ? W : W * 4);
        }
    }
    k->tex = XXH3_64bits_digest(&st);
    k->clut = 0;
    if (n) {   /* GSClut::Read32 */
        uint32_t raw[256], pal[256], cpsm = (tex0 >> 51) & 0xF, csa = (tex0 >> 56) & 0x1F;
        if (cpsm == 0 || cpsm == 1) {
            gs_get_palette(tex0 & ~(0x1Full << 56), texa, raw, 256);   /* CSA 0: the buffer as it is */
            for (uint32_t i = 0; i < n; i++)   /* T32 I8: the offset clamps at the last block (ReadCLUT_T32_I8) */
                pal[i] = raw[(n == 256 ? SDL_min((i & 0xF0) + (csa & 15) * 16, 240u) : (csa & 15) * 16) + (i & 15)];
        } else gs_get_palette(tex0, texa, pal, (int)n);
        k->clut = XXH3_64bits(pal, n * 4);
    }
    k->bits = psm | tw << 6 | th << 10;
    if (!n && psm != PSMCT32 && psm != PSMZ32)   /* psm.pal == 0 && psm.fmt > 0 */
        k->bits |= (uint32_t)(texa & 0xFF) << 15 | (uint32_t)((texa >> 15) & 1) << 23 | (uint32_t)((texa >> 32) & 0xFF) << 24;
    return 1;
}


/* one log line 2 s after lookups or loads stopped (after a room is in) */
static uint64_t last_activity;
static int stats_due;
static void stats(void) { stats_due = 1; last_activity = SDL_GetTicks(); }
static void stats_flush(void)
{
    if (!stats_due || SDL_GetTicks() - last_activity < 2000) return;
    stats_due = 0;
    host_log("hd textures: %u hits (%u rejected, %u font kept), %u misses; %u loaded, %u MB kept "
             "(read %u ms, checks %u ms, upload %u ms, keys %u ms)\n",
             hits, rejected, fonts, misses, loads, (unsigned)(bytes >> 20), (unsigned)(read_ns / 1000000),
             (unsigned)(check_ns / 1000000), (unsigned)(upload_ns / 1000000), (unsigned)(key_ns / 1000000));
}

void hdpack_note(unsigned int t, uint64_t tex0, uint64_t texa, int flags)
{
    Key k;
    Entry* e;
    uint64_t t0 = SDL_GetTicksNS();
    uint32_t W = 1u << ((tex0 >> 26) & 0xF), H = 1u << ((tex0 >> 30) & 0xF);
    if (!hdpack_active() || !make_key(tex0, texa, &k)) return;
    key_ns += SDL_GetTicksNS() - t0;
    e = bsearch(&(Entry){ .k = k }, ents, (size_t)nents, sizeof(Entry), key_only_cmp);
    while (e && e > ents && !key_cmp(&e[-1].k, &k)) e--;   /* the first of equal keys */
    if (t >= cmap) {
        size_t c = t + 4096;
        map = realloc(map, c * sizeof(*map));
        memset(map + cmap, 0, (c - cmap) * sizeof(*map));
        cmap = c;
    }
    if (e) hits++; else misses++;
    if (trace) host_log("hd textures: %s %llx-%llx-%x (tex0 %016llx%s)\n", e ? "hit " : "miss", (unsigned long long)k.tex,
                        (unsigned long long)k.clut, k.bits, (unsigned long long)tex0, flags & NTEX_FONT ? ", font" : flags & NTEX_KEEP ? ", port menu letters" : "");
    if (e && (flags & NTEX_FONT) && !g_config.hd_textures_font) { fonts++; e = NULL; }   /* the game's own font */
    map[t] = e ? ((uint32_t)(e - ents) + 1) | (uint32_t)(flags & 3) << 28 : 0;
    if (e && e->st == 0 && e->verdict == V_UNKNOWN && !e->orig && (e->orig = malloc((size_t)W * H * 4))) {
        gs_decode_texture(tex0, texa, W, H, e->orig);   /* for the check (GS memory and CLUT are this texture's now) */
        e->ow = (int)W; e->oh = (int)H;
    }
    {   /* RECVX_HD_DUMP=dir: the originals as PNG, named as PCSX2 would */
        static const char* dd; static int chk;
        if (!chk++) dd = SDL_getenv("RECVX_HD_DUMP");
        if (dd) {
            uint32_t* px = malloc((size_t)W * H * 4);
            char path[1100];
            SDL_Surface* s;
            gs_decode_texture(tex0, texa, W, H, px);
            if (k.clut) SDL_snprintf(path, sizeof(path), "%s/%llx-%llx-%08x.png", dd, (unsigned long long)k.tex, (unsigned long long)k.clut, k.bits);
            else SDL_snprintf(path, sizeof(path), "%s/%llx-%08x.png", dd, (unsigned long long)k.tex, k.bits);
            if ((s = SDL_CreateSurfaceFrom((int)W, (int)H, SDL_PIXELFORMAT_ABGR8888, px, (int)W * 4))) { SDL_SavePNG(s, path); SDL_DestroySurface(s); }
            free(px);
        }
    }
    stats();
}

void hdpack_forget(unsigned int t)
{
    if (t < cmap) map[t] = 0;
}

/* ---------------- DDS ---------------- */

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define GL_BGRA 0x80E1
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_SWIZZLE_A 0x8E45
typedef void (GLAPIENTRY* PFN_CompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);

/* DDS headers: pc_parse.h pc_dds_parse (the size limits, the mipmap chain) */
static GLenum dds_fmt(uint32_t dxgi)
{
    return dxgi == 71 || dxgi == 72 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : dxgi == 74 || dxgi == 75 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
         : dxgi == 77 || dxgi == 78 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_BPTC_UNORM;
}

static GLuint upload_dds(const uint8_t* d, size_t n, uint32_t* out_bytes)
{
    static PFN_CompressedTexImage2D cti;
    PcDds x;
    uint32_t off, total = 0, mips;
    GLuint t;
    if (!pc_dds_parse(d, n, n, &x)) return 0;
    if (x.block && !cti && !(cti = (PFN_CompressedTexImage2D)SDL_GL_GetProcAddress("glCompressedTexImage2D"))) return 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for (off = x.off, mips = 0; mips < x.mips; mips++) {   /* (the levels the file has: level 0 does, pc_dds_parse) */
        uint32_t mw = x.w >> mips ? x.w >> mips : 1, mh = x.h >> mips ? x.h >> mips : 1;
        uint64_t sz = pc_dds_level(&x, mips);
        if (!pc_range_ok(off, sz, n)) break;
        if (x.block) cti(GL_TEXTURE_2D, (GLint)mips, dds_fmt(x.dxgi), (GLsizei)mw, (GLsizei)mh, 0, (GLsizei)sz, d + off);
        else glTexImage2D(GL_TEXTURE_2D, (GLint)mips, GL_RGBA8, (GLsizei)mw, (GLsizei)mh, 0, x.bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, d + off);
        off += (uint32_t)sz; total += (uint32_t)sz;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)mips - 1);
    *out_bytes = total;
    return t;
}

static GLuint upload_rgba(SDL_Surface* c, uint32_t* out_bytes)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, c->pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, c->w, c->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    *out_bytes = (uint32_t)(c->w * c->h * 4);
    return t;
}

/* ---------------- the content check ---------------- */

/* the replacement's pixels: a decoded 4x4 block (DDS) or the PNG surface */
typedef struct { const uint8_t* d; PcDds x; SDL_Surface* png; int w, h; int cbx, cby; uint8_t blk[16 * 4]; } Img;

static const uint8_t* img_px(Img* m, int px, int py)
{
    int bx = px >> 2, by = py >> 2;
    if (m->png) return (const uint8_t*)m->png->pixels + py * m->png->pitch + px * 4;
    if (bx != m->cbx || by != m->cby) {   /* decode the block (one block cached) */
        const PcDds* x = &m->x;
        if (!x->block)
            for (int i = 0; i < 16; i++) {
                uint32_t sx = SDL_min((uint32_t)bx * 4 + (i & 3), x->w - 1), sy = SDL_min((uint32_t)by * 4 + (i >> 2), x->h - 1);
                const uint8_t* p = m->d + x->off + ((size_t)sy * x->w + sx) * 4;
                m->blk[i * 4 + 0] = p[x->bgra ? 2 : 0]; m->blk[i * 4 + 1] = p[1]; m->blk[i * 4 + 2] = p[x->bgra ? 0 : 2]; m->blk[i * 4 + 3] = p[3];
            }
        else {
            const uint8_t* src = m->d + x->off + ((size_t)by * ((x->w + 3) / 4) + bx) * x->block;
            switch (x->dxgi) {
            case 71: case 72: bcdec_bc1(src, m->blk, 16); break;
            case 74: case 75: bcdec_bc2(src, m->blk, 16); break;
            case 77: case 78: bcdec_bc3(src, m->blk, 16); break;
            default: bcdec_bc7(src, m->blk, 16); break;
            }
        }
        m->cbx = bx; m->cby = by;
    }
    return m->blk + ((py & 3) * 4 + (px & 3)) * 4;
}

/* Does the replacement show the original? 1 yes, 0 no (e->why), 2 it is (nearly) transparent where
   the original shows (not even hd_textures_ui uses it), -1 unreadable; sets the alpha
   mode. Compared on a grid of at most 32 x 32 original texels: each against the replacement's
   texels over its area (box filter, colour weighted by alpha + 1 so transparent texels' colour
   does not count where others are visible). Thresholds from a large pack: its upscaled room
   textures differ by 3-21 on average and in at most 7% of the texels by more than 48; translated
   or redesigned UI textures by 19-115 and in 13-76%. */
#define CHECK_GRID 32
static int check_grid(Entry* e, const void* data, size_t n_, int grid, int* borderline)
{
    Img m = { .cbx = -1, .cby = -1 };
    int ok = 1, nvis = 0, nbig = 0, n = 0, amin = 255, amax = 0, nclear = 0;
    int sx = grid ? SDL_max(1, e->ow / grid) : 1, sy = grid ? SDL_max(1, e->oh / grid) : 1;
    double ad[3] = { 0, 0, 0 }, mad = 0;
    if (!SDL_strcasecmp(strrchr(names + e->name, '.'), ".png")) { m.png = (SDL_Surface*)data; m.w = m.png->w; m.h = m.png->h; }
    else { if (!pc_dds_parse(data, n_, n_, &m.x)) return -1; m.d = data; m.w = (int)m.x.w; m.h = (int)m.x.h; }
    for (int i = 0; i < e->ow * e->oh; i++) { int a = (int)(e->orig[i] >> 24); amin = SDL_min(amin, a); amax = SDL_max(amax, a); }
    for (int y = sy / 2; y < e->oh; y += sy)
        for (int x = sx / 2; x < e->ow; x += sx) {
            int x0 = x * m.w / e->ow, x1 = SDL_max(x0 + 1, (x + 1) * m.w / e->ow), y0 = y * m.h / e->oh, y1 = SDL_max(y0 + 1, (y + 1) * m.h / e->oh);
            double s[3] = { 0, 0, 0 }, wsum = 0, asum = 0, r, g, bl, a, d;
            uint32_t o = e->orig[y * e->ow + x];
            int ao = (int)(o >> 24);
            for (int by = y0 >> 2; by <= (y1 - 1) >> 2; by++)   /* the texel's area, block by block (one decode each) */
                for (int bx = x0 >> 2; bx <= (x1 - 1) >> 2; bx++)
                    for (int py = SDL_max(y0, by * 4); py < SDL_min(y1, by * 4 + 4); py++)
                        for (int px = SDL_max(x0, bx * 4); px < SDL_min(x1, bx * 4 + 4); px++) {
                            const uint8_t* p = img_px(&m, px, py);
                            double w = p[3] + 1.0;
                            s[0] += p[0] * w; s[1] += p[1] * w; s[2] += p[2] * w; wsum += w; asum += p[3];
                        }
            r = s[0] / wsum; g = s[1] / wsum; bl = s[2] / wsum; a = asum / ((x1 - x0) * (y1 - y0));
            n++;
            ad[A_RAW] += SDL_fabs(a - ao); ad[A_SCALED] += SDL_fabs(a * 128.0 / 255.0 - ao); ad[A_CONST] += SDL_fabs((double)amin - ao);
            if (ao == 0 && amax > 0) continue;   /* invisible in the original */
            nclear += a < 8;   /* the replacement is transparent where the original shows */
            d = (SDL_fabs(r - (o & 0xFF)) + SDL_fabs(g - ((o >> 8) & 0xFF)) + SDL_fabs(bl - ((o >> 16) & 0xFF))) / 3;
            mad += d; nvis++; nbig += d > 48;
        }
    e->amode = ad[A_SCALED] < ad[A_RAW] ? A_SCALED : A_RAW;
    if (amin == amax && ad[A_CONST] < ad[e->amode]) e->amode = A_CONST;   /* (the original's alpha is constant) */
    e->aconst = (uint8_t)amin;
    if (nvis) mad /= nvis;
    if (ad[e->amode] / n > 32) { ok = 0; SDL_snprintf(e->why, sizeof(e->why), "alpha differs by %.0f", ad[e->amode] / n); }
    else if (mad > 24 || nbig > nvis * 8 / 100) { ok = 0; SDL_snprintf(e->why, sizeof(e->why), "colour differs by %.0f, %d%% of texels", mad, nvis ? nbig * 100 / nvis : 0); }
    if (!ok && nvis && nclear * 10 > nvis * 9) { ok = 2; SDL_snprintf(e->why, sizeof(e->why), "transparent where the original shows"); }
    /* near a threshold: a sample's estimate is not enough */
    *borderline = ad[e->amode] / n > 24 && ad[e->amode] / n < 40 ? 1 : mad > 18 && mad < 30 ? 1 : nvis && nbig * 100 > nvis * 5 && nbig * 100 < nvis * 12;
    if (trace) host_log("hd textures: check %s: %s (colour %.1f, %.3f differ; alpha %.1f, mode %d; %d texels)\n", rel(e), ok == 1 ? "ok" : ok ? "broken" : "rejected",
                        mad, nvis ? (double)nbig / nvis : 0.0, ad[e->amode] / n, e->amode, n);
    return ok;
}

/* a sample of the texels; all of them when the sample is near a threshold */
static int content_check(Entry* e, const void* data, size_t n)
{
    int borderline, r = check_grid(e, data, n, CHECK_GRID, &borderline);
    return r >= 0 && borderline && (e->ow > CHECK_GRID || e->oh > CHECK_GRID) ? check_grid(e, data, n, 0, &borderline) : r;
}

/* ---------------- loading ---------------- */

/* A replacement is loaded when its texture is first drawn. Reading and checking it (a few ms per
   file from the page cache, 100+ ms for a burst from a cold disk) is done by loader threads
   while the draws keep the original texture; the render thread only uploads what is ready, a
   few ms of it per frame. Under a screen fade that lets the picture through (the fade-in of a
   room) a draw waits for its file instead, so a room shows its replacements from its first
   visible frame; under a black screen the files are read in the meantime. Textures first drawn
   later (another camera cut, an enemy, an effect) show the original until their file is in. */
#define UPLOAD_BUDGET_NS 3000000   /* render-thread time for finishing loads per executed frame */
#define READY_MAX (128u << 20)     /* files read but not uploaded yet and being read (a 32-bit process; one larger file is read alone) */
#define LOADERS 3                  /* loader threads: reads overlap, checks run in parallel */
static SDL_Mutex* qm;
static SDL_Condition* qc;          /* a job done, queued, or ready bytes freed */
static int* queue_; static int qh, qn;
static uint64_t ready;             /* bytes read, not uploaded */
static uint64_t running;           /* bytes the loader threads reserved for the files they read (need_bytes) */
static int fade;                   /* hdpack_frame: 1 the picture shows through a fade */
static uint64_t spent;             /* this frame's time in finish */
static int nloading;               /* entries in st 1 */

/* may this lookup use the file? Ones that differ from the original only with hd_textures_font
   (font pages) or hd_textures_ui (anything else but the textures the port cuts its own menu
   entries from) */
static int usable(const Entry* e, uint32_t flags)
{
    if (e->verdict == V_OK) return 1;
    if (e->verdict != V_REJECTED) return 0;
    return flags & NTEX_FONT ? g_config.hd_textures_font : g_config.hd_textures_ui && !(flags & NTEX_KEEP);
}

/* the memory loading the file takes (the DDS file read; a PNG's file, its decoded surface and the RGBA
   copy), from its size and header, before it is read; 0: not usable (not a DDS of the formats used or
   a PNG, larger than PC_HD_MAX_DIM a side or PC_HD_MAX_BYTES, a DDS with more mipmap levels than its
   size has) */
static uint64_t need_bytes(const Entry* e)
{
    const char* path = names + e->name;
    uint8_t h[148];
    size_t got;
    Sint64 size;
    SDL_IOStream* f = SDL_IOFromFile(path, "rb");
    if (!f) return 0;
    size = SDL_GetIOSize(f);
    got = SDL_ReadIO(f, h, sizeof(h));
    SDL_CloseIO(f);
    if (size <= 0) return 0;
    if (!SDL_strcasecmp(strrchr(path, '.'), ".png")) {
        uint32_t w, hh;
        uint64_t px;
        if (!pc_png_size(h, got, &w, &hh) || !w || !hh || w > PC_HD_MAX_DIM || hh > PC_HD_MAX_DIM || (px = (uint64_t)w * hh * 4) > PC_HD_MAX_BYTES) return 0;
        return (uint64_t)size + 2 * px;
    } else {
        PcDds x;
        return (uint64_t)size <= PC_HD_MAX_BYTES && pc_dds_parse(h, got, (uint64_t)size, &x) ? (uint64_t)size : 0;
    }
}

/* read the file and check it the first time (a loader thread, or the render thread when it
   waits); touches only the entry. need: need_bytes (0: not read) */
static void read_check(Entry* e, uint64_t need)
{
    const char* path = names + e->name;
    uint64_t t0 = SDL_GetTicksNS(), t1;
    e->n = 0; e->r = -1; e->data = NULL; e->why[0] = 0;
    if (!need) SDL_strlcpy(e->why, "too large, or not a DDS / PNG of the formats used", sizeof(e->why));
    else if (!SDL_strcasecmp(strrchr(path, '.'), ".png")) {
        SDL_Surface* s = SDL_LoadPNG(path);
        e->data = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888) : NULL;   /* RGBA bytes */
        SDL_DestroySurface(s);
        if (e->data) e->n = (size_t)((SDL_Surface*)e->data)->pitch * ((SDL_Surface*)e->data)->h;
    } else e->data = SDL_LoadFile(path, &e->n);
    t1 = SDL_GetTicksNS();
    if (e->data && e->verdict == V_UNKNOWN && e->orig) e->r = (int8_t)content_check(e, e->data, e->n);
    e->rns = t1 - t0; e->cns = SDL_GetTicksNS() - t1;
}

static int SDLCALL loader(void* unused)
{
    (void)unused;
    crash_thread_init();
    SDL_LockMutex(qm);
    for (;;) {
        Entry* e;
        uint64_t need;
        while (!qn) SDL_WaitCondition(qc, qm);
        e = &ents[queue_[qh]]; qh = (qh + 1) % nents; qn--;
        e->inq = 0;
        if (e->job != J_QUEUED) continue;   /* the render thread took it over */
        SDL_UnlockMutex(qm);
        need = need_bytes(e);
        SDL_LockMutex(qm);
        /* its memory reserved before it starts, within READY_MAX with the files read but not uploaded
           and the other jobs' (one alone always runs); until then the render thread may take it over */
        while (e->job == J_QUEUED && (ready || running) && ready + running + need > READY_MAX) SDL_WaitCondition(qc, qm);
        if (e->job != J_QUEUED) continue;
        e->job = J_RUNNING;
        running += need;
        SDL_UnlockMutex(qm);
        read_check(e, need);
        SDL_LockMutex(qm);
        running -= need;
        e->job = J_DONE;
        ready += e->n;
        SDL_BroadcastCondition(qc);
    }
    return 0;
}

static void loader_start(void)
{
    int n = 0;
    if ((queue_ = malloc((size_t)nents * sizeof(int))) && (qm = SDL_CreateMutex()) && (qc = SDL_CreateCondition()))
        for (int i = 0; i < LOADERS; i++) {
            SDL_Thread* th = SDL_CreateThread(loader, "hdpack", NULL);
            if (th) { SDL_DetachThread(th); n++; }
        }
    if (!n) qm = NULL;   /* loaded on the render thread */
}

/* the render thread's part: the verdict, the upload, the memory budget */
static void finish(Entry* e)
{
    uint32_t flags = e->flags;
    const char* path = names + e->name;
    int png = !SDL_strcasecmp(strrchr(path, '.'), ".png");
    uint64_t t0 = SDL_GetTicksNS();
    if (e->r >= 0) {
        SDL_PathInfo info;
        FILE* f;
        e->verdict = e->r == 1 ? V_OK : e->r ? V_BROKEN : V_REJECTED;
        if (SDL_GetPathInfo(path, &info) && (f = pc_fopen(cache_path, "a"))) {
            char line[1200];   /* (SDL's printf: the 64-bit size also with msvcrt) */
            SDL_snprintf(line, sizeof(line), "%s %u %u %llu %s\n", e->r == 1 ? "ok" : e->r ? "broken" : "rejected", e->amode, e->aconst, (unsigned long long)info.size, rel(e));
            fputs(line, f);
            fclose(f);
        }
        if (e->r != 1) { host_log("hd textures: %s does not show the original (%s)%s\n", rel(e), e->why, usable(e, flags) ? "" : ", not used"); rejected++; }
    }
    free(e->orig); e->orig = NULL;
    e->gl = !e->data || !usable(e, flags) ? 0 : png ? upload_rgba(e->data, &e->bytes) : upload_dds(e->data, e->n, &e->bytes);
    if (png) SDL_DestroySurface(e->data); else SDL_free(e->data);
    if (!e->gl) {   /* unreadable or not uploadable: never tried again (bind would on every draw) */
        e->st = e->data && e->verdict != V_UNKNOWN && !usable(e, flags) ? 3 : 4;
        if (e->st == 4) host_log("hd textures: cannot load %s%s%s\n", rel(e), e->why[0] ? ": " : "", e->why);
    } else {
        if (e->amode == A_CONST) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
        e->st = 2;
        loads++;
        bytes += e->bytes;
    }
    e->data = NULL;
    while (bytes > cap_bytes) {   /* least recently used */
        Entry* o = NULL;
        for (int i = 0; i < nents; i++) if (ents[i].st == 2 && &ents[i] != e && (!o || ents[i].last < o->last)) o = &ents[i];
        if (!o) break;
        gs_tex_delete(o->gl);   /* this frame's replays may still use it */
        bytes -= o->bytes;
        o->gl = 0; o->st = 0;
    }
    read_ns += e->rns; check_ns += e->cns; upload_ns += SDL_GetTicksNS() - t0;
    spent += SDL_GetTicksNS() - t0;
    nloading--;
    stats();
}

/* st 0 -> 1: to the loader threads (without them, at once) */
static void start(Entry* e, uint32_t flags)
{
    e->st = 1; e->flags = (uint8_t)flags;
    nloading++;
    if (!qm) { read_check(e, need_bytes(e)); finish(e); return; }
    SDL_LockMutex(qm);
    e->job = J_QUEUED;
    if (!e->inq) { e->inq = 1; queue_[(qh + qn++) % nents] = (int)(e - ents); }
    SDL_BroadcastCondition(qc);
    SDL_UnlockMutex(qm);
}

/* st 1: finished if read; wait: read now if no loader thread has begun it, else wait for it */
static void poll(Entry* e, int wait)
{
    int job;
    SDL_LockMutex(qm);
    if (wait) {
        if (e->job == J_QUEUED) e->job = J_NONE;
        while (e->job == J_RUNNING) SDL_WaitCondition(qc, qm);
    }
    job = e->job;
    if (job == J_DONE) { ready -= e->n; e->job = J_NONE; SDL_BroadcastCondition(qc); }
    SDL_UnlockMutex(qm);
    if (job == J_NONE) read_check(e, need_bytes(e));
    if (job == J_NONE || job == J_DONE) finish(e);
}

/* a game frame is executed: what the loader threads have read is uploaded, a few ms of it */
void hdpack_frame(int faded)
{
    fade = faded;
    spent = 0;
    for (int i = 0; qm && nloading && i < nents && spent < UPLOAD_BUDGET_NS; i++)
        if (ents[i].st == 1) poll(&ents[i], 0);
}

unsigned int hdpack_bind(unsigned int t, float* ascale)
{
    Entry* e;
    if (state <= 0) return t;
    stats_flush();
    if (t >= cmap || !map[t]) return t;
    e = &ents[(map[t] & 0x0FFFFFFF) - 1];
    if (e->st == 3 && usable(e, map[t] >> 28)) e->st = 0;   /* not used so far, now asked for (font / UI setting) */
    if (e->st == 0) start(e, map[t] >> 28);
    if (e->st == 1 && fade) poll(e, 1);
    if (e->st != 2 || !usable(e, map[t] >> 28)) return t;
    e->last = ++clock_;
    /* GS alpha (0x80 = 1.0) as the original: the replacement's 0..255 scaled, or the original's constant */
    *ascale = e->amode == A_RAW ? 255.0f : e->amode == A_SCALED ? 128.0f : (float)e->aconst;
    return e->gl;
}
