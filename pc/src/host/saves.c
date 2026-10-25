/* The save system (saves.h): native save files or the memory card. */
#include "ps2_platform.h"
#include "saves.h"
#include "settings.h"
#include "savestore.h"
#include <SDL3/SDL.h>
#include <stdio.h>

int save_native(void) { return g_config.save_system == 0; }

const char* save_native_dir(void)
{
    static char dir[1100];
    if (!dir[0]) {
        snprintf(dir, sizeof(dir), "%s/saves", g_config.save_dir);
        if (!SDL_GetPathInfo(dir, NULL)) {   /* the first native start: the memory card's saves come along (copies) */
            int n;
            SDL_CreateDirectory(dir);
            n = savestore_import_card(g_config.save_dir, dir);
            if (n) host_log("saves: native save files in %s; copied %d memory card save%s (card0 keeps them)\n", dir, n, n == 1 ? "" : "s");
            else host_log("saves: native save files in %s\n", dir);
        }
    }
    return dir;
}
