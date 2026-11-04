/* Quick knife (docs/ARCHITECTURE.md "Input"): while its key is held the knife is equipped and
   readied as if chosen in the inventory and aimed, so the fire button slashes; on release the
   weapon equipped before comes back once the knife is lowered again. Everything goes through
   the inventory's own WeaponSet (equipped slot sys->itm[ply*16], weapon number, the weapon
   file load, motions), so ammo, aiming, firing, and the third-person pose of the knife
   are the game's. Game thread: knife_pad from bhSetPad (pad.c), after tps_pad; the weapon shortcuts
   (weapon_glue.c) follow it.

   Always have the knife (knife_always): the quick knife does not need the bag's knife. While drawn it is
   an item in inventory entry HIDDEN, which no grid, pickup, item box or event looks at (they use
   entries 2..11), and it is gone again once the knife is lowered; the bag's knife, if any, stays a
   normal item (equipped from the inventory, an event that asks for it). The inventory and map
   buttons wait while it is in the hand: the inventory screen shifts the equipped entry when items
   move, which would lose an entry outside the grid. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/sub1.h"
#include "../src/host/cheat.h"
#include <stdlib.h>

int pad_quick_knife(void);
int pc_in_play(const BH_PWORK* p, int scope);   /* reload_glue.c */
int pc_quick_knife_on(void);   /* the option (host.c) */
int pc_knife_always_on(void);
void wpn_pad(void);            /* weapon_glue.c */

#define KNIFE 8                         /* item 8 "Combat Knife", weapon 2 (WpnTab flag 0x40000000: the knife's hit check) */
#define ID(v) ((v) >> 16 & 0x7FF)       /* item id without the ammo kind and unlimited bits */
#define MENUS 0xE000u                   /* virtual pad bits bhCheckSubTask opens the map, inventory, options with */
#define HIDDEN 15                       /* the always-there knife's entry: past the grid (2..11, 12..15 unused) */
#define HIDDEN_KNIFE ((unsigned int)KNIFE << 16)

static int state;                       /* 0 idle, 1 held, 2 released: the weapon before comes back when the knife is down */
static int own;                         /* we equipped it (else it was equipped already: nothing to give back) */
static int knife_slot, prev_slot, prev_id;
static int hidden;                      /* the drawn knife is the always-there one (knife_slot HIDDEN) */

static unsigned int* inv(void) { return &sys->itm[sys->ply_id * 16]; }

/* normal play: our player in control; no event, door, menu, first person or attract demo (reload_glue.c) */
static int in_play(void) { return pc_in_play(plp, 0); }

/* weapon down and standing, turning, walking, running or stepping back (not the quick turn,
   stairs, pushing, ...), or in the idle animation (mode2 13, after 10 s standing: flg 0x10000 is
   its pad lock, which the aim button breaks as from standing, bhCPM0_action) */
static int at_rest(void)
{
    return in_play() && plp->mode1 == 0 && ((plp->mode2 <= 11 && !(plp->flg & 0x10000)) || plp->mode2 == 13);
}
int pc_weapon_at_rest(void) { return sys && plp && at_rest(); }   /* also the weapon shortcuts' (weapon_glue.c) */

/* the knife is drawn by its key or about to be given back: the weapon shortcuts wait (weapon_glue.c) */
int knife_busy(void) { return state || (pc_quick_knife_on() && g_cheat.noclip != 2 && pad_quick_knife()); }

/* WeaponSet queues the weapon file load (system.c bhSysCallMonitor mode 3, a few frames). The
   inventory or map opened before it ends loads over it: the new weapon number with the old
   weapon's model and motions (the knife firing bullets); the options screen, whose files go
   where the weapon's do, crashed. Their buttons wait until it is done. */
static int loading;   /* frames left to hold them, while a load we started may still run */
void pc_weapon_load_started(void) { loading = 120; }   /* also the weapon shortcuts' (weapon_glue.c) */

static unsigned int load_hold(unsigned int pad)
{
    if (!loading) return pad;
    if (!*(int*)&sys->mn_mode0 && !*(int*)&sys->mn_md0 && !sys->mn_setct) loading = 0;   /* done */
    else { loading--; pad &= ~MENUS; }
    return pad;
}

static void equip(int slot)
{
    swork.pip = inv();
    WeaponSet(slot, slot);   /* as the inventory: weapon number, equipped slot, weapon file load */
    pc_weapon_load_started();
}

static void give_back(void)
{
    unsigned int* iv = inv();
    int i, n = (sys->gm_flg & 0x8000000) ? 11 : 9, to = 0;
    if (!prev_id) to = knife_slot;   /* nothing before: the knife's slot again unequips it (WeaponSet) */
    else if (ID(iv[prev_slot]) == prev_id) to = prev_slot;
    else
        for (i = 1; i <= n && !to; i++) if (ID(iv[i]) == prev_id) to = i;   /* moved */
    host_log("knife: back to %s (slot %d, item %d, count %d)\n", !prev_id ? "no weapon" : to ? "the weapon before" :
             hidden ? "nothing (it is gone), the knife goes" : "nothing (it is gone), the knife stays",
             to, to && prev_id ? ID(iv[to]) : 0, to && prev_id ? iv[to] & 0xFFFF : 0);
    if (!to && hidden) to = HIDDEN;   /* the always-there knife cannot stay: unequipped */
    if (to) equip(to);
    state = 0;
}

