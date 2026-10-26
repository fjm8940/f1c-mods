#pragma once

#include "f1c_broker_abi.h"

typedef enum F1CBrokerDiscoveryResult {
    F1C_BROKER_DISCOVERY_OK = 0,
    F1C_BROKER_DISCOVERY_BAD_ARGUMENT,
    F1C_BROKER_DISCOVERY_NOT_FOUND,
    F1C_BROKER_DISCOVERY_DUPLICATE,
    F1C_BROKER_DISCOVERY_QUERY_FAILED,
    F1C_BROKER_DISCOVERY_INCOMPATIBLE_ABI,
    F1C_BROKER_DISCOVERY_MISSING_CAPABILITY,
    F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS
} F1CBrokerDiscoveryResult;

typedef struct F1CBrokerApi {
    uint32_t structure_size;
    uint32_t negotiated_abi;
    uint64_t capabilities;
    HMODULE module;
    F1CBrokerRegisterFn register_client;
    F1CBrokerUnregisterFn unregister_client;
    F1CBrokerPostFn post;
    F1CBrokerGetStatsFn get_stats;
    F1CBrokerSetCallToBoxFn set_call_to_box;
    F1CBrokerPulseCallToBoxFn pulse_call_to_box;
    F1CBrokerRequestMiniRestartFn request_mini_restart;
    F1CBrokerGetInfoFn get_info;
    F1CBrokerGetVehicleHandlesFn get_vehicle_handles;
    F1CBrokerGetVehicleSnapshotFn get_vehicle_snapshot;
    F1CBrokerPollEventsFn poll_events;
    F1CBrokerGetSessionSnapshotFn get_session_snapshot;
    F1CBrokerSetPaceCapFn set_pace_cap;
    /* ABI 10: pause-menu capability (append-only suffix). */
    F1CBrokerPauseMenuArmFn pause_menu_arm;
    F1CBrokerPauseMenuCommandFn pause_menu_command;
    /* ABI 12: shared high-gear ratio stabilization. */
    F1CBrokerSetHighGearRatioFn set_high_gear_ratio;
    /* ABI 13: synchronous pre-restart guard. */
    F1CBrokerSetRestartGuardFn set_restart_guard;
    /* ABI 14: native vehicle-load initialization. */
    F1CBrokerSetPreloadFn set_preload;
} F1CBrokerApi;

/* ABI 7 is append-only: this exact prefix is the complete ABI 6 table. */
#define F1C_BROKER_API_ABI6_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, get_session_snapshot))
/* ABI 8 is append-only over ABI 7: this exact prefix is the complete ABI 7
   table (get_session_snapshot included, set_pace_cap not). */
#define F1C_BROKER_API_ABI7_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, set_pace_cap))
/* ABI 10 is append-only over ABI 8/9: this exact prefix is the complete ABI 9
   table (set_pace_cap included, pause-menu pointers not). */
#define F1C_BROKER_API_ABI9_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, pause_menu_arm))
#define F1C_BROKER_API_ABI11_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, set_high_gear_ratio))
#define F1C_BROKER_API_ABI12_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, set_restart_guard))
#define F1C_BROKER_API_ABI13_SIZE \
    ((uint32_t)offsetof(F1CBrokerApi, set_preload))

F1CBrokerDiscoveryResult f1c_broker_discover(
    uint32_t client_min_abi, uint32_t client_max_abi,
    uint64_t required_capabilities, F1CBrokerApi *api_out);

/* A successful discovery pins the broker module. Release exactly once after
   every thread has stopped calling through the table. Callers must unregister
   first; release is not concurrent-call synchronization. */
void f1c_broker_release(F1CBrokerApi *api);
