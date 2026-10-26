#pragma once

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#define F1C_BROKER_ABI_VERSION 15u
#define F1C_BROKER_MIN_COMPATIBLE_ABI 5u
#define F1C_BROKER_INFO_ABI6_SIZE 24u
#define F1C_BROKER_MAX_CLIENTS 16u
#define F1C_BROKER_QUEUE_CAPACITY 256u
#define F1C_BROKER_CLIENT_QUOTA 32u
#define F1C_BROKER_UPDATE_BUDGET 32u
#define F1C_BROKER_UPDATE_TIME_BUDGET_US 2000u
#define F1C_BROKER_SLOW_CALLBACK_US 5000u
#define F1C_BROKER_EVENT_CAPACITY 256u

#define F1C_BROKER_CAP_LIFECYCLE          (1ull << 0)
#define F1C_BROKER_CAP_SEMANTIC_PIT       (1ull << 1)
#define F1C_BROKER_CAP_VEHICLE_SNAPSHOTS  (1ull << 2)
#define F1C_BROKER_CAP_POLL_EVENTS        (1ull << 3)
#define F1C_BROKER_CAP_SESSION_SNAPSHOT   (1ull << 4)
#define F1C_BROKER_CAP_PACE_CONTROL       (1ull << 5)
#define F1C_BROKER_CAP_PAUSE_MENU         (1ull << 6)
#define F1C_BROKER_CAP_HIGH_GEAR_RATIO    (1ull << 7)
#define F1C_BROKER_CAP_RESTART_GUARD      (1ull << 8)
#define F1C_BROKER_CAP_RESULTS_FINALIZE (1ull << 12)
/* Optional ABI15 service. Retired player in a race monitor: Leave Circuit
 * opens native simulated results, retaining field/UI until native ACCEPT.
 * Available without an ESC-menu client; no new API structure fields. */
#define F1C_BROKER_CAP_GARAGE_RESULTS (1ull << 14)
#define F1C_BROKER_CAP_SESSION_END (1ull << 11)
#define F1C_BROKER_CAP_CALLBACK_RECOVERY  (1ull << 10)
#define F1C_BROKER_NATIVE_CALLBACK_US 250000u

#define F1C_BROKER_CAP_PRELOAD            (1ull << 9)

typedef uint32_t F1CBrokerToken;
typedef void (__cdecl *F1CBrokerTaskFn)(void *context);

/* ABI 10 pause-menu commands. The broker owns the ESC key at RestartGate
   (0x004A0FD0, its ABI-5 observation site) and runs a per-frame pause-menu
   state machine in a pre-original section. Clients arm the capability and
   may queue one pending command per call; the broker applies commands on the
   game thread and emits F1C_BROKER_EVENT_PAUSE_MENU with value = command.
   Menu options differ by session type (race = RETIRE_TO_PITS, other in-car
   sessions = RETURN_TO_PITS) -- the command codes are identical, only the
   rendered label changes. */
typedef enum F1CBrokerMenuCommand {
    F1C_BROKER_MENU_NONE = 0,
    F1C_BROKER_MENU_OPEN,        /* open the pause menu, freeze the sim */
    F1C_BROKER_MENU_CONTINUE,    /* close the menu, resume the sim */
    F1C_BROKER_MENU_RETIRE_PITS, /* close+resume; player retires to garage */
    F1C_BROKER_MENU_EXIT_RACE,   /* close+resume; vanilla ESC behavior */
    F1C_BROKER_MENU_REPLAY,      /* close+resume; start instant replay */
    F1C_BROKER_MENU_CURSOR_UP,   /* move the menu cursor up (wrap) */
    F1C_BROKER_MENU_CURSOR_DOWN  /* move the menu cursor down (wrap) */
} F1CBrokerMenuCommand;

