/* Pure broker core: event ring, cursor/drop accounting, vehicle slot
 * identity transitions, and generation math. No game memory, no hooks, no
 * process state. Shared verbatim by the sandbox broker
 * (mods/sdk_sandbox/game_update_broker/game_update_broker.c) and the
 * deterministic regression harness (broker_regression_test.c) so the tests
 * exercise exactly the code the broker runs.
 *
 * Semantics mirror the broker ABI contracts in tools/sdk/f1c_broker_abi.h:
 *  - The ring keeps the most recent F1C_BROKER_EVENT_CAPACITY events.
 *  - Polling reports the precise number of records lost when a client cursor
 *    falls behind the oldest retained sequence; cursors never roll back.
 *  - A slot identity changes (generation bump) when a live vehicle at a slot
 *    is destroyed or replaced, so stale handles fail closed.
 */
#pragma once

#include <string.h>

#include "f1c_broker_abi.h"

#ifndef VEH_SLOTS
#define VEH_SLOTS 104u
#endif
_Static_assert(VEH_SLOTS == 104u,
               "broker core requires the canonical 104 vehicle slots");

typedef struct BrokerEventRing {
    F1CBrokerEvent events[F1C_BROKER_EVENT_CAPACITY];
    uint64_t sequence; /* latest emitted event sequence (0 = empty) */
} BrokerEventRing;

/* Appends one event, overwriting the oldest when full. update_sequence is the
 * owner's snapshot sequence at emit time; session_generation is the owner's
 * current session generation. Identical to the broker's emit_event_locked. */
static void broker_ring_emit(BrokerEventRing *ring, uint64_t update_sequence,
                             uint32_t session_generation,
                             F1CBrokerEventType type, uint32_t slot,
                             uint32_t object_generation, int32_t value)
{
    F1CBrokerEvent *event;
    uint64_t sequence = ++ring->sequence;
    event = &ring->events[(sequence - 1u) % F1C_BROKER_EVENT_CAPACITY];
    memset(event, 0, sizeof(*event));
    event->structure_size = sizeof(*event);
    event->type = (uint32_t)type;
    event->sequence = sequence;
    event->update_sequence = update_sequence;
    event->session_generation = session_generation;
    event->object_generation = object_generation;
    event->vehicle_slot = slot;
    event->value = value;
}

/* Bounded read of events after after_sequence. Returns records lost when the
 * cursor fell behind the retained window and never returns a record the caller
 * already consumed. Identical to the broker's F1CBrokerPollEvents body.
 * Returns F1C_BROKER_BAD_ARGUMENT for a hostile buffer and writes nothing. */
static F1CBrokerResult broker_ring_poll(
    const BrokerEventRing *ring, uint64_t after_sequence,
    F1CBrokerEvent *events, uint32_t capacity, uint32_t *count_out,
    uint64_t *cursor_out, uint64_t *latest_sequence_out, uint64_t *dropped_out)
{
    uint64_t oldest;
    uint64_t sequence;
    uint64_t dropped = 0;
    uint32_t count = 0;
    if (!count_out || !cursor_out || !latest_sequence_out || !dropped_out ||
        (capacity && !events))
        return F1C_BROKER_BAD_ARGUMENT;
    oldest = ring->sequence > F1C_BROKER_EVENT_CAPACITY
        ? ring->sequence - F1C_BROKER_EVENT_CAPACITY + 1u : 1u;
    sequence = after_sequence + 1u;
    if (!sequence) sequence = 1u;
    if (sequence < oldest) {
        dropped = oldest - sequence;
        sequence = oldest;
    }
    while (sequence <= ring->sequence && count < capacity) {
        events[count++] = ring->events[(sequence - 1u) %
                                       F1C_BROKER_EVENT_CAPACITY];
        ++sequence;
    }
    *cursor_out = count ? sequence - 1u : after_sequence;
    *latest_sequence_out = ring->sequence;
    *dropped_out = dropped;
    *count_out = count;
    return F1C_BROKER_OK;
}

