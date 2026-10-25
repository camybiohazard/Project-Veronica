/* Frozen-enemy watchdog, a safety net (eneset.c bhControlEnemy, every enemy step): an enemy alive
 * whose modes, motion, frame, position and hp have not changed for WATCH_STEPS steps (5 s) is
 * logged (and every 30 s it stays so). A zombie stuck in its grab (mode0 2: every grab state moves
 * on by its motion) is put back to standing (then the chase), and our player let go if it plays
 * that zombie's grab. Not a zombie lying after a push-off (mode0 1, MV04, motion stopped, the hold
 * bit still set): it waits, legitimately, until no other zombie is getting up
 * (bhEne01_CmnFlgCheck). The repair only in a room with added enemies (pc_enemy_added): a retail
 * room behaves as retail (the log stays).
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void host_log(const char* fmt, ...);
uint64_t gs_frame_count(void);

static int ene_slot(void* p) { int k = (BH_PWORK*)p - ene; return p && k >= 0 && k < 128 ? k : 255; }

/* an en01 grab hold: set when the grab starts, cleared by every release (~0x24080, ~0xA4080) */
static int holding(BH_PWORK* ep) { return ep->exp0 && (*(int*)(ep->exp0 + 0x40) & 0x20000); }

/* the grab was refused (eneset.c's one-holder rule) or froze (the watchdog): the enemy goes back to
   standing (en01 MV00: idle motion, then the chase), released, as at the end of a grab. Its linked
   lower body runs the same modes (en01 copies them at every change): it goes along. */
static void unhold(BH_PWORK* ep, int dead)
{
    if (!ep->exp0 || ep->id != 1) return;
    *(int*)(ep->exp0 + 0x40) &= ~0xA4080;
    if (!dead) {
        *(int*)&ep->mode0 = 1;
        ep->flg2 &= ~1;
        ep->flg |= 0x40;   /* the player collides with it again (the grab cleared it, its end sets it) */
    }
    if (ene_slot(ep->exp1) < 128 && (((BH_PWORK*)ep->exp1)->flg & 0x80)) unhold((BH_PWORK*)ep->exp1, dead);
}
/* A dead one only lets go, its death goes on: put back to standing, a crawler killed by the player's
   stomp at the end of its leg grab (en01 NG02 sets hp -1 while the player still plays the stomp) walked
   again with hp -1 and grabbed again - one grab after another, for good. */
void pc_enemy_unhold(BH_PWORK* ep) { unhold(ep, ep->hp < 0); }

/* our player out of a grab, as en01 releases it */
static void player_let_go(void)
{
    sys->pad_on &= ~0xF;
    plp->flg &= ~0x10004;
    plp->flg |= 8;
    plp->stflg &= ~0x50000;
    plp->at_flg = 0;
    plp->mnwP = plp->mnwPb;
    *(int*)&plp->mode0 = 1;
}

/* an event task works on this enemy (LoadWork): its script drives it */
extern BH_SCEWORK bhEtask[16];
static int event_driven(const BH_PWORK* ep)
{
    int i;
    for (i = 0; i < 16; i++)
        if (bhEtask[i].status && bhEtask[i].work == ep) return 1;
    return 0;
}

/* being hit or held by an enemy (flg 4), or dying */
static int ply_busy(const BH_PWORK* pl) { return (pl->flg & 4) || pl->mode0 == 4 || pl->mode0 == 6 || pl->hp < 0; }

#define WATCH_STEPS 150
static struct { int sig[6], still; } watch[128];
/* Test aid RECVX_ENEMY_TRACE=dist: one line per step of every living enemy (not parts) within dist
   units (10 = 1 m) of our player: its type, distance, modes, motion, hp, mode0 of the player, whether
   it counts as busy, a zombie's exp0 flag word (0x20000 holding), the enemy's and our position. Only reads. */
static int etr_lim = -1;
static void enemy_trace(BH_PWORK* ep, int i)
{
    int lim = etr_lim < 0 ? (etr_lim = getenv("RECVX_ENEMY_TRACE") ? atoi(getenv("RECVX_ENEMY_TRACE")) : 0) : etr_lim;
    float dh;
    if (!lim || !sys || !plp || (ep->flg & 0x80) || !ep->mode0 || ep->hp < 0 || ep->id >= 31) return;
    dh = sqrtf((ep->px - plp->px) * (ep->px - plp->px) + (ep->pz - plp->pz) * (ep->pz - plp->pz));
    if (dh > lim) return;
    fprintf(stderr, "etr g=%u s=%d id=%d ty=%d dH=%.1f m=%d.%d.%d.%d mtn=%u f=%d hp=%d pH=%d.%d bH=%d x=%x e=%.0f,%.0f h=%.0f,%.0f\n", (unsigned)gs_frame_count(), i, ep->id, ep->type,
            dh, ep->mode0, ep->mode1, ep->mode2, ep->mode3, ep->mtn_no, ep->frm_no >> 16, ep->hp, plp->hp, plp->mode0, ply_busy(plp),
            ep->id == 1 && ep->exp0 ? *(int*)(ep->exp0 + 0x40) : 0, ep->px, ep->pz, plp->px, plp->pz);
}

void pc_enemy_watch(BH_PWORK* ep, int i)
{
    int s[6] = { *(int*)&ep->mode0, (int)ep->mtn_no, ep->frm_no, 0, 0, ep->hp };
    memcpy(&s[3], &ep->px, 4); memcpy(&s[4], &ep->pz, 4);
    if (i < 0 || i >= 128) return;
    enemy_trace(ep, i);
    if (memcmp(s, watch[i].sig, sizeof(s))) { memcpy(watch[i].sig, s, sizeof(s)); watch[i].still = 0; return; }
    if (++watch[i].still < WATCH_STEPS || (watch[i].still - WATCH_STEPS) % 900 || (ep->flg & 0x80) || !ep->mode0 || ep->hp < 0 || (!ep->hp && ep->id != 1)) return;   /* a zombie at hp 0 is alive; helpers (hp 0) are not watched */
    host_log("watchdog: enemy %d (id %d) still for %d steps: modes %d %d %d %d, motion %u frame %d add %d, hp %d, flg %x flg2 %x, hold %d\n",
             i, ep->id, watch[i].still, ep->mode0, ep->mode1, ep->mode2, ep->mode3, ep->mtn_no, ep->frm_no >> 16, ep->mtn_add, ep->hp,
             ep->flg, ep->flg2, ep->id == 1 && holding(ep));
    if (ep->id != 1 || ep->mode0 != 2 || event_driven(ep)) return;
    if (!pc_enemy_added) return;   /* a retail room: as retail (logged only) */
    if (plp && holding(ep) && plp->mnwP == ep->mnwP && plp->mnwP != plp->mnwPb && (plp->mode0 == 4 || plp->mode0 == 6)) player_let_go();
    pc_enemy_unhold(ep);
    host_log("watchdog: enemy %d was stuck in its grab: back to standing\n", i);
}
