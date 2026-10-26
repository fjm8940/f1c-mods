/* CO-OP main-menu spinner logic. Pure: no Windows or game access.
 * Game mode values are dword_7D0410 (see docs/INVESTIGATION_2026-10-09.md). */
#ifndef COOP_MENU_LOGIC_H
#define COOP_MENU_LOGIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    COOP_MODE_TESTDAY = 1,
    COOP_MODE_RACE = 3,
    COOP_MODE_CHAMPIONSHIP = 4,
    COOP_MODE_MULTIPLAYER = 5,
    COOP_SPINNER_INDEX = 8,
    COOP_DIR_NEXT = 0,
    COOP_DIR_PREVIOUS = 1
};

/* Spinner step. handled=0 means the caller runs the native step and clears coop. */
void coop_on_change(int mode, int coop, int dir,
                    int *out_mode, int *out_coop, int *handled);
/* Item index the spinner must show. */
int coop_display_index(int mode, int coop);
/* CO-OP can only exist on top of the multiplayer mode. */
int coop_sanitize(int mode, int coop);
/* COOP-R01: 1 when bots still owned after CO-OP has been inert (not selected) with no
 * network session (net_state 0) for at least hold_ms. Times are GetTickCount values. */
int coop_bot_release_due(int inert, int net_state, int owned_bots,
                         uint32_t inert_since_ms, uint32_t now_ms, uint32_t hold_ms);

#ifdef __cplusplus
}
#endif

#endif
