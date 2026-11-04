/* Update check (update.c): once a day at start, the latest GitHub release of the project; the
   title screen and F1 say when it is newer than this build. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_RELEASES_URL "https://github.com/camybiohazard/Project-Veronica/releases/latest"

void update_start(void);                 /* main thread, after config_load: the check in the background (Check for updates) */
int  update_newer(char* ver, int n);     /* a newer release is known (its version into ver, as 1.2.3): 1 */
const char* update_current(void);        /* this build's version, 1.2.3 */
void update_title_seen(void);            /* game thread: the title menu ran (title_glue.c) */
int  update_title_up(void);              /* the title menu is up now */

#ifdef __cplusplus
}
#endif
