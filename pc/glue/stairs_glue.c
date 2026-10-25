/* Auto-climb stairs (GAMEPLAY option, docs/USER_GUIDE.md): walking or running into a staircase
   climbs it as the action button would. player.c bhControlPlayer asks when the button was not
   pressed; 1 = call bhCheckExmAtari as for a press, so the climb and its camera are the game's.
   Only when the trigger a press would answer first (bhCheckExmAtari's order and test points) is a
   free staircase (type 1, bhCPM2_act_kdu / kdd, not the unused ladder variant) and the player
   faces its climb direction within 45 degrees: doors, items, examine spots and steps on or off a
   ledge still need the button. Game thread. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/player.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaMath.h"

int pc_in_play(const BH_PWORK* p, int scope);   /* reload_glue.c */
int pc_auto_stairs_on(void);                    /* the option (host.c) */
void host_log(const char* fmt, ...);

/* the direction bits of doors, examine spots and items (0x400..0x2000): not usable facing so */
static int blocked(const ATR_WORK* e)
{
    int a = (plp->ay + 8192) & 0xC000;
    return e->attr & (a == 0x8000 ? 0x400 : a == 0x4000 ? 0x800 : a == 0 ? 0x1000 : 0x2000);
}

int pc_auto_stairs(void)
{
    const EXP_WORK* x = (const EXP_WORK*)plp->exp0;
    float s, c, px[5], pz[5];
    int i, n, dir;
    /* walking or running (also while turning), the weapon down, where the press would examine */
    if (!pc_auto_stairs_on() || !pc_in_play(plp, 0) || plp->mode1 != 0 || plp->mode2 < 3 || plp->mode2 > 6 ||
        (plp->stflg & 0x10080) || (plp->flg & 0x4000004) || (sys->cb_flg & 0x4017))
        return 0;
    njSinCos(plp->ay, &s, &c);   /* bhCheckExmAtari's test point for each trigger type */
    px[0] = x->fpx - (s * (1.5f + (3.0f - x->arn)));
    pz[0] = x->fpz - (c * (1.5f + (3.0f - x->arn)));
    px[1] = plp->px - (0.75f * s);
    pz[1] = plp->pz - (0.75f * c);
    px[2] = plp->px - (4.5f * s);
    pz[2] = plp->pz - (4.5f * c);
    px[3] = x->fpx - (s * (3.0f + (3.0f - x->arn)));
    pz[3] = x->fpz - (c * (3.0f + (3.0f - x->arn)));
    px[4] = plp->px - (2.0f * s);
    pz[4] = plp->pz - (2.0f * c);
    n = rom->etc_n + sys->metc_n;
    for (i = 0; i < n; i++) {
        const ATR_WORK* e = i < rom->etc_n ? &rom->etcp[i] : &sys->metcp[i - rom->etc_n];
        if (!(e->flg & 0x1) || e->type > 4) continue;
        if (!(e->px <= px[e->type] && e->px + e->w >= px[e->type] && e->pz <= pz[e->type] && e->pz + e->d >= pz[e->type] && e->flr_no == plp->flr_no))
            continue;
        /* the ones a press passes over: a door, examine spot or item faced the wrong way, a step not to climb up */
        if (e->type == 2 ? (plp->stflg & 0x20) || e->prm0 : e->type != 1 && blocked(e)) continue;
        if (e->type != 1 || (e->attr & 0x400001)) return 0;   /* (0x400000: in use, 0x1: ladder) */
        dir = -((int)(182.04445f * (e->prm1 * 90)) & 0xFFFF);   /* the climb's facing (bhCPM2_act_kdu) */
        if ((unsigned short)(plp->ay - dir + 8192) >= 16384) return 0;
        host_log("stairs: auto-climb %s, trigger %d in %d/%d\n", e->prm0 ? "down" : "up", i, sys->stg_no, sys->rom_no);
        return 1;
    }
    return 0;
}