/* Monotonic generation helpers. Object generations bump per slot identity
 * change; session generations wrap at 24 bits. Zero is never a valid
 * generation, so every wrap skips it. Identical to the broker's statics. */
static uint32_t broker_next_object_generation(uint32_t generation)
{
    ++generation;
    return generation ? generation : 1u;
}

static uint32_t broker_next_session_generation(uint32_t generation)
{
    generation = (generation + 1u) & 0x00FFFFFFu;
    return generation ? generation : 1u;
}

/* Per-slot vehicle identity. generation bumps on every observed
 * destroy/recreate, so a handle holding an old generation is rejected. */
typedef struct BrokerSlotState {
    uint32_t generation;
    int32_t driver_id;
    uint8_t was_live;
} BrokerSlotState;

typedef enum BrokerSlotTransition {
    BROKER_SLOT_NONE = 0,
    BROKER_SLOT_DESTROYED = 1,
    BROKER_SLOT_CREATED = 2,
    BROKER_SLOT_DESTROYED_CREATED = 3
} BrokerSlotTransition;

/* Advances the slot identity for one observation. live is whether a vehicle
 * occupies the slot; driver_id identifies the occupant. Emits exactly the
 * destroy/create decisions the broker's capture_vehicle_snapshots performs:
 * destroy carries the old generation, create the bumped generation. */
static BrokerSlotTransition broker_slot_transition(
    BrokerSlotState *state, int live, int32_t driver_id,
    uint32_t *destroy_generation, uint32_t *create_generation)
{
    if (!live) {
        if (state->was_live) {
            if (destroy_generation) *destroy_generation = state->generation;
            state->was_live = 0;
            return BROKER_SLOT_DESTROYED;
        }
        return BROKER_SLOT_NONE;
    }
    if (!state->was_live || state->driver_id != driver_id) {
        BrokerSlotTransition result =
            state->was_live ? BROKER_SLOT_DESTROYED_CREATED
                            : BROKER_SLOT_CREATED;
        if (destroy_generation && state->was_live)
            *destroy_generation = state->generation;
        state->generation = broker_next_object_generation(state->generation);
        state->driver_id = driver_id;
        state->was_live = 1;
        if (create_generation) *create_generation = state->generation;
        return result;
    }
    return BROKER_SLOT_NONE;
}

/* Handle lookup decision used by F1CBrokerGetVehicleSnapshot:
 * BAD_ARGUMENT for a malformed handle, STALE_SESSION after a session
 * replacement, STALE_OBJECT when the slot is empty or its identity changed. */
static F1CBrokerResult broker_handle_result(
    uint32_t session_generation, int session_active,
    const F1CBrokerVehicleSnapshot *slot_snapshot,
    const F1CBrokerVehicleHandle *handle)
{
    if (handle->slot >= VEH_SLOTS || !handle->session_generation ||
        !handle->object_generation)
        return F1C_BROKER_BAD_ARGUMENT;
    if (!session_active || handle->session_generation != session_generation)
        return F1C_BROKER_STALE_SESSION;
    if (!slot_snapshot->structure_size ||
        slot_snapshot->handle.object_generation != handle->object_generation)
        return F1C_BROKER_STALE_OBJECT;
    return F1C_BROKER_OK;
}

/* ============================ pause-menu core =============================
 *
 * Pure state machine behind the broker's ABI-10 pause-menu capability. The
 * broker runs this every frame in its RestartGate pre-section (before the
 * original RestartGate_004a0fd0 scans the key queue for DIK 1 = ESC and
 * abandons the race). The game's key queue is a per-frame array of pressed
 * DirectInput scancodes at word_7CB21C with count dword_7CB218; the broker
 * feeds this core the queue, and the core decides the menu command.
 *
 * Key codes are the game's own (RestartGate matches `*v5 == 1` for ESC):
 *   ESC 0x01, UP 0xC8, DOWN 0xD0, ENTER 0x1C, R 0x13 (replay inject).
 *
 * Edge handling: the queue holds pressed keys every frame, so a held key
 * would retrigger; prev_flags tracks last frame's pressed bits and commands
 * fire only on 0->1 edges. While the menu is open the broker swallows the
 * whole queue (zeroes the count) so the game never sees ESC/ENTER/etc.
 */

