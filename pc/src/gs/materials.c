/* Surface materials: mods/materials.ini maps a texture's image hash (textures.c) to a class,
   "1a2b3c4d = metal" per line; gs_gl.c gives lit surfaces of that texture the class's highlight
   (fs_src, u_mat). Loaded on first use, written whenever the F2 picker changes one. */
#include "materials.h"
#include "../host/fileio.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const struct { const char* key; const char* name; float shine, strength; int metal; } classes[] = {
    { "none", "None", 0, 0, 0 },
    { "metal", "Metal (rough)", 30, 0.35f, 1 },
    { "metal_polished", "Metal (polished)", 110, 0.9f, 1 },
    { "wood", "Wood (varnished)", 50, 0.18f, 0 },
    { "stone", "Stone / concrete", 10, 0.06f, 0 },
    { "tile", "Tile / ceramic", 120, 0.45f, 0 },
    { "glass", "Glass", 200, 0.9f, 0 },
    { "paint", "Paint / plastic", 45, 0.22f, 0 },
    { "wet", "Wet", 140, 0.8f, 0 },
    { "leather", "Leather", 25, 0.15f, 0 },
};
#define NCLASS ((int)(sizeof(classes) / sizeof(classes[0])))

#define MAP 8192   /* open addressing; a few hundred entries at most */
static struct { uint32_t hash; uint8_t cls; } map[MAP];
static int loaded, count;
static volatile int present;   /* any entry, for the game thread (mat_present) */

int mat_class_count(void) { return NCLASS; }
const char* mat_class_name(int cls) { return cls >= 0 && cls < NCLASS ? classes[cls].name : "?"; }
void mat_class_params(int cls, float* shine, float* strength, int* metal)
{
    if (cls < 0 || cls >= NCLASS) cls = 0;
    *shine = classes[cls].shine; *strength = classes[cls].strength; *metal = classes[cls].metal;
}

static const char* path(void)
{
    static char p[1024];
    SDL_snprintf(p, sizeof p, "%s/materials.ini", SDL_getenv("RECVX_MODS") ? SDL_getenv("RECVX_MODS") : "mods");
    return p;
}

static int slot(uint32_t hash)
{
    uint32_t i = (hash * 2654435761u) & (MAP - 1);
    while (map[i].hash && map[i].hash != hash) i = (i + 1) & (MAP - 1);
    return (int)i;
}

static void put(uint32_t hash, int cls)
{
    int i = slot(hash);
    if (!map[i].hash && cls) { if (count >= MAP / 2) return; map[i].hash = hash; count++; }
    if (map[i].hash) map[i].cls = (uint8_t)cls;   /* 0: kept as none (the picker's "None") */
    present = count > 0;
}

static void load(void)
{
    FILE* f;
    char line[256];
    loaded = 1;
    if (!(f = pc_fopen(path(), "r"))) return;
    while (fgets(line, sizeof line, f)) {
        unsigned h;
        char key[64];
        if (line[0] == '#' || sscanf(line, "%x = %63s", &h, key) != 2 || !h) continue;
        for (int c = 0; c < NCLASS; c++) if (!strcmp(key, classes[c].key)) { put(h, c); break; }
    }
    fclose(f);
    SDL_Log("materials: %d textures from %s", count, path());
}

int mat_get(uint32_t hash)
{
    int i;
    if (!loaded) load();
    if (!hash || !count) return 0;
    i = slot(hash);
    return map[i].hash ? map[i].cls : 0;
}

int mat_present(void) { return present; }

int mat_active(void)
{
    if (!loaded) load();
    return count > 0;
}

int mat_count(void)
{
    int n = 0;
    if (!loaded) load();
    for (int i = 0; i < MAP; i++) n += map[i].hash && map[i].cls;
    return n;
}

static int write_all(FILE* f, void* ud)
{
    (void)ud;
    fprintf(f, "# Surface materials: texture image hash = class (docs/modding/MATERIALS.md)\n");
    for (int i = 0; i < MAP; i++)
        if (map[i].hash && map[i].cls) fprintf(f, "%08x = %s\n", map[i].hash, classes[map[i].cls].key);
    return ferror(f) ? -1 : 0;
}

void mat_set(uint32_t hash, int cls)
{
    if (!loaded) load();
    if (!hash || cls < 0 || cls >= NCLASS) return;
    put(hash, cls);
    if (pc_save_file(path(), "w", write_all, NULL)) SDL_Log("materials: cannot write %s", path());
}
