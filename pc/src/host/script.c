/* Room script instruction lengths and kinds (script.h, docs/modding/ROOM_FORMAT.md "Event scripts").
   Pure functions over the script bytes: no game state. */
#include "script.h"
#include <string.h>

static const unsigned char op_len[256] = {   /* pc/tools/rdx.py OP_LEN */
    2, 2, 2, 2, 6, 6, 4, 6, 4, 4, 4, 4, 4, 4, 6, 2, 2, 2, 2, 4, 4, 4, 2, 8, 2, 8, 2, 6, 6, 4, 6, 4,
    4, 6, 4, 6, 4, 10, 2, 2, 2, 2, 4, 2, 4, 2, 2, 4, 4, 2, 12, 8, 12, 4, 6, 2, 6, 4, 2, 6, 4, 4, 4, 4,
    8, 2, 4, 4, 6, 2, 8, 10, 6, 10, 8, 4, 6, 4, 14, 6, 2, 12, 12, 12, 12, 2, 6, 2, 14, 12, 4, 2, 2, 6, 4, 2,
    6, 2, 6, 2, 1, 4, 1, 1, 6, 1, 2, 2, 8, 22, 22, 12, 2, 2, 14, 22, 4, 4, 4, 6, 14, 6, 2, 10, 10, 4, 4, 6,
    4, 2, 4, 2, 8, 4, 16, 2, 10, 2, 2, 12, 6, 2, 4, 4, 2, 4, 2, 2, 4, 4, 2, 8, 12, 4, 4, 6, 2, 4, 2, 2,
    6, 2, 4, 12, 4, 4, 4, 4, 10, 2, 10, 6, 6, 4, 2, 4, 22, 6, 4, 8, 4, 4, 2, 2, 2, 10, 16, 2, 2, 2, 6, 4,
    28, 2, 6, 4, 8, 2, 2, 4, 8, 4, 2, 2, 4, 2, 2, 2, 4, 10, 10, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 4, 3, 4, 2, 2, 1, 1, 2,
};
/* controller ops 100/102/103/105: sub-command -> length incl. the sub-command byte (rdx.py
   CTRL_LEN); an unknown sub-command is not consumed */
static const unsigned char ctrl100[][2] = { { 0, 1 }, { 7, 7 }, { 12, 5 }, { 13, 5 }, { 52, 1 }, { 128, 1 }, { 129, 11 }, { 130, 1 }, { 131, 3 }, { 132, 5 }, { 137, 3 }, { 139, 1 }, { 140, 7 }, { 141, 5 }, { 142, 1 }, { 143, 3 }, { 145, 5 }, { 150, 3 } };
static const unsigned char ctrl102[][2] = { { 1, 1 }, { 20, 3 } };
static const unsigned char ctrl103[][2] = { { 0, 1 }, { 128, 1 }, { 139, 1 }, { 143, 5 }, { 144, 1 }, { 146, 7 }, { 147, 7 }, { 148, 3 }, { 149, 3 } };
static const unsigned char ctrl105[][2] = { { 0, 1 }, { 2, 2 }, { 3, 2 }, { 4, 2 }, { 5, 5 }, { 6, 5 }, { 7, 7 }, { 8, 7 }, { 9, 5 }, { 10, 5 }, { 11, 5 }, { 12, 5 }, { 13, 5 }, { 14, 3 }, { 15, 5 }, { 16, 5 }, { 17, 2 }, { 18, 2 }, { 19, 2 }, { 21, 3 }, { 22, 5 }, { 23, 3 }, { 24, 7 }, { 25, 7 }, { 26, 9 }, { 27, 7 }, { 28, 2 }, { 29, 2 }, { 30, 2 }, { 31, 2 }, { 32, 2 }, { 33, 2 }, { 34, 9 }, { 35, 7 }, { 36, 1 }, { 38, 9 }, { 40, 3 }, { 41, 3 }, { 42, 3 }, { 43, 4 }, { 44, 4 }, { 45, 4 }, { 46, 3 }, { 47, 1 }, { 48, 3 }, { 49, 3 }, { 50, 3 }, { 51, 5 }, { 52, 3 }, { 53, 3 }, { 132, 5 }, { 140, 7 }, { 141, 5 } };

static int sub_len(const unsigned char (*t)[2], int n, int sub)
{
    int i;
    for (i = 0; i < n; i++)
        if (t[i][0] == sub) return t[i][1];
    return 0;
}

int script_insn_len(const uint8_t* p)
{
    switch (p[0]) {
    case 100: return 1 + sub_len(ctrl100, sizeof(ctrl100) / 2, p[1]);
    case 102: return 1 + sub_len(ctrl102, sizeof(ctrl102) / 2, p[1]);
    case 103: return 1 + sub_len(ctrl103, sizeof(ctrl103) / 2, p[1]);
    case 105: return 1 + sub_len(ctrl105, sizeof(ctrl105) / 2, p[1]);
    }
    return op_len[p[0]];
}

/* Ck CmpB CmpW UseItemCheck PlItemCheck AdxCk AdxTimeCk DieEventCk ArmsItemCheck FrameCheck
   PlItemBlockCk PadCheck TFrameCheck CamCheck AreaSearchObj SubMapBusyCk PlayerPoisonCk RandTest
   ZombieUpDieCk MovieCk MovieTimeCk ObjDposCk PlyDposCk */
int script_op_cond(int op)
{
    switch (op) {
    case 4: case 6: case 7: case 16: case 17: case 27: case 30: case 33: case 38: case 56: case 77: case 93: case 96:
    case 98: case 114: case 129: case 138: case 140: case 142: case 155: case 172: case 177: case 190: return 1;
    }
    return 0;
}

int script_atr_set(const uint8_t* p, ScriptAtrSet* out)
{
    if (p[0] != 37 && p[0] != 123 && p[0] != 124) return 0;   /* EtcAtariSet2 WalAtariSet2 FlrAtariSet2 (event.c) */
    out->ix = p[1];
    out->attr = p[2] | p[3] << 8;
    memcpy(out->prm, p + 4, 4);
    out->type = p[8];
    return 1;
}