typedef enum F1CBrokerResult {
    F1C_BROKER_OK = 0,
    F1C_BROKER_NOT_READY,
    F1C_BROKER_BAD_ARGUMENT,
    F1C_BROKER_NO_CLIENT_SLOT,
    F1C_BROKER_STALE_TOKEN,
    F1C_BROKER_QUEUE_FULL,
    F1C_BROKER_CLIENT_QUOTA_REACHED,
    F1C_BROKER_WOULD_DEADLOCK,
    F1C_BROKER_TIMEOUT,
    F1C_BROKER_STOPPING,
    F1C_BROKER_STALE_SESSION,
    F1C_BROKER_BUFFER_TOO_SMALL,
    F1C_BROKER_STALE_OBJECT,
    F1C_BROKER_CAPABILITY_IN_USE
} F1CBrokerResult;

typedef struct F1CBrokerInfo {
    uint32_t structure_size;
    uint32_t abi_version;
    uint32_t min_compatible_abi;
    uint32_t max_vehicle_slots;
    uint64_t capabilities;
} F1CBrokerInfo;

typedef struct F1CBrokerStats {
    uint32_t abi_version;
    uint32_t structure_size;
    uint32_t hook_thread_id;
    uint32_t active_clients;
    uint32_t queued_tasks;
    uint32_t update_budget;
    uint32_t update_time_budget_us;
    int32_t last_race_phase;
    uint32_t observed_phase_mask;
    uint64_t hook_calls;
    uint64_t tasks_posted;
    uint64_t tasks_executed;
    uint64_t stale_tasks_discarded;
    uint64_t budget_exhaustions;
    uint64_t time_budget_exhaustions;
    uint64_t slow_client_quarantines;
    uint64_t phase_transitions;
    uint64_t maximum_callback_us;
    uint64_t semantic_commands_queued;
    uint64_t semantic_commands_executed;
    uint64_t semantic_commands_rejected_phase;
    uint64_t semantic_commands_rejected_vehicle;
    uint64_t semantic_pulses_executed;
    uint64_t semantic_pulses_restored;
    uint64_t semantic_pulses_changed_by_game;
    uint32_t session_generation;
    uint32_t session_active;
    uint64_t session_entries;
    uint64_t session_exits;
    uint64_t restart_gate_calls;
    uint64_t restart_gate_return_4;
    uint64_t restart_gate_return_6;
    uint64_t semantic_commands_rejected_session;
    uint64_t mini_restarts_requested;
    /* ABI 10: pause-menu capability counters (append-only suffix). */
    uint64_t pause_menu_opens;
    uint64_t pause_menu_closes;
    uint64_t pause_menu_commands;
    uint64_t pause_menu_swallowed_frames;
} F1CBrokerStats;

typedef struct F1CBrokerVehicleHandle {
    uint32_t session_generation;
    uint32_t object_generation;
    uint32_t slot;
    uint32_t reserved;
} F1CBrokerVehicleHandle;

#define F1C_VEH_SNAPSHOT_POSITION       (1ull << 0)
#define F1C_VEH_SNAPSHOT_CONTROLS       (1ull << 1)
#define F1C_VEH_SNAPSHOT_POWERTRAIN     (1ull << 2)
#define F1C_VEH_SNAPSHOT_TIMING         (1ull << 3)
#define F1C_VEH_SNAPSHOT_PIT            (1ull << 4)
#define F1C_VEH_SNAPSHOT_DAMAGE         (1ull << 5)
#define F1C_VEH_SNAPSHOT_IDENTITY       (1ull << 6)
#define F1C_VEH_SNAPSHOT_TEAM           (1ull << 7)
#define F1C_VEH_SNAPSHOT_TRACK_LIMITS   (1ull << 8)
#define F1C_VEH_SNAPSHOT_FIELD_STATUS   (1ull << 9)
#define F1C_VEH_SNAPSHOT_VISUAL_DAMAGE  (1ull << 10)
#define F1C_VEH_SNAPSHOT_ORIENTATION (1ull << 12)
#define F1C_VEH_SNAPSHOT_NAMES          (1ull << 11)

/* Pass to F1CBrokerSetPaceCap to release a held cap; the broker writes the
   game's own uncapped sentinel (99999, matches ParameterValueSetter's own
   "no cap" convention) once and stops re-arming the slot. */
#define F1C_PACE_CAP_CLEAR (-1)

