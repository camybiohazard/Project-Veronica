/* "You appear as" for our own player (docs/ACTORS.md): another player file's mesh in place of the
 * character played's, or a catalogue model drawn over it (pc/glue/actor_glue.c), and the voice of
 * the character shown. Game thread: game.c calls appear_control after bhControlPlayer while
 * appear_on, appear_frame runs at the frame hand-off.
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/dread.h"
#include "../../decomp/include/ps2/veronica/prog/Motion.h"
#include "../../decomp/include/ps2/veronica/prog/MdlPut.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_texture.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaTextureFunction.h"
#include "../../decomp/include/ps2/veronica/prog/player.h"
#include "../src/host/actors.h"
#include <string.h>

void host_log(const char* fmt, ...);

int appear_on;                          /* "You appear as" in use: game.c runs appear_control */

static unsigned char part_file[1 << 21] __attribute__((aligned(64)));   /* one part file as read from disc */
static struct {                         /* the motions of a part file loaded only for its mesh */
    unsigned char bmt[393216], mth[12288], zmt[8192];
} sink __attribute__((aligned(64)));
static int in_game;                     /* appear_control ran this frame: our player is in a game */

/* a mesh's own bone offsets (node positions 1..21), put back where motions or scripts authored
   for another character wrote theirs */
static float self_bone[22][3];

static void save_bones(ML_WORK* m, float (*bone)[3])
{
    int i;
    for (i = 1; i < (int)m->obj_num && i < 22; i++) memcpy(bone[i], m->objP[i].pos, sizeof(bone[i]));
}

static void keep_bones(ML_WORK* m, float (*bone)[3])
{
    int i;
    for (i = 1; i < (int)m->obj_num && i < 22; i++) memcpy(m->objP[i].pos, bone[i], sizeof(bone[i]));
}

static int read_part(int file)   /* into part_file (actor_glue.c afs_read) */
{
    int ok = afs_read(sys->sys_partid, file, part_file, sizeof(part_file));
    if (!ok) host_log("appear: cannot read part file %d\n", file);
    return ok;
}

/* Claire's hair (id 1211, simulated by bhObjClpn) or Wesker's piece (1212, rigid) for our model as
   character `chr` (actor_glue.c player_hair) */
static void setup_hair(int chr)
{
    player_hair(sys->obwp, plp, chr, NULL, NULL);
}

/* The chosen part file's model is loaded into a buffer of its own (the motions into the sink), so
   the original model data in sys->plmdlp stays intact and is put back by copying the saved
   ML_WORKs. Our motions, weapon files, PlyInfo and voice stay the character's. The original's
   textures stay loaded while the mesh is shown. Sub-models the new file lacks (Claire's 5..7: the
   moth egg, en27, takes mdl[6], skp[7]) keep the original's. When the game reloads the player (game
   start, load, continue, story switch) the model no longer points into our buffer; it is applied
   again at the next frame in control. */
static unsigned char self_mdl[131072] __attribute__((aligned(64)));
static struct {
    ML_WORK mdl[16];
    int* skp[16];
    NJS_CNK_OBJECT* mbp[16];
    NJS_TEXLIST* txp[16];
    int mdl_n, mlw;
} self_orig;
static int self_mesh = -1, self_fail = -1, self_tex_gone;
static int self_cine;                   /* self_cine_hair: the character's own head part is up for a cutscene double */
/* What a cutscene leaves on the played character's model, set up for that character: scripts
   re-attach the head part sys->obwp[2] with its offsets (ObjLinkSetPly; Claire's ponytail at
   0, 1.58, 0.77, behind the head: Wesker's sunglasses ended up there) and a few room motions
   carry node positions (bone lengths). For another mesh both are put back every tick. */
static struct { NJS_CNK_OBJECT* objP; unsigned char* lkwkp; int lkono; float lox, loy, loz; } self_part;

static int self_ours(void) { return plp->mdl[0].objP && (unsigned char*)plp->mdl[0].objP >= self_mdl && (unsigned char*)plp->mdl[0].objP < self_mdl + sizeof(self_mdl); }

