#pragma once
/* Read-only live-session guard. Addresses and the offline predicate are
 * copied from the canonical Red Flag v76 review; consumers receive only a
 * fail-closed verdict and must not embed these target details themselves. */
#include <stdint.h>
#include <string.h>
typedef int (*F1CSessionGuardRead)(uintptr_t, void *, size_t);
typedef struct {
    int valid, offline, settled, paused, player_out;
    int session_type, game_state, race_phase;
} F1CSessionGuard;
static F1CSessionGuard f1c_session_guard_read(F1CSessionGuardRead read_at) {
    F1CSessionGuard g = {0};
    uint32_t manager = 0;
    int state = -1;
    if (!read_at ||
        !read_at(0x007C56A4u, &g.session_type, sizeof(g.session_type)) ||
        !read_at(0x007C569Cu, &g.game_state, sizeof(g.game_state)) ||
        !read_at(0x007C56A8u, &g.race_phase, sizeof(g.race_phase)) ||
        !read_at(0x007C56ACu, &g.paused, 1) ||
        !read_at(0x007CE258u, &g.player_out, sizeof(g.player_out)) ||
        !read_at(0x007CD25Cu, &manager, sizeof(manager)) || !manager ||
        manager > UINT32_MAX - 16324u - (uint32_t)sizeof(state) ||
        !read_at(manager + 16324u, &state, sizeof(state))) return g;
    g.valid = 1;
    g.offline = state == 0; /* the only measured safe value */
    g.settled = g.session_type == 7 &&
        (g.game_state == 5 || g.game_state == 7) &&
        (g.race_phase == 3 || g.race_phase == 5) &&
        !g.paused;
    return g;
}
