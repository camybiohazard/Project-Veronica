/* Enemy variants (docs/ENEMIES.md "Enemy variants"): spawnable enemies made of parts the game has - a
   base type's AI, collision and motions, optionally another model drawn retargeted onto its skeleton,
   stat factors, motion and sound mappings - and randomizer metadata. Built in: a C table
   (variants.c); more from mods/enemies.ini. Each has a stable id (the F2 spawner and the randomizer
   carry it). No disc data and no game state here (pc/glue/variant_glue.c applies
   them); ctest `variants`. */
#ifndef RECVX_VARIANTS_H
#define RECVX_VARIANTS_H
#ifdef __cplusplus
extern "C" {
#endif

#define VARIANT_MAX 64
#define VARIANT_USER_MIN 100            /* ids of mods/enemies.ini: 100..999; built in: 1..99 */
#define VARIANT_USER_MAX 999
#define VARIANT_STAGES_ALL 0x3FF        /* stages 0..9 */

typedef struct EnemyVariant {
    unsigned short id;
    char name[40];                      /* logs and the randomizer's lines; the spawner list shows ui */
    char ui[48];                        /* "[C] " + name */
    unsigned char type, mdlver;         /* base: the spawner entry of this enemy type and model version (AI, hits, motions) */
    short model;                        /* "You appear as" catalogue entry drawn instead, retargeted onto the base's skeleton; -1: the base's own
                                           (and, while a donor motion plays, the base's catalogue model posed from the donor) */
    unsigned char attach_n, attach[4][2];   /* sub-models of that model drawn on its nodes ({sub-model, node}): Monster Steve's arms */
    float hp;                           /* times the base's hit points (of each difficulty) */
    float armour;                       /* damage taken: weapon damage x armour / hp */
    float damage;                       /* damage dealt to the player, times the base's */
    float speed;                        /* steps per frame (0.5..2): AI, motions, movement together */
    float scale;                        /* drawn size (hits and collision stay the base's) */
    unsigned char motion_n;
    short motion[8][2];                 /* the base asks for motion [0], plays [1] of its own set */
    unsigned char donor_type, donor_mdlver;  /* another enemy whose motions it borrows (0: none): its skeleton plays them, */
    unsigned char dmotion_n, death_n;   /*   retargeted onto the drawn model (the base's own or `model`) */
    short dmotion[8][2];                /* while the base plays motion [0], the body shows the donor's motion [1] (same phase) */
    short death[4];                     /* when the base dies (flg & 2): these donor motions in turn, the last one's end held */
    unsigned short sound_room;          /* RM_<room>'s enemy sound bank for the mapped sounds (0: none) */
    unsigned char sound_n, sound[16][2];/* base sound program [0] -> program [1] of that bank */
    unsigned char hurt;                 /* program of that bank played as its voice when a hit lands (0: none) */
    unsigned char wild, mild;           /* randomizer weights in Wild / Mild (0: never drawn) */
    unsigned char slots;                /* enemy slots it reserves (0: the base's) */
    unsigned char big;                  /* needs open space (not in corridors and small rooms) */
    unsigned char heavy;                /* boss-like: Wild only, one per room, open space, not where doors count kills */
    unsigned short stages;              /* bit n: the randomizer may place it in stage n (areas: 0 prison .. 9 Antarctic base) */
    unsigned char user;                 /* from mods/enemies.ini */
} EnemyVariant;

int  variant_count(void);               /* built in, then the mods file's */
const EnemyVariant* variant_at(int i);
const EnemyVariant* variant_by_id(int id);   /* NULL: unknown */
/* "[Name]" sections of "key = value" lines (docs/ENEMIES.md "Enemy variants"); each valid section adds a
   variant. Problems go to log(line, message) (NULL: silent). Returns how many were added. */
int  variants_parse(const char* text, void (*log)(int line, const char* msg));
void variants_load(void);               /* <mods>/enemies.ini (RECVX_MODS, else ./mods), once at start */
void variants_reset_user(void);         /* (tests) drop the mods file's */

#ifdef __cplusplus
}
#endif
#endif
