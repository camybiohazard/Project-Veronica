/* Enemy options (docs/ENEMIES.md "Enemy options"): extra enemies, rearranged placements, randomizer. */
#ifndef RECVX_ENEMIES_H
#define RECVX_ENEMIES_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct EnemyOpts {
    int extra;                  /* 0 off, 1..3: up to that many extra enemies per room (types the room has) */
    int place;                  /* 1: rearranged placements */
    int rand;                   /* 1: enemy randomizer */
    int seed;                   /* placements and randomizer (0..2^31-1) */
    int variety;                /* randomizer: 0 Mild (two common kinds per room), 1 Wild (every kind that fits; default) */
    int events;                 /* randomizer: 1 also event enemies (default) */
} EnemyOpts;
extern EnemyOpts g_enemy_opts;  /* our settings (F1, recvx.ini) */

/* an enemy that is not in the room file: where its data is and where it stands */
typedef struct SpawnNote {
    unsigned short room;        /* source room file RM_<room>.RDX and its record */
    unsigned char rec, id, mdlver, type;
    signed char flr;
    unsigned char pad;
    float px, py, pz;
    short ay;
    unsigned short variant;     /* an enemy variant's id (pc/src/host/variants.h; 0: the type itself) */
} SpawnNote;

/* spawn_at's `mode` (pc/glue/spawn_glue.c) */
#define SPAWN_LOADING 1             /* from the room load (the randomizer): no readiness checks */

/* enemy slots the spawner and the randomizer leave free for what enemies spawn at run time (moth
   eggs, damage dummies; bhSetEnemy has no NULL check in their callers) */
#define SPAWN_SLOTS_FREE 6
int  spawn_slots_needed(int id);                /* a type and the enemies its Init spawns */

/* game thread */
const EnemyOpts* enemy_opts(void);              /* in effect (g_enemy_opts) */
int  room_file_now(void);                       /* the room file we are in: RM_<stg * 1000 + rom * 10 + case> */

#ifdef __cplusplus
}
#endif
#endif