static void self_release(ML_WORK* mdl, int n)
{
    int i;
    if (self_tex_gone) return;   /* njReleaseTextureAll took them */
    for (i = 0; i < n; i++)
        if (mdl[i].texP) njReleaseTexture(mdl[i].texP);
    bhGarbageTexture(NULL, 0);
}

static void self_put_back(void);

/* back to the character's own mesh (or forget ours when the game has reloaded the player) */
static void self_restore(void)
{
    if (self_mesh < 0) return;
    if (self_ours()) {
        self_release(plp->mdl, plp->mdl_n);
        self_put_back();
        setup_hair(sys->ply_id);
        self_cine = 0;
    }
    /* else the game has loaded the player again: after the game ended (appear_textures_released) or a
       character switch (appear_player_unload put the original back first). The original's texture lists
       were in the buffer the new load overwrote: releasing them crashed (story switch, Battle Game). */
    self_mesh = -1;
}

/* system.c bhSysCallMonitor: the game releases the player's textures and loads another character */
void appear_player_unload(void) { self_restore(); }

/* The player's node matrices (O_WORK) are last tick's pose: bhFixPosition (motions that plant a foot,
   plp->flg 0x40000) moves the player by the difference between them and the new pose. A model swapped
   in must carry that pose, or the player jumps: fresh O_WORKs are zero (to the room's origin, out of
   the map), the original's were last computed when it was swapped out (back to where the player
   stood then, another room's coordinates after a door). Also no blend from the T-pose. */
static void self_carry_pose(const ML_WORK* from)
{
    ML_WORK* to = plp->mlwP;
    int i, n = from->obj_num < to->obj_num ? from->obj_num : to->obj_num;
    if (!from->objP || !to->objP || !from->owP || !to->owP || from->objP == to->objP) return;
    memcpy(to->objP[0].pos, from->objP[0].pos, sizeof(to->objP[0].pos));
    for (i = 0; i < n && i < 22; i++) memcpy(to->objP[i].ang, from->objP[i].ang, sizeof(to->objP[i].ang));
    bhCalcTree((NJS_MATRIX*)plp->mtx, to);
}

/* called with the original model in place (self_mesh < 0) */
static void self_put_back(void)
{
    ML_WORK cur = *plp->mlwP;
    int i;
    for (i = 0; i < 16; i++) { plp->mdl[i] = self_orig.mdl[i]; plp->skp[i] = self_orig.skp[i]; plp->mbp[i] = self_orig.mbp[i]; plp->txp[i] = self_orig.txp[i]; }
    plp->mdl_n = self_orig.mdl_n;
    plp->mlwP = &plp->mdl[self_orig.mlw];
    self_carry_pose(&cur);
}

static int self_load(int mesh)
{
    MN_WORK *mnw = plp->mnwP, *mnwb = plp->mnwPb;
    int i, mlw = plp->mlwP - plp->mdl, ok = 1;
    if (!read_part(mesh + 10)) return 0;
    memcpy(self_orig.mdl, plp->mdl, sizeof(self_orig.mdl));
    memcpy(self_orig.skp, plp->skp, sizeof(self_orig.skp));
    memcpy(self_orig.mbp, plp->mbp, sizeof(self_orig.mbp));
    memcpy(self_orig.txp, plp->txp, sizeof(self_orig.txp));
    self_orig.mdl_n = plp->mdl_n;
    self_orig.mlw = mlw >= 0 && mlw < 16 ? mlw : 0;
    self_tex_gone = 0;
    memset(plp->skp, 0, sizeof(plp->skp));   /* the loader would apply stale skin pointers */
    player_model_read(plp, part_file, self_mdl, sink.bmt, sink.mth, sink.zmt);
    plp->mnwP = mnw; plp->mnwPb = mnwb;
    for (i = 0; i < plp->mdl_n; i++) ok &= plp->mdl[i].owP != NULL;
    if (!plp->mdl_n || !ok) {   /* keep the original */
        self_release(plp->mdl, plp->mdl_n);
        self_put_back();
        return 0;
    }
    for (i = plp->mdl_n; i < self_orig.mdl_n; i++) {   /* sub-models only the original has (moth egg) */
        plp->mdl[i] = self_orig.mdl[i]; plp->skp[i] = self_orig.skp[i]; plp->mbp[i] = self_orig.mbp[i]; plp->txp[i] = self_orig.txp[i];
    }
    plp->mlwP = &plp->mdl[0];
    plp->mlwP->owP[7].flg |= 0x8;   /* as bhSetPlayer */
    plp->mlwP->owP[11].flg |= 0x8;
    self_carry_pose(&self_orig.mdl[self_orig.mlw]);
    setup_hair(mesh & 3);
    self_cine = 0;
    {
        O_WRK* op = &sys->obwp[2];
        self_part.objP = (op->flg & 0x1) ? op->mdl[0].objP : NULL;
        self_part.lkwkp = op->lkwkp; self_part.lkono = op->lkono;
        self_part.lox = op->lox; self_part.loy = op->loy; self_part.loz = op->loz;
    }
    save_bones(&plp->mdl[0], self_bone);
    self_mesh = mesh;
    host_log("appear: our own model is now part file %d\n", mesh + 10);
    return 1;
}