typedef struct F1CBrokerVehicleSnapshot {
    uint32_t structure_size;
    uint32_t snapshot_version;
    uint64_t snapshot_sequence;
    F1CBrokerVehicleHandle handle;
    uint64_t valid_fields;
    int32_t race_phase;
    int32_t driver_id;
    float world_x;
    float world_y;
    float world_z;
    float speed;
    float throttle_factor;
    float brake_factor;
    float engine_rpm;
    float engine_max_rpm;
    int32_t current_gear;
    int32_t lap_number;
    int32_t race_position;
    float best_lap_time;
    float elapsed_time;
    int32_t pit_status;
    int32_t call_to_box;
    int32_t ai_control;
    int32_t finish_state;
    int32_t reason_code;
    int32_t car_conditions;
    float engine_life_remaining;
    float brakes_life_remaining;
    float gearbox_life_remaining;
    float suspension_life_remaining;
    /* ABI 8: FNV-1a hash of the roster team-name string at
       dword_8AC394 + 1060*driver_id + 124 (CONFIRMED read site, see
       functions/sub_417640_00417640.c). Zero when driver_id is out of the
       roster's bounds. Two cars sharing a team compare equal; the hash is
       opaque, not a team enum, so it needs no hardcoded team-name list and
       survives any livery/roster mod. */
    uint32_t team_id;
    /* ABI 9: track-limits state at veh+0x40E8 (sub_4BCCC0's this+0xB4,
       FACT, live Frida-verified 2026-07-28: byte transitioned 0->1->0
       exactly as the player ran wide off the racing surface and returned;
       findings.jsonl "off-track-byte-40e8"). Same byte the game's own
       cut-track penalty logic reads, so this is the game's own detector,
       not a derived heuristic. 0 = on track, 1 = off track. */
    int32_t off_track;
    /* ABI 11: neutral whole-field status. controller_role is the game's
       reviewed slot-role field: -1 inactive, 0 human, 1 AI, 2 remote.
       qualifying_position is the vehicle's planned grid position. */
    int32_t controller_role;
    int32_t retired;
    int32_t qualifying_position;
    int32_t driver_model_visible;
    /* Wing record presence distinguishes a missing/detached part from an
       intact flag that happens to be zero while no record exists. */
    int32_t front_wing_record_present;
    int32_t front_wing_intact;
    int32_t rear_wing_record_present;
    int32_t rear_wing_intact;
    float tire_puncture_rr;
    float tire_puncture_rl;
    float tire_puncture_fr;
    float tire_puncture_fl;
    char driver_name[64];
    char car_name[156];
    float orientation_up_dot, inverted_seconds;
    int32_t orientation_state, suspected_incapacitated;
    int32_t sc_inversion_reason,rf_inversion_reason;
} F1CBrokerVehicleSnapshot;

/* Vehicle snapshots were introduced in ABI 6. These prefix sizes let a new
   broker copy only what an older caller allocated instead of rejecting the
   caller or overflowing its structure. */
#define F1C_BROKER_VEHICLE_SNAPSHOT_ABI6_SIZE \
    ((uint32_t)offsetof(F1CBrokerVehicleSnapshot, team_id))
#define F1C_BROKER_VEHICLE_SNAPSHOT_ABI8_SIZE \
    ((uint32_t)offsetof(F1CBrokerVehicleSnapshot, off_track))
#define F1C_BROKER_VEHICLE_SNAPSHOT_ABI9_SIZE \
    ((uint32_t)offsetof(F1CBrokerVehicleSnapshot, controller_role))

#define F1C_SESSION_SNAPSHOT_CORE         (1ull << 0)
#define F1C_SESSION_SNAPSHOT_RACE_LENGTH  (1ull << 1)
#define F1C_SESSION_SNAPSHOT_ENVIRONMENT  (1ull << 2)