#define F1C_MENU_KEY_ESC      0x01u
#define F1C_MENU_KEY_UP       0xC8u
#define F1C_MENU_KEY_DOWN     0xD0u
#define F1C_MENU_KEY_CONFIRM  0x39u /* DIK_SPACE confirms the selection */
#define F1C_MENU_KEY_GARAGE_EXIT 0x2Du /* DIK_X after RETIRE TO PITS */
#define F1C_MENU_OPTION_COUNT 3u /* maximum selectable options in any mode */
#define F1C_MENU_LINE_COUNT   3u /* maximum rendered lines (strip holds 4) */

#define F1C_MENU_MODE_RACE     0u  /* CONTINUE/RETIRE TO PITS/EXIT RACE */
#define F1C_MENU_MODE_SESSION  1u  /* CONTINUE/RETURN TO PITS */
#define F1C_MENU_MODE_CONFIRM_EXIT 2u /* race: REALLY EXIT RACE? NO/YES */
/* 2026-10-12 (owner): multiplayer/CO-OP race has no EXIT. RETIRE TO PITS is
 * the native multiplayer ESC (RACE RESULTS, then garage, then LEAVE CIRCUIT),
 * so it returns EXIT_RACE and the broker hands ESC back to the game. */
#define F1C_MENU_MODE_MP_RACE  3u  /* CONTINUE/RETIRE TO PITS */
/* broker_menu_tick race_session value for a multiplayer race. */
#define F1C_MENU_SESSION_MP_RACE 2

/* Selectable options per mode. Practice/qualifying/warmup/test day have no
 * EXIT: RETURN TO PITS is the vanilla ESC action there. */
static uint8_t broker_menu_option_count(uint8_t mode)
{
    if (mode == F1C_MENU_MODE_RACE) return 3u;
    return 2u;
}

typedef struct F1CMenuState {
    uint8_t open;      /* menu currently open */
    uint8_t mode;      /* F1C_MENU_MODE_*; race may enter CONFIRM_EXIT */
    uint8_t cursor;    /* 0..broker_menu_option_count(mode)-1 */
    uint8_t prev_flags;/* bit0 ESC, bit1 UP, bit2 DOWN, bit3 ENTER (edges) */
} F1CMenuState;

#define F1C_MENU_FLAG_ESC     0x01u
#define F1C_MENU_FLAG_UP      0x02u
#define F1C_MENU_FLAG_DOWN    0x04u
#define F1C_MENU_FLAG_CONFIRM 0x08u

/* Scans keys for a pressed bit; returns 1 on a 0->1 edge, updates state. */
static int broker_menu_edge(F1CMenuState *state, const uint16_t *keys,
                            uint32_t key_count, uint16_t key, uint8_t flag)
{
    uint32_t i;
    int pressed = 0;
    for (i = 0; i < key_count; ++i)
        if (keys[i] == key) { pressed = 1; break; }
    if (pressed && !(state->prev_flags & flag)) {
        state->prev_flags |= flag;
        return 1;
    }
    if (!pressed) state->prev_flags &= (uint8_t)~flag;
    return 0;
}

/* Per-frame menu tick. armed is the capability switch, race_session whether
 * SessionType == 7 (selects the option label set). Returns the command the
 * broker must apply; F1C_BROKER_MENU_NONE means nothing to do. Updates state
 * including edge tracking. Always call every frame (even while a pending
 * external command is queued) so edge tracking stays current. */