/* An enemy/NPC model shown for us (pc/glue/actor_glue.c) instead of the player's: the head part
   (ponytail, sunglasses) goes while it is shown, and comes back as bhSetPlayer sets it up. */
static void self_actor(int idx)
{
    int was = actor_shown(0);
    if (actor_set(0, idx)) { if (!self_cine) sys->obwp[2].flg = 0; }
    else if (was) { setup_hair(self_mesh >= 0 ? self_mesh & 3 : sys->ply_id); self_cine = 0; }
}

/* "Swapped model in cutscenes" off (g_appear_cutscene), or the doubles kept for the played character's
   own cutscene model (actor_double_kept): while a cutscene double of the character played acts, sys->obwp[2] is the character's own head part (Claire's ponytail: the scripts link it to the
   double, ObjLinkSet), from its own model; when the double is gone, ours again. Where the script
   linked it (work, node, x offset) is kept. Every tick before the objects follow their links. */
extern ETTY_WORK lkmtab[2];             /* player.c: the head parts (both sub-model 4) */
static void self_cine_hair(void)
{
    O_WRK* op = &sys->obwp[2];
    int mesh = self_mesh >= 0 && self_ours(), want = (!g_appear_cutscene || actor_double_kept()) && (actor_shown(0) || mesh) && actor_double_acting();
    int v = lkmtab[0].mdlver, n = plp->mdl_n, ono = op->lkono, *s = plp->skp[v];
    unsigned char* lk = op->lkwkp;
    float lox = op->lox;
    ML_WORK m;
    if (want == self_cine) return;
    self_cine = want;
    m = plp->mdl[v];
    if (want && mesh) {   /* the head part of the model the game loaded (self_orig) */
        plp->mdl[v] = self_orig.mdl[v]; plp->skp[v] = self_orig.skp[v]; plp->mdl_n = self_orig.mdl_n;
    }
    if (want || mesh) setup_hair(want ? sys->ply_id : self_mesh & 3);   /* (as bhSetPlayer / self_load) */
    else op->flg = 0;   /* (as self_actor) */
    plp->mdl[v] = m; plp->skp[v] = s; plp->mdl_n = n;
    op->lkwkp = lk; op->lkono = ono; op->lox = lox;
    if (want && op->id == 1211) {   /* the ponytail's work: the player's, as bhObjClpn takes it linked to plp
                                       (linked to the double it took it from sys->memp, under a pending disc read) */
        op->flg |= 0x100000;
        op->exp0 = &sys->pletcp[4096];
        op->exp3 = sys->pletcp;
    }
}

