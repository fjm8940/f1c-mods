/*
 * f1c_pit.h - reviewed, semantic pit/AI interface for F1 Challenge 99-02.
 *
 * Unlike the generated field catalogue, this file exposes operations.  State
 * machine outputs are read-only by convention; commands modify only inputs
 * with live evidence that the game consumes them organically.
 */
#pragma once

#include "f1c_sdk.h"
#include "f1c_pit_struct.h"

#if defined(_MSC_VER)
#define F1C_INLINE static __inline
#else
#define F1C_INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct F1CVehicle;

#if defined(_MSC_VER) && defined(__cplusplus)
#define F1C_THISCALL __thiscall
#elif defined(__GNUC__) && defined(__i386__)
#define F1C_THISCALL __attribute__((thiscall))
#else
#define F1C_THISCALL
#endif

typedef enum F1CPitStatus {
    F1C_PIT_TRACK    = 0,
    F1C_PIT_CALLED   = 1,
    F1C_PIT_LANE     = 2,
    F1C_PIT_SERVICE  = 3,
    F1C_PIT_EXIT     = 4
} F1CPitStatus;

enum {
    F1C_PIT_MACHINE_OFFSET = 0x32CC,
    F1C_PIT_SCHEDULE_COUNT = 4,
    F1C_PIT_SCHEDULE_STRIDE = 16
};

F1C_INLINE F1CPitStatus f1c_pit_status(const void *vehicle)
{
    return (F1CPitStatus)VEHF_PitStatus((void*)vehicle);
}

F1C_INLINE int f1c_pit_service_ready(const void *vehicle)
{
    return VEHF_PitServiceReadyLatch((void*)vehicle) != 0;
}

F1C_INLINE int f1c_pit_service_active(const void *vehicle)
{
    return f1c_pit_status(vehicle) == F1C_PIT_SERVICE &&
           f1c_pit_service_ready(vehicle);
}

F1C_INLINE struct F1CPitMachine *f1c_pit_machine(void *vehicle)
{
    return (struct F1CPitMachine*)((uint8_t*)vehicle + F1C_PIT_MACHINE_OFFSET);
}

/* Ordinary service call.  The game evaluates one of four strategy records;
 * populate all four so the command is independent of the current index. */
F1C_INLINE void f1c_pit_request_organic_stop(void *vehicle, int32_t scheduled_lap)
{
    int i;
    uint8_t *v = (uint8_t*)vehicle;
    for (i = 0; i < F1C_PIT_SCHEDULE_COUNT; ++i)
        *(int32_t*)(v + F1C_OFF_PitStopSchedule0 +
                    i * F1C_PIT_SCHEDULE_STRIDE) = scheduled_lap;
    VEHF_PitDecisionTimer(vehicle) = 0;
}

/* pace_probe's game-owned garage path.  This is intentionally distinct from
 * an ordinary service request: the rear-wing condition makes the game's own
 * blocking/retirement path garage the vehicle after it drives in. */
F1C_INLINE void f1c_pit_request_game_owned_garage(void *vehicle)
{
    VEHF_RearWingIntactFlag(vehicle) = 0;
    VEHF_CallToBox(vehicle) = 1;
}

/* Fail-safe hold.  Call only after the game has entered SERVICE and latched
 * serviceReady; stopping the caller then releases the car within lead_seconds. */
F1C_INLINE int f1c_pit_hold_after_service(void *vehicle,
                                          float session_time,
                                          float lead_seconds)
{
    if (!f1c_pit_service_active(vehicle)) return 0;
    if (lead_seconds < 0.1f) lead_seconds = 0.1f;
    VEHF_PitStopEndTime(vehicle) = session_time + lead_seconds;
    return 1;
}

/* Reviewed game ABI. Calls that mutate simulation state belong on the game
 * thread unless the individual function is independently proven leaf-safe.
 * MSVC exposes __thiscall only in C++; its C mode intentionally receives the
 * semantic helpers and addresses, but no incorrectly-conventioned typedefs. */
#if !defined(_MSC_VER) || defined(__cplusplus)
typedef void (F1C_THISCALL *F1CPitStopReadyCheckFn)(
    struct F1CPitMachine *pit, float session_time);
typedef uint8_t (F1C_THISCALL *F1CPitStopProgressFn)(
    struct F1CPitMachine *pit, float session_time);
typedef void (F1C_THISCALL *F1CPitStateUpdateMachineFn)(
    struct F1CPitMachine *pit, float session_time);
typedef uint8_t (F1C_THISCALL *F1CPitStopDecisionFn)(
    struct F1CVehicle *vehicle);
typedef void (F1C_THISCALL *F1CAICommandDispatcherFn)(
    struct F1CVehicle *vehicle, int32_t command, int32_t value);
typedef void (F1C_THISCALL *F1CParameterValueSetterFn)(
    struct F1CVehicle *vehicle, int32_t value);
#endif

#ifdef __cplusplus
}
#endif

#undef F1C_INLINE
#undef F1C_THISCALL
