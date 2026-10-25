/* Room scripts (docs/modding/ROOM_FORMAT.md "Event scripts"): the instruction table the PC hooks
   walk scripts with (cheat_glue.c "Unlock all", enemyopt_glue.c). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int script_insn_len(const uint8_t* p);      /* bytes of the instruction at p, 0 unknown */
int script_op_cond(int op);                 /* tests: continue or skip, nothing else */
/* EtcAtariSet2 / WalAtariSet2 / FlrAtariSet2: the record index and what the instruction writes */
typedef struct ScriptAtrSet { uint8_t ix, type, prm[4]; uint32_t attr; } ScriptAtrSet;
int script_atr_set(const uint8_t* insn, ScriptAtrSet* out);   /* 1: the instruction is such a write */

#ifdef __cplusplus
}
#endif
