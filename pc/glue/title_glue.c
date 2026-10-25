/* The title menu's PC additions (adv.c Adv_BioCvTitle): BATTLE GAME by setting, the mouse on its
 * plates, and the text and shape helpers the ENHANCED SETTINGS pages of the OPTION menu draw with
 * (pc/glue/options_glue.c): the game's font (sys->ef_tlist, ASCII 32..126 as codes 0..94).
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/adv.h"
#include "../../decomp/include/ps2/veronica/prog/padman.h"
#include "../../decomp/include/ps2/veronica/prog/message.h"
#include "../../decomp/include/ps2/veronica/prog/sdfunc.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaDraw.h"
#include "../src/host/menu_mouse.h"
#include <string.h>

void bhDispFontEx(NJS_POINT2* pos, int code, unsigned int argb, float pri);
extern ADV_WORK AdvWork;
extern float FontScaleY;

enum { BTN_START = 0x800 };             /* Pad[] bits */

/* ---------------------------------------------------------------- text */
#define PRI_TEXT -1.0f

static int code_of(char c) { return c > 32 && c < 127 ? c - 32 : 0; }
static float adv(int c) { return c ? FontSz[c] : 11.0f; }   /* the font's space is a full cell */

#define TRACK 0.88f                     /* a little tighter than the game's messages */

static float text_w(const char* s, float sc)
{
    float w = 0;
    for (; *s; s++) w += adv(code_of(*s)) * sc * TRACK;
    return w;
}

/* the game's font, left-aligned at x (bhDispMessage's advance), with a drop shadow */
static int font_ok;                     /* the game's font is loaded (sys->ss_flg 0x40) */

static void text(float x, float y, const char* s, float sc, unsigned int argb)
{
    float kx = FontScaleX, ky = FontScaleY, kc = FontScaleCR, x0 = x;
    const char* p;
    int pass;
    if (!font_ok) return;
    bhFontScaleSet(sc, sc, sc);
    for (pass = 0; pass < 2; pass++)
        for (p = s, x = x0; *p; p++) {
            int c = code_of(*p);
            NJS_POINT2 pos = { x + (pass ? 0 : 2), y + (pass ? 0 : 2) };
            if (c) bhDispFontEx(&pos, c, pass ? argb : (argb & 0xFF000000) >> 1 & 0xFF000000, PRI_TEXT);
            x += adv(c) * sc * TRACK;
        }
    bhFontScaleSet(kx, ky, kc);
}

/* at most `w` wide: the words that fit, then the next line (returns the lines drawn) */
static int text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    char line[160];
    int n = 0;
    while (*s && n < max_lines) {
        int len = 0, fit = 0;
        while (s[len] && len < (int)sizeof(line) - 1) {
            memcpy(line, s, len + 1); line[len + 1] = 0;
            if (text_w(line, sc) > w) break;
            len++;
            if (!s[len] || s[len] == ' ') fit = len;
        }
        if (!s[len]) fit = len;
        if (!fit) fit = len ? len : 1;
        memcpy(line, s, fit); line[fit] = 0;
        text(x, y + n * 28 * sc * 1.05f, line, sc, argb);
        n++;
        s += fit;
        while (*s == ' ') s++;
    }
    return n;
}

static void rect(float x0, float y0, float x1, float y1, unsigned int argb, float z)
{
    NJS_POLYGON_VTX v[4] = { { x0, y0, z, argb }, { x0, y1, z, argb }, { x1, y0, z, argb }, { x1, y1, z, argb } };
    njDrawPolygon(v, 4, 1);
}

/* a small arrow head pointing left (dir -1) or right (1), its tip at x */
static void arrow(float x, float y, int dir, unsigned int argb)
{
    NJS_POLYGON_VTX v[3] = { { x, y, 0.3f, argb }, { x - dir * 7.0f, y - 6.0f, 0.3f, argb }, { x - dir * 7.0f, y + 6.0f, 0.3f, argb } };
    njDrawPolygon(v, 3, 1);
}