typedef struct F1CBrokerSessionSnapshot {
    uint32_t structure_size;
    uint32_t snapshot_version;
    uint64_t snapshot_sequence;
    uint64_t valid_fields;
    uint32_t session_generation;
    uint32_t session_active;
    int32_t session_type;
    int32_t race_phase;
    int32_t vehicle_count;
    int32_t race_active;
    int32_t total_race_laps;
    float race_time_threshold;
    float track_condition_state;
    float online_wetness;
    float offline_wetness;
    float ambient_temp_c;
    float track_temp_c;
    int32_t spectated_vehicle_slot;
} F1CBrokerSessionSnapshot;

typedef enum F1CBrokerEventType {
    F1C_BROKER_EVENT_SESSION_STARTED = 1,
    F1C_BROKER_EVENT_SESSION_ENDED,
    F1C_BROKER_EVENT_RACE_PHASE_CHANGED,
    F1C_BROKER_EVENT_VEHICLE_CREATED,
    F1C_BROKER_EVENT_VEHICLE_DESTROYED,
    /* ABI 10: emitted when a pause-menu command executes on the game thread.
       value = F1CBrokerMenuCommand; slot/object_generation unused. Clients use
       RETIRE_PITS to start their own retirement sequence (the broker applies
       only the game-thread half: menu close + unpause). */
    F1C_BROKER_EVENT_PAUSE_MENU
} F1CBrokerEventType;

typedef struct F1CBrokerEvent {
    uint32_t structure_size;
    uint32_t type;
    uint64_t sequence;
    uint64_t update_sequence;
    uint32_t session_generation;
    uint32_t object_generation;
    uint32_t vehicle_slot;
    int32_t value;
} F1CBrokerEvent;

typedef F1CBrokerResult (__cdecl *F1CBrokerRegisterFn)(HMODULE, const char *, F1CBrokerToken *);
typedef F1CBrokerResult (__cdecl *F1CBrokerUnregisterFn)(F1CBrokerToken, DWORD);
typedef F1CBrokerResult (__cdecl *F1CBrokerPostFn)(F1CBrokerToken, F1CBrokerTaskFn, void *);
typedef F1CBrokerResult (__cdecl *F1CBrokerGetStatsFn)(F1CBrokerStats *);
typedef F1CBrokerResult (__cdecl *F1CBrokerSetCallToBoxFn)(F1CBrokerToken, uint32_t, uint32_t);
typedef F1CBrokerResult (__cdecl *F1CBrokerPulseCallToBoxFn)(F1CBrokerToken, uint32_t, uint32_t);
typedef F1CBrokerResult (__cdecl *F1CBrokerRequestMiniRestartFn)(F1CBrokerToken, uint32_t);
typedef F1CBrokerResult (__cdecl *F1CBrokerGetInfoFn)(F1CBrokerInfo *);
typedef F1CBrokerResult (__cdecl *F1CBrokerGetVehicleHandlesFn)(F1CBrokerToken, uint32_t, F1CBrokerVehicleHandle *, uint32_t, uint32_t *);
typedef F1CBrokerResult (__cdecl *F1CBrokerGetVehicleSnapshotFn)(F1CBrokerToken, F1CBrokerVehicleHandle, F1CBrokerVehicleSnapshot *);
typedef F1CBrokerResult (__cdecl *F1CBrokerGetSessionSnapshotFn)(F1CBrokerToken, uint32_t, F1CBrokerSessionSnapshot *);
typedef F1CBrokerResult (__cdecl *F1CBrokerPollEventsFn)(F1CBrokerToken, uint64_t, F1CBrokerEvent *, uint32_t, uint32_t *, uint64_t *, uint64_t *, uint64_t *);
typedef F1CBrokerResult (__cdecl *F1CBrokerSetPaceCapFn)(F1CBrokerToken, uint32_t, int32_t);
/* pause_menu_arm values: 0 off, 1 on (freezes the sim), 2 on without pause
   (multiplayer; added 2026-10-10, older brokers return BAD_ARGUMENT). */
#define F1C_BROKER_PAUSE_MENU_ARM_NO_FREEZE 2u
typedef F1CBrokerResult (__cdecl *F1CBrokerPauseMenuArmFn)(F1CBrokerToken, uint32_t);
typedef F1CBrokerResult (__cdecl *F1CBrokerPauseMenuCommandFn)(F1CBrokerToken, F1CBrokerMenuCommand);
/* ABI 12: canonical-target-only stabilization for the engine's shared ratio
   slot used by forward gears above seven. ratio==0 releases ownership. */
