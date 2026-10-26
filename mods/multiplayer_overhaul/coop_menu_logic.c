#include "coop_menu_logic.h"

void coop_on_change(int mode, int coop, int dir,
                    int *out_mode, int *out_coop, int *handled)
{
    coop = coop_sanitize(mode, coop);
    *out_mode = mode;
    *out_coop = 0;
    *handled = 0;
    if (dir == COOP_DIR_NEXT) {
        if (mode == COOP_MODE_MULTIPLAYER && !coop) {
            *out_coop = 1;
            *handled = 1;
        } else if (mode == COOP_MODE_MULTIPLAYER && coop) {
            *out_mode = COOP_MODE_TESTDAY;
            *handled = 1;
        }
    } else if (dir == COOP_DIR_PREVIOUS) {
        if (mode == COOP_MODE_MULTIPLAYER && coop) {
            *handled = 1;
        } else if (mode == COOP_MODE_TESTDAY) {
            *out_mode = COOP_MODE_MULTIPLAYER;
            *out_coop = 1;
            *handled = 1;
        }
    }
}

int coop_display_index(int mode, int coop)
{
    return coop_sanitize(mode, coop) ? COOP_SPINNER_INDEX : mode;
}

int coop_sanitize(int mode, int coop)
{
    return mode == COOP_MODE_MULTIPLAYER && coop ? 1 : 0;
}

int coop_bot_release_due(int inert, int net_state, int owned_bots,
                         uint32_t inert_since_ms, uint32_t now_ms, uint32_t hold_ms)
{
    if (!inert || owned_bots <= 0 || net_state != 0) return 0;
    return (uint32_t)(now_ms - inert_since_ms) >= hold_ms;
}