static uint8_t broker_menu_tick(F1CMenuState *state, int armed, int race_session,
                                const uint16_t *keys, uint32_t key_count)
{
    uint8_t command = F1C_BROKER_MENU_NONE;
    if (!armed || !state) return command;
    if (!state->open) {
        if (broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_ESC,
                             F1C_MENU_FLAG_ESC)) {
            state->mode = race_session == F1C_MENU_SESSION_MP_RACE
                ? F1C_MENU_MODE_MP_RACE
                : race_session ? F1C_MENU_MODE_RACE : F1C_MENU_MODE_SESSION;
            state->cursor = 0;
            state->open = 1;
            command = F1C_BROKER_MENU_OPEN;
        }
        /* consume edge state for the other keys too so a held key cannot
           fire on the frame the menu opens */
        broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_UP,
                         F1C_MENU_FLAG_UP);
        broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_DOWN,
                         F1C_MENU_FLAG_DOWN);
        broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_CONFIRM,
                         F1C_MENU_FLAG_CONFIRM);
        return command;
    }
    if (broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_ESC,
                         F1C_MENU_FLAG_ESC)) {
        if (state->mode == F1C_MENU_MODE_CONFIRM_EXIT) {
            /* ESC backs out of the prompt; menu and pause stay. */
            state->mode = F1C_MENU_MODE_RACE;
            state->cursor = 2u;
            return F1C_BROKER_MENU_CURSOR_UP;
        }
        state->open = 0;
        return F1C_BROKER_MENU_CONTINUE;
    }
    if (broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_UP,
                         F1C_MENU_FLAG_UP)) {
        state->cursor = state->cursor == 0
            ? (uint8_t)(broker_menu_option_count(state->mode) - 1u)
            : (uint8_t)(state->cursor - 1u);
        return F1C_BROKER_MENU_CURSOR_UP;
    }
    if (broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_DOWN,
                         F1C_MENU_FLAG_DOWN)) {
        state->cursor = (uint8_t)((state->cursor + 1u) %
                                  broker_menu_option_count(state->mode));
        return F1C_BROKER_MENU_CURSOR_DOWN;
    }
    if (broker_menu_edge(state, keys, key_count, F1C_MENU_KEY_CONFIRM,
                         F1C_MENU_FLAG_CONFIRM)) {
        if (state->mode == F1C_MENU_MODE_CONFIRM_EXIT) {
            if (state->cursor == 1u) {
                state->open = 0;
                return F1C_BROKER_MENU_EXIT_RACE;
            }
            state->mode = F1C_MENU_MODE_RACE;   /* NO: back to the menu */
            state->cursor = 2u;
            return F1C_BROKER_MENU_CURSOR_UP;
        }
        if (state->mode == F1C_MENU_MODE_RACE && state->cursor == 2u) {
            state->mode = F1C_MENU_MODE_CONFIRM_EXIT;
            state->cursor = 0u;                 /* default to NO */
            return F1C_BROKER_MENU_CURSOR_DOWN;
        }
        state->open = 0;
        if (state->cursor == 0u) return F1C_BROKER_MENU_CONTINUE;
        /* Non-race RETURN TO PITS is exactly vanilla ESC: hand the native
           key back (EXIT_RACE command) instead of the race retirement write
           set, which leaves a later pit exit as a frozen retired car. */
        return state->mode == F1C_MENU_MODE_SESSION ||
               state->mode == F1C_MENU_MODE_MP_RACE
            ? F1C_BROKER_MENU_EXIT_RACE : F1C_BROKER_MENU_RETIRE_PITS;
    }
    return command;
}

/* Once RETIRE TO PITS has selected the garage spectator route, X must take
 * the same native race-exit path as vanilla ESC. Consuming the one-shot arm
 * prevents a held key or later menu from repeating the transition. */
/* Preserved legacy helper, no longer called by the broker. X is a GUI icon. */
static inline uint8_t broker_menu_retired_exit_tick(uint8_t *armed,
                                             const uint16_t *keys,
                                             uint32_t key_count,
                                             int physical_x_down)
{
    uint32_t i;
    if (!armed || !*armed) return F1C_BROKER_MENU_NONE;
    if (physical_x_down) {
        *armed = 0u;
        return F1C_BROKER_MENU_EXIT_RACE;
    }
    for (i = 0; i < key_count; ++i) {
        if (keys[i] == F1C_MENU_KEY_GARAGE_EXIT) {
            *armed = 0u;
            return F1C_BROKER_MENU_EXIT_RACE;
        }
    }
    return F1C_BROKER_MENU_NONE;
}

