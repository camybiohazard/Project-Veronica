/* BioRand packs in game (pc/src/host/biorand.h, docs/USER_GUIDE.md "Randomizer (BioRand)"). Its room files carry
   the randomized items the engine reads (docs/modding/ROOM_FORMAT.md "Enemies, objects, items"): an item record's
   `type` is the item the pickup gives (its `id` stays the model), `wrk_no` its amount (decomp hitchk.c, sub1.c
   GetItem); the start inventory is a block of room 0/0's entry script under "ev_flg 1 clear" (C7 145 -> X,
   C7 0 -> X, C7 0xFF X n for the STANDARD slot, C9 counts; event.c). Here: rifle ammo stacking, the save words,
   the load screen's question and a co-op guest's start inventory. Game thread. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/item.h"
#include "../src/host/biorand.h"
#include "../src/host/savestore.h"
#include "../include/pc_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);

/* "Rifle ammo stacks" (GAMEPLAY), always while BioRand is on (biorand_rifle_stack): the stack flag of the rifle and
   assault rifle bullets (item.c itemdata 40, 42) */
void biorand_frame(void)
{
    unsigned short on = biorand_rifle_stack() ? 0x200 : 0;
    itemdata[40].type = (itemdata[40].type & ~0x200) | on;
    itemdata[42].type = (itemdata[42].type & ~0x200) | on;
}

/* ---------------------------------------------------------------- the save
   Two save-region words no game code uses (AllItemInit clears them; the item boxes end at itm[255], co-op's words
   are 256..287 and 320..359): BR_W 'B' << 24 | seed (0xFFFFF unknown) while BioRand is on, else 0; BR_W + 1 the
   pack's id. Older builds keep them as they are; their saves have 0 (no BioRand). */
#define BR_W 288
#define BR_ON 0x42000000u
#define BR_OFFSET ((int)((char*)&((SYS_WORK*)0)->itm[BR_W] - (char*)&((SYS_WORK*)0)->version))

static void words(unsigned int* w)
{
    int on = biorand_active(), seed = biorand_seed();
    w[0] = on ? BR_ON | (seed >= 0 ? (unsigned int)seed : 0xFFFFFu) : 0;
    w[1] = on ? biorand_id() : 0;
}

/* the game state: the current choice (a new game: coop_glue.c pc_quick_start; a co-op guest's start) */
void pc_biorand_stamp(void)
{
    if (sys) words(&sys->itm[BR_W]);
}

/* ps2_LoadScreen.c: the save just read (card bytes from sys->version) was made with another BioRand state: 1, the
   question's lines */
static char ask[3][64];
int pc_biorand_save_differs(const void* save)
{
    unsigned int w[2], now[2];
    int seed, cur = biorand_seed();
    if (!biorand_shown()) return 0;   /* (BioRand only with --dev) */
    memcpy(w, (const char*)save + BR_OFFSET, sizeof(w));
    if ((w[0] & 0xFF000000u) != BR_ON) w[0] = w[1] = 0;
    words(now);
    if (w[0] == now[0] && w[1] == now[1]) return 0;
    seed = (int)(w[0] & 0xFFFFF);
    if (!w[0]) snprintf(ask[0], sizeof(ask[0]), "This save was made without BioRand;");
    else if (seed == 0xFFFFF) snprintf(ask[0], sizeof(ask[0]), "This save was made with another BioRand pack;");
    else snprintf(ask[0], sizeof(ask[0]), "This save was made with BioRand seed %d;", seed);
    if (!now[0]) snprintf(ask[1], sizeof(ask[1]), "BioRand is off.");
    else if (w[0] && seed == cur) snprintf(ask[1], sizeof(ask[1]), "the BioRand folder has other options of it.");
    else if (cur >= 0) snprintf(ask[1], sizeof(ask[1]), "the BioRand folder has seed %d.", cur);
    else snprintf(ask[1], sizeof(ask[1]), "the BioRand folder has another pack.");
    snprintf(ask[2], sizeof(ask[2]), "Items and doors will not match. Load it?");
    host_log("biorand: %s %s %s\n", ask[0], ask[1], ask[2]);
    return 1;
}

const char* pc_biorand_ask(int i) { return i >= 0 && i < 3 ? ask[i] : ""; }

/* loaded anyway: the save goes on under the current choice (its next save has it) */
void pc_biorand_save_accept(void* save)
{
    unsigned char* d = save;
    unsigned int w[2], sum = 0;
    int i;
    words(w);
    memcpy(d + BR_OFFSET, w, sizeof(w));
    for (i = 0; i < SAVE_DATA_BYTES - 4; i++) sum += d[i];   /* its byte sum, as savestore_card_valid checks it */
    memcpy(d + SAVE_DATA_BYTES - 4, &sum, 4);
}

/* the question on the load screen (ps2_LoadScreen.c DispLoadMessageSelect 12), above YES / NO (message 466) */
void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb);   /* lobby_glue.c: the game's font */
float pc_ui_text_w(const char* s, float sc);
void pc_biorand_load_text(void)
{
    int i;
    for (i = 0; i < 3; i++) pc_ui_text(320 - pc_ui_text_w(ask[i], 0.75f) / 2, 250 + 26 * i, ask[i], 0.75f, 0xFFFFFFFFu);
}

/* ---------------------------------------------------------------- the start inventory
   A co-op guest without a game of its own starts with AllItemInit's kit (coop_glue.c own_restore); with BioRand
   the pack's, as the host's new game got it: the C7/C9 actions of the "ev_flg 1 clear" blocks of room 0/0's entry
   script (scd0), run by the game's own handlers on our inventory. Only when that flag is set in the state we got:
   else our own entry into 0/0 runs the block (it is a new game) and would give it twice. */
int coop_insn_len_raw(const unsigned char* p);   /* net/coop_proto.c */
extern unsigned char* bhScePtr;
extern unsigned int (*bhScenarioJmpT[256])();

void pc_biorand_start_kit(void)
{
    unsigned char *d, *save = bhScePtr;
    unsigned int at[PC_KIT_MAX];
    int size = 0, ops, i;
    if (!sys || !biorand_active() || !(sys->ev_flg[0] & 0x40000000u) || !(d = biorand_room0(&size))) return;
    if ((ops = pc_startkit_scan(d, (unsigned int)size, at, PC_KIT_MAX, coop_insn_len_raw)) < 0) {   /* (checked whole first: all of it or nothing) */
        free(d);
        host_log("biorand: the start inventory of the pack's room 0/0 is damaged: not given\n");
        return;
    }
    for (i = 0; i < ops; i++) { bhScePtr = d + at[i]; bhScenarioJmpT[d[at[i]]](); }
    bhScePtr = save;
    free(d);
    host_log("biorand: start inventory from the pack's room 0/0 (%d actions)\n", ops);
}