/* the always-there knife outside its use (put away, or left behind by an event, game over or a save
   made with it drawn): gone, unequipped first when it is still in the hand; an equipped entry the
   inventory screen shifted off it (12..14, empty) is pointed back at it for that */
static void hidden_tidy(unsigned int* iv)
{
    if (iv[HIDDEN] != HIDDEN_KNIFE) return;
    if (iv[0] > 11 && iv[0] < HIDDEN && !iv[iv[0]]) iv[0] = HIDDEN;
    if (iv[0] == HIDDEN) {
        if (!at_rest()) return;   /* as give_back: once the game has lowered it */
        host_log("knife: the always-there knife was left in the hand: put away\n");
        if (plp->wpnr_no == 2) equip(HIDDEN);   /* the same weapon from the same slot: unequipped */
        else iv[0] = 0;
    }
    iv[HIDDEN] = 0;
}

/* RECVX_KNIFE_TRACE=1 (test aid): state, player mode and the room's enemy health every 15 frames
   while the key is held. RECVX_KNIFE_DROP=1: drawing the knife empties the previous weapon's
   slot (as if it were used up or thrown away meanwhile). */
static int knife_drop(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECVX_KNIFE_DROP") != NULL;
    return on;
}

static void trace(void)
{
    static int on = -1, n;
    float hp = 0;
    unsigned int* iv = inv();
    if (on < 0) on = getenv("RECVX_KNIFE_TRACE") != NULL;
    if (!on || n++ % 15) return;
    for (int i = 0; i < sys->ewk_n; i++) if (ene[i].flg & 1) hp += ene[i].hp;
    host_log("knife: trace state %d play %d rest %d mode %d/%d/%d/%d weapon %d inv %x %x %x %x hidden %x enemy hp %.0f\n", state, in_play(), at_rest(), plp->mode0,
             plp->mode1, plp->mode2, plp->mode3, plp->wpnr_no, iv[0], iv[1], iv[2], iv[3], iv[HIDDEN], hp);
}

void knife_game_exit(void) { state = 0; hidden = 0; loading = 0; }

/* pad.c bhSetPad, before pad_ps is derived (virtual bits) */
unsigned int knife_pad(unsigned int pad)
{
    unsigned int* iv;
    int held, i, n;
    if (!sys || !plp) return pad;
    iv = inv();
    if (state && (plp->mode0 == 3 || plp->hp <= 0 ||iv[0] != (unsigned int)knife_slot || ID(iv[knife_slot]) != KNIFE)) {
        host_log("knife: weapon changed elsewhere, nothing to give back\n");   /* inventory, script, game over */
        state = 0;
    }
    if (!state) {
        hidden = 0;
        hidden_tidy(iv);
    }
    held = pc_quick_knife_on() && g_cheat.noclip != 2 && pad_quick_knife();
    if (held) trace();
    if (held && state == 2) state = 1;
    if (held && !state && at_rest()) {
        n = (sys->gm_flg & 0x8000000) ? 11 : 9;   /* [1] the separate slot (lighter), [2..] the grid */
        for (i = 1, knife_slot = 0; i <= n && !knife_slot; i++) if (ID(iv[i]) == KNIFE) knife_slot = i;
        if (pc_knife_always_on() && (!knife_slot || (unsigned int)knife_slot != iv[0]) && !iv[HIDDEN]) {
            iv[HIDDEN] = HIDDEN_KNIFE;   /* the bag's knife only when it is the one in the hand already */
            knife_slot = HIDDEN;
            hidden = 1;
        }
        if (knife_slot) {
            state = 1;
            own = iv[0] != (unsigned int)knife_slot;
            prev_slot = iv[0];
            prev_id = prev_slot ? ID(iv[prev_slot]) : 0;
            host_log("knife: drawn from %s %d (before: slot %d, item %d, count %d)\n", hidden ? "the always-there entry" : "slot", knife_slot, prev_slot, prev_id, prev_id ? iv[prev_slot] & 0xFFFF : 0);
            if (own) equip(knife_slot);
            if (own && prev_id && knife_drop()) iv[prev_slot] = 0;
        }
    }
    if (state == 1 && !held) {
        state = own ? 2 : 0;
        host_log("knife: released in mode %d/%d%s\n", plp->mode1, plp->mode2, own ? "" : " (it was equipped before: it stays)");
    }
    if (state == 2 && at_rest()) give_back();   /* after the swing: the game lowers the knife first */
    if (state == 1 && in_play()) pad |= 0x10;   /* readied: fire slashes */
    if (state && hidden && in_play()) pad &= ~MENUS;   /* the inventory, map and options wait */
    wpn_pad();
    return load_hold(pad);
}