static int broker_menu_should_arm_retired_exit(uint8_t command,
                                               int command_from_menu,
                                               uint8_t menu_mode)
{
    return command == F1C_BROKER_MENU_RETIRE_PITS && command_from_menu &&
           menu_mode == F1C_MENU_MODE_RACE;
}

/* RestartGate returns the current game state. State 2 is the intended garage
 * spectator handoff and must retain the X arm. A real full exit (6), or the
 * next session entering, ends ownership of the old garage action. */
static uint8_t broker_menu_retired_exit_after_restart(uint8_t armed,
                                                      int restart_result,
                                                      int session_entered)
{
    if (session_entered || restart_result == 6) return 0u;
    return armed;
}

/* Commands which must be handed back to the game's native input handlers.
 * The caller appends this key exactly once after applying the command state. */
static uint16_t broker_menu_native_key(uint8_t command)
{
    if (command == F1C_BROKER_MENU_EXIT_RACE) return F1C_MENU_KEY_ESC;
    if (command == F1C_BROKER_MENU_REPLAY) return 0x13u; /* DIK_R */
    return 0u;
}

/* Native P-pause contract: the game's frame tick treats byte_7C56AC as the
 * authoritative pause state. Opening sets it; every closing action clears it;
 * cursor and no-op commands leave the current state untouched. */
static uint8_t broker_menu_pause_value(uint8_t command, uint8_t current)
{
    if (command == F1C_BROKER_MENU_OPEN) return 1u;
    if (command == F1C_BROKER_MENU_CONTINUE ||
        command == F1C_BROKER_MENU_RETIRE_PITS ||
        command == F1C_BROKER_MENU_EXIT_RACE ||
        command == F1C_BROKER_MENU_REPLAY) return 0u;
    return current;
}

/* Native P is handled after RestartGate. Suppress its action on both boundary
 * frames as well as every frame between them; ordinary P remains untouched
 * when the ESC menu is closed. */
static int broker_menu_blocks_native_pause(int menu_was_open, uint8_t command)
{
    return menu_was_open || command == F1C_BROKER_MENU_OPEN;
}

/* Rendered lines: the confirm prompt adds a non-selectable header. */
static uint8_t broker_menu_line_count(uint8_t mode)
{
    return (uint8_t)(broker_menu_option_count(mode) +
                     (mode == F1C_MENU_MODE_CONFIRM_EXIT ? 1u : 0u));
}

/* Renders one menu line for slot index (0..broker_menu_line_count-1) into
 * out (capacity >= 32). Line 0 of CONFIRM_EXIT is its header. */
static const char *broker_menu_label(uint8_t mode, uint8_t slot, int selected,
                                     char *out, size_t capacity)
{
    static const char *const race_labels[F1C_MENU_OPTION_COUNT] = {
        "CONTINUE", "RETIRE TO PITS", "EXIT RACE"
    };
    static const char *const session_labels[F1C_MENU_OPTION_COUNT] = {
        "CONTINUE", "RETURN TO PITS", ""
    };
    static const char *const confirm_labels[F1C_MENU_LINE_COUNT] = {
        "REALLY EXIT RACE?", "NO", "YES"
    };
    static const char *const mp_race_labels[F1C_MENU_OPTION_COUNT] = {
        "CONTINUE", "RETIRE TO PITS", ""
    };
    const char *const *labels = mode == F1C_MENU_MODE_RACE ? race_labels
        : mode == F1C_MENU_MODE_CONFIRM_EXIT ? confirm_labels
        : mode == F1C_MENU_MODE_MP_RACE ? mp_race_labels
        : session_labels;
    const char *label;
    if (!out || capacity < 32u) return NULL;
    if (slot >= broker_menu_line_count(mode)) return NULL;
    label = labels[slot];
    out[0] = selected ? '>' : ' ';
    out[1] = ' ';
    {
        size_t i;
        size_t limit = capacity - 3u < 30u ? capacity - 3u : 30u;
        for (i = 0; i < limit && label[i]; ++i) out[2 + i] = label[i];
        out[2 + i] = '\0';
    }
    return out;
}
