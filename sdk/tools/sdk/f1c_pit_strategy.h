#ifndef F1C_PIT_STRATEGY_H
#define F1C_PIT_STRATEGY_H
/* AI pit-strategy restart preservation primitive. Reviewed static layout,
 * 2026-09-22 (revision 2 after independent review R1-R3); native/gameplay
 * acceptance pending.
 *
 * Evidence (canonical decompiled functions):
 *  - sub_429960 (called by AIDriverStructInit on a full restart reset) rebuilds
 *    the plan and zeroes the current-entry cursor at vehicle+0x45E0, so a car
 *    restored to a late lap can see an already-served early stop as due again.
 *  - AIPushLap (sub_42A180) requests a pit stop when
 *    entries[cursor].lap <= current lap; ForcePitExit/AIStateFlagSetter then
 *    set the native request latch (+0x454C).
 *  - AIDriverStructInit clears the request latch (this[4435]=0) on the same
 *    full reset, so a legitimate active request is otherwise lost.
 *  - PitStopDecisionTireCompound (sub_42A3D0) rewrites the current entry and
 *    advances the cursor through PostPitCleanup (sub_42A6F0) when a service or
 *    a forced retirement completes. Callers must not record their own forced
 *    pit-retirement mutations as race progress; this header cannot tell the
 *    two apart and only stores/restores what the caller captured.
 *  - The plan has at most four planned stops (sub_429960 parses 1..4 stop
 *    times); it writes a terminator entry at index count, so slots 0..4 are
 *    meaningful and 0x4590..0x45DF is exactly five 16-byte entries. The cursor
 *    sits immediately after them.
 *  - The plan is per vehicle, independent of slot/roster position, and only
 *    ever read for cars the game still counts as active. Retirees are excluded
 *    by the caller's active list; this header never widens ownership.
 *
 * Scope: the reviewed block below (count, five plan entries, cursor and the
 * reviewed request latch). Companion fields recomputed by
 * sub_429960/initialization (tyre-compound mirror at +0x458C, fuel scalar at
 * +0x45E8, stint/speed value at +0x45F0, service advance flag at +0x4584)
 * stay native: tyre/weather/fuel/damage decisions are deliberately not
 * rewritten. Restore reasserts a captured active request exactly as the
 * reviewed native setter would, and never clears a request.
 *
 * Extent contract (review finding R1): `bytes` is the caller's declared
 * readable/writable extent measured from the vehicle base pointer. Every
 * access is at an absolute offset inside the vehicle, so the minimum declared
 * extent is F1C_PIT_STRATEGY_REQUIRED_EXTENT (0x45E4, one past the cursor's
 * last byte). Both helpers refuse before any access when the declared extent
 * is smaller. A caller that passes a full vehicle stride always satisfies it.
 *
 * Caller owns exact vehicle identity, readable/writable extent, offline race
 * session and the reviewed game-thread/broker timing. Refusals are fail-safe:
 * nothing is written for an unsupported target.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum {
    F1C_PIT_STRATEGY_COUNT_OFFSET     = 0x4588u, /* int32 planned stops 0..4 */
    F1C_PIT_STRATEGY_ENTRIES_OFFSET   = 0x4590u, /* five 16-byte plan entries */
    F1C_PIT_STRATEGY_CURSOR_OFFSET    = 0x45E0u, /* int32 next-stop index */
    F1C_PIT_STRATEGY_CALLTOBOX_OFFSET = 0x454Cu, /* int32 native request latch */
    F1C_PIT_STRATEGY_ENTRY_BYTES      = 16u,
    F1C_PIT_STRATEGY_MAX_STOPS        = 4u,
    F1C_PIT_STRATEGY_ENTRY_SLOTS      = 5u,
    F1C_PIT_STRATEGY_REQUIRED_EXTENT  = 0x45E4u  /* highest offset + 1 */
};

typedef struct {
    int32_t  count;       /* planned stops in use, 0..4                          */
    int32_t  cursor;      /* completed/next stop index, 0..count                 */
    int32_t  call_to_box; /* legitimate native request latch at capture, 0 or 1  */
    uint32_t entries[F1C_PIT_STRATEGY_ENTRY_SLOTS][4]; /* raw plan entries 0..4 */
} F1CPitStrategyState;

/* Count/cursor bounds are the only invariants the native writers guarantee
 * across every plan shape (including the zero-stop plan when no pitstop
 * strategy is configured). The request latch is a reviewed 0/1 field. */
static int f1c_pit_strategy_state_valid(const F1CPitStrategyState *state) {
    if (!state) return 0;
    if (state->count < 0 || state->count > (int32_t)F1C_PIT_STRATEGY_MAX_STOPS)
        return 0;
    if (state->cursor < 0 || state->cursor > state->count) return 0;
    if (state->call_to_box < 0 || state->call_to_box > 1) return 0;
    return 1;
}

/* Read the complete reviewed plan. Returns 0 (accessing nothing) when the
 * declared extent is below REQUIRED_EXTENT or the native fields are outside
 * the reviewed bounds, so an unsupported/mismatched layout can never be
 * preserved as if it were valid. */
static int f1c_pit_strategy_capture(const void *vehicle, size_t bytes,
                                    F1CPitStrategyState *out) {
    F1CPitStrategyState state;
    if (!vehicle || !out || bytes < (size_t)F1C_PIT_STRATEGY_REQUIRED_EXTENT)
        return 0;
    memcpy(&state.count, (const uint8_t *)vehicle + F1C_PIT_STRATEGY_COUNT_OFFSET,
           sizeof state.count);
    memcpy(&state.cursor, (const uint8_t *)vehicle + F1C_PIT_STRATEGY_CURSOR_OFFSET,
           sizeof state.cursor);
    memcpy(&state.call_to_box,
           (const uint8_t *)vehicle + F1C_PIT_STRATEGY_CALLTOBOX_OFFSET,
           sizeof state.call_to_box);
    memcpy(state.entries, (const uint8_t *)vehicle + F1C_PIT_STRATEGY_ENTRIES_OFFSET,
           sizeof state.entries);
    if (!f1c_pit_strategy_state_valid(&state)) return 0;
    *out = state;
    return 1;
}

/* Write a captured plan back into the reviewed block. The target must first
 * pass the same capture validation (this proves the live block still looks
 * like the reviewed layout after the native reset); a valid saved state is
 * required as well. Entries, count and cursor are written, and an active
 * captured request is reasserted with the same 0/1 value the reviewed native
 * setter writes. A zero request is never written, so a legitimate request
 * raised after the restart is not cleared. */
static int f1c_pit_strategy_restore(void *vehicle, size_t bytes,
                                    const F1CPitStrategyState *state) {
    F1CPitStrategyState current;
    int32_t request = 1;
    if (!vehicle || !state) return 0;
    if (!f1c_pit_strategy_capture(vehicle, bytes, &current)) return 0;
    if (!f1c_pit_strategy_state_valid(state)) return 0;
    memcpy((uint8_t *)vehicle + F1C_PIT_STRATEGY_ENTRIES_OFFSET,
           state->entries, sizeof state->entries);
    memcpy((uint8_t *)vehicle + F1C_PIT_STRATEGY_COUNT_OFFSET,
           &state->count, sizeof state->count);
    memcpy((uint8_t *)vehicle + F1C_PIT_STRATEGY_CURSOR_OFFSET,
           &state->cursor, sizeof state->cursor);
    if (state->call_to_box == 1)
        memcpy((uint8_t *)vehicle + F1C_PIT_STRATEGY_CALLTOBOX_OFFSET,
               &request, sizeof request);
    return 1;
}
#endif
