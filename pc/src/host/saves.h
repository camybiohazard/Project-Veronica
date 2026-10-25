/* The save system (ini save_system, docs/USER_GUIDE.md "Saves"): native files (default) or the
   memory card. Any thread. */
#ifndef RECVX_SAVES_H
#define RECVX_SAVES_H
#ifdef __cplusplus
extern "C" {
#endif

int  save_native(void);
const char* save_native_dir(void);      /* <save_dir>/saves; the first use creates it and copies the card's saves in */

#ifdef __cplusplus
}
#endif
#endif
