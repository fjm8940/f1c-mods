// f1c_sdk.hpp - source-like C++ facade over the canonical C SDK.
#pragma once

#include "f1c_pit.h"

namespace f1c {

enum class PitStatus : int32_t {
    track   = F1C_PIT_TRACK,
    called  = F1C_PIT_CALLED,
    lane    = F1C_PIT_LANE,
    service = F1C_PIT_SERVICE,
    exit    = F1C_PIT_EXIT
};

class PitRef {
public:
    explicit PitRef(void *vehicle) : vehicle_(vehicle) {}

    PitStatus status() const
    {
        return static_cast<PitStatus>(f1c_pit_status(vehicle_));
    }

    bool serviceReady() const { return f1c_pit_service_ready(vehicle_) != 0; }
    bool serviceActive() const { return f1c_pit_service_active(vehicle_) != 0; }

    void requestOrganicStop(int32_t scheduledLap = -1)
    {
        f1c_pit_request_organic_stop(vehicle_, scheduledLap);
    }

    void requestGameOwnedGarage()
    {
        f1c_pit_request_game_owned_garage(vehicle_);
    }

    bool holdAfterService(float sessionTime, float leadSeconds = 3.0f)
    {
        return f1c_pit_hold_after_service(vehicle_, sessionTime, leadSeconds) != 0;
    }

private:
    void *vehicle_;
};

class VehicleRef {
public:
    explicit VehicleRef(void *vehicle) : vehicle_(vehicle) {}

    static VehicleRef fromSlot(int slot) { return VehicleRef(VEH(slot)); }
    void *raw() const { return vehicle_; }
    PitRef pit() const { return PitRef(vehicle_); }
    float speed() const { return VEHF_Speed(vehicle_); }
    int32_t lap() const { return VEHF_lapNumber(vehicle_); }
    bool retired() const { return VEHF_RetiredFlag(vehicle_) != 0; }

private:
    void *vehicle_;
};

namespace abi {
constexpr uintptr_t BuildActiveVehiclePointerArray =
    F1C_FN_BuildActiveVehiclePointerArray;
constexpr uintptr_t PitStopBlockingCondition = F1C_FN_PitStopBlockingCondition;
constexpr uintptr_t PitStopProgress = F1C_FN_PitStopProgress;
constexpr uintptr_t PitStopReadyCheck = F1C_FN_PitStopReadyCheck;
constexpr uintptr_t PitStateUpdateMachine = F1C_FN_PitStateUpdateMachine;
} // namespace abi

} // namespace f1c