typedef F1CBrokerResult (__cdecl *F1CBrokerSetHighGearRatioFn)(F1CBrokerToken, double);
/* ABI 13: synchronous gate immediately before the broker calls the game's
   already-owned RestartGate trampoline. A guard is invoked only when the
   vanilla mini-restart flag is armed. Returning BLOCK clears that flag and
   cancels the restart; returning ALLOW leaves vanilla behavior unchanged.
   Broker revision 2026-09-08 supports one guard per client. All snapshotted
   guards run in client-slot order; any non-ALLOW cancels. NULL clears only
   the caller's guard. Clearing/replacing affects subsequent snapshots; keep
   callback context alive until unregister completes or synchronize on the
   game thread. Older ABI-13 brokers may return CAPABILITY_IN_USE. */
typedef enum F1CBrokerRestartDecision {
    F1C_BROKER_RESTART_ALLOW = 0,
    F1C_BROKER_RESTART_BLOCK = 1
} F1CBrokerRestartDecision;
typedef F1CBrokerRestartDecision (__cdecl *F1CBrokerRestartGuardFn)(
    void *, uint32_t, int32_t);
typedef F1CBrokerResult (__cdecl *F1CBrokerSetRestartGuardFn)(
    F1CBrokerToken, F1CBrokerRestartGuardFn, void *);

/* ABI 14: synchronous initialization before native vehicle construction.
   Runs on the native DynMan_Init caller, which need not be the update thread.
   Initialization only: no current-field assumptions, queued tasks or forced
   vehicle creation. Register before starting a weekend. No replay for a load
   already in progress. NULL clears this client's callback; a retained snapshot
   may run once more. Keep context alive until unregister has completed.
   Unregister within preload dispatch returns WOULD_DEADLOCK for reserved clients.
   Available only on the reviewed canonical target. */
typedef F1CBrokerResult (__cdecl *F1CBrokerSetPreloadFn)(
    F1CBrokerToken, F1CBrokerTaskFn, void *);

#ifdef F1C_BROKER_BUILD
#define F1C_BROKER_EXPORT __declspec(dllexport)
#else
#define F1C_BROKER_EXPORT __declspec(dllimport)
#endif

F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerRegister(HMODULE, const char *, F1CBrokerToken *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerUnregister(F1CBrokerToken, DWORD);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerPost(F1CBrokerToken, F1CBrokerTaskFn, void *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerGetStats(F1CBrokerStats *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerSetCallToBox(F1CBrokerToken, uint32_t, uint32_t);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerPulseCallToBox(F1CBrokerToken, uint32_t, uint32_t);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerRequestMiniRestart(F1CBrokerToken, uint32_t);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerGetInfo(F1CBrokerInfo *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerGetVehicleHandles(F1CBrokerToken, uint32_t, F1CBrokerVehicleHandle *, uint32_t, uint32_t *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerGetVehicleSnapshot(F1CBrokerToken, F1CBrokerVehicleHandle, F1CBrokerVehicleSnapshot *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerGetSessionSnapshot(F1CBrokerToken, uint32_t, F1CBrokerSessionSnapshot *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerPollEvents(F1CBrokerToken, uint64_t, F1CBrokerEvent *, uint32_t, uint32_t *, uint64_t *, uint64_t *, uint64_t *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerSetPaceCap(F1CBrokerToken, uint32_t, int32_t);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerPauseMenuArm(F1CBrokerToken, uint32_t);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerPauseMenuCommand(F1CBrokerToken, F1CBrokerMenuCommand);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerSetHighGearRatio(F1CBrokerToken, double);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerSetRestartGuard(F1CBrokerToken, F1CBrokerRestartGuardFn, void *);
F1C_BROKER_EXPORT F1CBrokerResult __cdecl F1CBrokerSetPreload(F1CBrokerToken, F1CBrokerTaskFn, void *);

#undef F1C_BROKER_EXPORT
