/* Surface materials (materials.c, docs/modding/MATERIALS.md): a material class per texture, by its
   image hash (textures.c ntex_hash_of), from mods/materials.ini; the F2 picker assigns them.
   Main (render) thread only. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int         mat_class_count(void);
const char* mat_class_name(int cls);
/* a class's shininess (Blinn-Phong exponent), strength and whether the highlight takes the
   texture's colour (metals) */
void        mat_class_params(int cls, float* shine, float* strength, int* metal);
int         mat_get(uint32_t hash);              /* its class, 0 none */
void        mat_set(uint32_t hash, int cls);     /* and mods/materials.ini is written */
int         mat_count(void);                     /* textures with a class */
int         mat_active(void);                    /* any entry (cheap; loads the file on first use) */
int         mat_present(void);                   /* any entry, without loading: any thread */

/* the picker (gs_gl.c): a click at x, y in the game's 2D screen space (gs_window_to_screen) asks
   which textured 3D triangle is nearest there in the next executed frame; *hash gets it (0: none)
   once that frame is done, returning 1 once */
void gs_pick_request(float x, float y);
int  gs_pick_result(uint32_t* hash);
void gs_mat_highlight(uint32_t hash);            /* surfaces of this texture pulse (0: none) */

#ifdef __cplusplus
}
#endif