/* ---------------------------------------------------------------- title menu */
void pc_title_menu_init(void)
{
    ADV_WORK* ap = &AdvWork;
    if (pc_battle_unlocked() && !ap->ExtraFlag) {   /* setting: BATTLE GAME as after a cleared game; the system data's clear flag (ssd_flg 1) stays as it is (its characters: event.c bhCk) */
        ap->ExtraFlag = 1;
        ap->CursorMax[0] = 4;
        ap->ModeCommandId[0] = 0; ap->ModeCommandId[1] = 1; ap->ModeCommandId[2] = 3; ap->ModeCommandId[3] = 2;
    }
}

/* ---------------------------------------------------------------- mouse */
static MouseTake mt;                    /* the pointer (game 2D px) and what it did */

void pc_title_mouse_active(int on)
{
    g_menu_mouse.active = on;
}

void menu_mouse_take(MouseTake* t)
{
    int x = g_menu_mouse.x, y = g_menu_mouse.y;
    t->left = __atomic_exchange_n(&g_menu_mouse.left, 0, __ATOMIC_RELAXED);
    t->right = __atomic_exchange_n(&g_menu_mouse.right, 0, __ATOMIC_RELAXED);
    t->wheel = __atomic_exchange_n(&g_menu_mouse.wheel, 0, __ATOMIC_RELAXED);
    __atomic_exchange_n(&g_menu_mouse.left_up, 0, __ATOMIC_RELAXED);
    t->moved = (x != t->px || y != t->py) && t->seen;
    t->px = x; t->py = y; t->seen = 1;
    t->x = x / 16.0f; t->y = y / 16.0f;
    if (!menu_mouse_on()) t->left = t->right = t->wheel = t->moved = 0;
}

/* CheckButton (before it reads the pad): hovering a row moves the cursor to it, a click on it
   confirms (as Start), a right click cancels. Level 0 the game modes, 1 the difficulty, 2 the
   extra games. */
void pc_title_mouse(int level, int flag)
{
    ADV_WORK* ap = &AdvWork;
    int hit = -1, i, n = ap->CursorMax[level];
    menu_mouse_take(&mt);
    for (i = 0; i < n; i++) {
        float x0, y0, w;
        if (level == 0) { x0 = 140; w = 360; y0 = (ap->ExtraFlag ? 272.0f : 288.0f) + 32.0f * i; }   /* DisplayGameModePlate */
        else if (level == 1) { x0 = 224; w = 193; y0 = 304.0f + 32.0f * i; }   /* DisplayNewGamePlate, DisplayExtraGamePlate */
        else { x0 = 64.0f + 256.0f * i; w = 256; y0 = 352; }
        if (mt.x >= x0 && mt.x < x0 + w && mt.y >= y0 && mt.y < y0 + 32) hit = i;
    }
    if (hit >= 0 && (!ap->CursorFlag[level][hit] || (level == 0 && hit == 0 && !flag))) hit = -1;   /* not selectable now */
    if (hit >= 0 && (mt.moved || mt.left) && hit != ap->Cursor[level]) {
        ap->Cursor[level] = hit;
        CallSystemSe(0, 2);
    }
    if (mt.left && hit >= 0) Pad[ap->PortId].press |= BTN_START;
    else if (mt.right) Pad[ap->PortId].press |= AdvGetCancelButton();
}

/* drawing helpers for the ENHANCED SETTINGS pages of the OPTION menu (pc/glue/options_glue.c) */
void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb) { font_ok = (sys->ss_flg & 0x40) != 0; text(x, y, s, sc, argb); }
float pc_ui_text_w(const char* s, float sc) { return text_w(s, sc); }
int pc_ui_text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    font_ok = (sys->ss_flg & 0x40) != 0;
    return text_wrap(x, y, w, s, sc, argb, max_lines);
}
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z) { rect(x0, y0, x1, y1, argb, z); }
void pc_ui_arrow(float x, float y, int dir, unsigned int argb) { arrow(x, y, dir, argb); }
