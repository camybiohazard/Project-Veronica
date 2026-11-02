/* Ammo counter (GAMEPLAY option, docs/USER_GUIDE.md): the equipped weapon's magazine over its size
   ("12/15") in the bottom right corner, in the game's font (message.c bhDispFont: the sys->ef_tlist
   glyphs the event timer draws with, code = ASCII - 32). system.c bhSysCallScreenSaver draws it after
   the game's own timer, game thread, in normal play and the scope (pc_play_screen, reload_glue.c),
   also while the player is hit or grabbed; not with no weapon equipped nor for weapons without a
   magazine (knife, the lighter). Unlimited ammo (the weapon's own or the F2 cheat, as bhCheckBullet) shows "--".
   Widescreen: the window's corner, as the stretched vignette (tps_glue.c tps_filter).
   Above it the flashlight's battery (light_glue.c) while it is in use: an outlined battery filled
   to its charge, green, amber below half, red below a fifth, blinking when nearly empty with the
   light on. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/player.h"
#include "../../decomp/include/ps2/veronica/prog/sub1.h"
#include "../../decomp/include/ps2/veronica/prog/item.h"
#include "../../decomp/include/ps2/veronica/prog/message.h"
#include <stdio.h>

int pc_play_screen(int scope);   /* reload_glue.c */
int pc_ammo_hud_on(void);        /* the option (host.c) */
int pc_inf_ammo(void);
float pc_hor_scale(void);
int pc_flashlight_battery(float* level, int* on);   /* light_glue.c */
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z);   /* title_glue.c */
extern unsigned int pc_ot_pretext;   /* ps2_dummy.c */

#define HUD_MARGIN 17.0f   /* from the screen's right and bottom edges (640x480) */
#define HUD_SCALE 0.7f     /* of the game's font (bhFontScaleSet, put back after) */

extern float FontScaleY;   /* message.c, beside FontScaleX */

/* the battery at the bottom right corner (x1, y1) of the icon, its nub included */
static void battery_draw(float x1, float y1)
{
    static unsigned int blink;
    float level, x0, y0, fx;
    int on;
    unsigned int fill;
    if (!pc_flashlight_battery(&level, &on)) return;
    x1 -= 3.0f;   /* the nub */
    x0 = x1 - 26.0f; y0 = y1 - 11.0f;
    fill = level < 0.2f ? 0xFFE03020 : level < 0.5f ? 0xFFE0A020 : 0xFF50C040;
    blink++;
    if (on && level < 0.1f && (blink & 8)) fill = 0x00000000;
    pc_ot_pretext = 1;   /* the outline is drawn first: bloom from the picture before it (ps2_dummy.c) */
    pc_ui_rect(x0, y0, x1, y1, 0xD0D0D0D0, 0.25f);                          /* outline */
    pc_ot_pretext = 0;   /* taken, or the 2D path is not the native one */
    pc_ui_rect(x1, y0 + 3.0f, x1 + 3.0f, y1 - 3.0f, 0xD0D0D0D0, 0.25f);      /* nub */
    pc_ui_rect(x0 + 1.5f, y0 + 1.5f, x1 - 1.5f, y1 - 1.5f, 0xD0101010, 0.26f);
    fx = x0 + 3.0f + (x1 - x0 - 6.0f) * level;
    if (fill && fx > x0 + 3.0f) pc_ui_rect(x0 + 3.0f, y0 + 3.0f, fx, y1 - 3.0f, fill, 0.27f);
}

void hud_draw(void)
{
    const unsigned int* iv = swork.pip;
    char s[24];
    NJS_POINT2 pos;
    float w = 0, sx = FontScaleX, sy = FontScaleY, scr = FontScaleCR;
    int i, item, max, col = 0, m = ply.mode0;

    /* action, damage, grabbed, hit by an enemy (player.c bhCtrPly_mode0): not dying, events */
    if (!sys || !ply.mnwP || !ply.mlwP || (m != 1 && m != 2 && m != 4 && m != 5) || sys->ssv_an > 0 || !pc_play_screen(1))
        return;
    battery_draw(320.0f + 320.0f * pc_hor_scale() - HUD_MARGIN, 480.0f - HUD_MARGIN - 28.0f * HUD_SCALE - 6.0f);
    if (!pc_ammo_hud_on() || !iv || !iv[0])
        return;
    item = (iv[iv[0]] >> 16) & 0xFF;
    /* a weapon (itemdata type 0x1) with a count (0x800): not the knife nor Claire's lighter (0x401, "1/1") */
    if (item >= 156 || (itemdata[item].type & 0x801) != 0x801 || (max = (int)getbulletmax[item][sys->gm_mode]) <= 0) return;
    if (((iv[iv[0]] >> 16) & 0x800) || (ply.at_flg & 0x40000000) || pc_inf_ammo())
        snprintf(s, sizeof s, "--");
    else {
        snprintf(s, sizeof s, "%d/%d", (int)(iv[iv[0]] & 0xFFFF), max);
        col = (iv[iv[0]] & 0xFFFF) ? 0 : 2;   /* white, red with the magazine empty (bhDispFont colours) */
    }
    bhFontScaleSet(HUD_SCALE, HUD_SCALE, scr);
    for (i = 0; s[i]; i++) w += FontScaleX * bhGetFontSize(s[i] - 32);
    pos.x = 320.0f + 320.0f * pc_hor_scale() - HUD_MARGIN - w;
    pos.y = 480.0f - HUD_MARGIN - 28.0f * FontScaleY;
    for (i = 0; s[i]; i++) {
        bhDispFont(&pos, s[i] - 32, col, -0.9f);
        pos.x += FontScaleX * bhGetFontSize(s[i] - 32);
    }
    bhFontScaleSet(sx, sy, scr);
}