/* every tick after bhControlPlayer (before the objects follow their links) */
static void self_keep(void)
{
    O_WRK* op = &sys->obwp[2];
    self_cine_hair();
    if (actor_shown(0) && !self_cine) op->flg &= ~0x1;   /* also when the game set up the player again (story switch) */
    actor_source(0, plp, self_mesh >= 0 && self_ours() ? self_mesh & 3 : actor_shown(0) ? -1 : sys->ply_id, sys->ply_id, tps_aim_pitch());   /* gender-matched animations */
    actor_pose(0, plp);
    if (self_mesh < 0 || !self_ours()) return;
    keep_bones(&plp->mdl[0], self_bone);
    if (self_part.objP && op->mdl[0].objP == self_part.objP) {
        op->lkwkp = self_part.lkwkp; op->lkono = self_part.lkono;
        op->lox = self_part.lox; op->loy = self_part.loy; op->loz = self_part.loz;
        op->flg |= 0x80;
    }
}

/* at the hand-off */
static void self_frame(void)
{
    int own = sys->ply_id + (sys->costume & 3) * 4, want, chr = g_appear_chr;
    if (!plp || !plp->mdl_n) return;
    if (self_mesh >= 0 && !self_ours()) self_restore();   /* the game reloaded the player */
    if (in_game)   /* in cutscenes the character played is acted by its event double */
        actor_doubles(!chr ? -1 : chr >= APPEAR_CHR_ACTOR ? chr - APPEAR_CHR_ACTOR :
                      (chr == 5 ? 4 : chr - 1) == own ? -1 : ACTOR_PART + (chr == 5 ? 4 : chr - 1));
    if (!in_game || (sys->cb_flg & 0x1)) return;
    self_actor(chr >= APPEAR_CHR_ACTOR ? chr - APPEAR_CHR_ACTOR : -1);
    want = !chr || chr >= APPEAR_CHR_ACTOR ? -1 : chr == 5 ? 4 : chr - 1;
    if (want == own) want = -1;
    if (want == self_mesh || (want >= 0 && want == self_fail)) return;   /* (-1 is no failure: back to the original) */
    self_restore();
    self_fail = -1;
    if (want >= 0 && !self_load(want)) { self_fail = want; host_log("appear: part file %d cannot be our model\n", want + 10); }
}

/* ps2_NaTextureFunction.c njReleaseTextureAll */
void appear_textures_released(void)
{
    actor_textures_released();
    self_tex_gone = 1;
    self_cine = 0;
    /* our mesh ends with the game too: the character's own model data back in the player work (its
       head part comes with the next game's bhSetPlayer). Left in place, putting it back later set up
       the head part in sys->obwp, NULL at the title: the crash. */
    if (self_mesh >= 0 && self_ours()) self_put_back();
    self_mesh = -1;
    /* the game ended (bhExitGame: continue, title, load): our player is not in a game until
       appear_control runs again. Set in this frame before the exit, it made the hand-off load
       the appearance and write sys->obwp[2] of the zeroed game state (NULL): the crash on continue. */
    in_game = 0;
}

/* game.c bhMainSequence, after bhControlPlayer (while appear_on) */
void appear_control(void)
{
    if (!plp || !plp->mnwP) return;
    self_keep();
    actor_double_tick();   /* the cutscene double as our model (its hands before the objects follow them) */
    in_game = 1;
}

/* the frame hand-off (pc/src/ps2/libgraph.c) */
void appear_frame(void)
{
    appear_on = sys && plp && (g_appear_chr || self_mesh >= 0 || actor_shown(0) || actor_shown(ACTOR_DOUBLE));
    if (appear_on) self_frame();
    in_game = 0;
}

/* Voices (sdfunc.c CallPlayerVoice, docs/ACTORS.md "Voices"): a player vocal sound (SE bank 4,
   0x400 + program) in the voice of the character shown (appear_voice). Returns the extra bank:
   0 = the game's bank 4 (CORE_<ply_id>) when that is the voice, else that CORE bank (snd_xbank_load). */
int snd_xbank_load(int key);
int pc_player_voice(int se)
{
    int own = sys->ply_id, v;
    (void)se;
    v = !g_appear_cutscene && actor_double_acting() ? -1 : appear_voice(g_appear_chr);   /* "Swapped model in cutscenes" off: the double's own */
    if (v < 0) v = own;
    return v == own ? 0 : snd_xbank_load(0xFFF0 | v);
}
