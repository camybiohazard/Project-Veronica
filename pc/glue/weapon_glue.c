/* Weapon shortcuts (docs/USER_GUIDE.md "Weapon shortcuts", docs/ARCHITECTURE.md "Input"): previous / next weapon (keys
 * 1 / 2, the d-pad's left / right) equip the bag's weapon before / after the one in the hand, in the inventory's
 * order (the lighter's slot first, then the grid left to right, top to bottom), round from the last to the first;
 * with none in the hand next gives the first, previous the last. Claire's lighter is never counted (equipable, but no
 * weapon); with the quick knife on the knife is not counted either, it has its own key.
 *
 * Equipped through the inventory's own WeaponSet, as the quick knife (knife_glue.c): equipped slot, weapon number,
 * weapon file and model, ammo display. Only at rest (the quick knife's rule: in play, weapon down, standing,
 * walking, running or idle); a press up to half a second before that waits for it. No other weapon: nothing.
 *
 * In play with the weapon down the shortcut rows' inputs are theirs alone (pad.c g_wpn_take, bindings.c): the
 * d-pad's left / right no longer turn there (the left stick does, up / down still walk), and in menus, events and
 * while aiming it is the game's as before. Game thread: wpn_pad from knife_pad (bhSetPad), after the quick knife. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/sub1.h"
#include "../../decomp/include/ps2/veronica/prog/item.h"
#include "../../decomp/include/ps2/veronica/prog/flag.h"
#include "../src/host/bindings.h"
#include "../src/host/cheat.h"

void host_log(const char* fmt, ...);
int pc_in_play(const BH_PWORK* p, int scope);   /* reload_glue.c */
int pc_weapon_at_rest(void);                    /* knife_glue.c */
int knife_busy(void);
int pc_quick_knife_on(void);                    /* host.c: the settings */
int pc_weapon_shortcuts_on(void);
void pc_weapon_load_started(void);              /* knife_glue.c: the inventory and map wait for the weapon load */
extern PARTS parts_06b[24];

#define KNIFE 8                                 /* item 8 "Combat Knife" (knife_glue.c) */
#define LIGHTER 55                              /* item 55, the lighter (itemview.c LighterOpen) */
#define ID(v) ((int)((v) >> 16 & 0xFF))         /* item id without the ammo kind (WeaponSet's switch) */

static int pending, pending_t;                  /* -1 previous, 1 next, 0 none */
static unsigned int was_keys;

static unsigned int* inv(void) { return &sys->itm[(sys->ply_id & 3) * 16]; }
static int bag_n(void) { return (sys->gm_flg & 0x8000000) ? 11 : 9; }   /* [1] the lighter's slot, [2..] the grid */
static int weapon(int id) { return id > 0 && id < 156 && (itemdata[id].type & 0x1) && id != LIGHTER && !(id == KNIFE && pc_quick_knife_on()); }   /* equipable (ItemCommand's EQUIP) */

/* the bag slot (1..bag_n) of the weapon dir (-1, 1) from the one in the hand, round; 0 none other */
static int cycle(int dir)
{
    unsigned int* iv = inv();
    int n = bag_n(), at = iv[0] >= 1 && iv[0] <= (unsigned int)n ? (int)iv[0] : dir > 0 ? 0 : n + 1;
    for (int k = 1; k <= n; k++) {
        int i = ((at - 1 + dir * k) % n + n) % n + 1;
        if (i == (int)iv[0]) break;   /* round to the one in the hand */
        if (weapon(ID(iv[i]))) return i;
    }
    return 0;
}

/* pad.c, each pad report: the shortcuts take their inputs (our player in play with the weapon down; not while the
   quick knife has the hand or noclip flies) */
int wpn_take_ok(void)
{
    return pc_weapon_shortcuts_on() && sys && plp && pc_in_play(plp, 0) && plp->mode1 == 0 && !knife_busy() && g_cheat.noclip != 2;
}

static void equip(int dir)
{
    unsigned int* iv = inv();
    int at = cycle(dir), id = ID(iv[at]);
    if (!at) return;   /* none other meanwhile */
    host_log("weapons: %s: item %d (bag slot %d, count %u) for item %d\n", dir > 0 ? "next" : "previous", id, at, iv[at] & 0xFFFF,
             iv[0] ? ID(iv[iv[0]]) : 0);
    /* the sniper rifle's scope state (ev_flg 74) goes through the command list's sprite (WeaponSet reads
       parts_06b[17].anim, ItemCommand sets it from the flag): kept as it is */
    if (id == 10) parts_06b[17].anim = bhCkFlg(sys->ev_flg, 74) ? 12 : 11;
    swork.pip = iv;
    WeaponSet(at, at);   /* as the inventory: weapon number, equipped slot, weapon file load, motion reset */
    pc_weapon_load_started();
}

static void request(int dir)
{
    if (!cycle(dir)) {
        host_log("weapons: %s: no other weapon in the bag\n", dir > 0 ? "next" : "previous");
        return;
    }
    pending = dir;
    pending_t = 15;   /* game frames: a press just before the weapon is down counts */
}

void wpn_game_exit(void) { pending = 0; }

/* knife_pad (pad.c bhSetPad), after the quick knife */
void wpn_pad(void)
{
    unsigned int keys = 0, press;
    int n;
    if (!sys || !plp) return;
    for (n = 0; n < WPN_KEYS; n++) if (pad_wpn_key(n)) keys |= 1u << n;
    press = keys & ~was_keys;
    was_keys = keys;
    /* only presses the shortcuts took (elsewhere the d-pad was the game's: a turn, aiming) */
    if (g_wpn_take && press) request(press & 1 ? -1 : 1);   /* (both at once: previous) */
    if (!pending) return;
    if (!pc_weapon_shortcuts_on() || !pc_in_play(plp, 0) || --pending_t < 0) {
        host_log("weapons: %s dropped (%s)\n", pending > 0 ? "next" : "previous", pending_t < 0 ? "not at rest in time" : "no longer in play");
        pending = 0;
    } else if (pc_weapon_at_rest() && !knife_busy()) {
        equip(pending);
        pending = 0;
    }
}
