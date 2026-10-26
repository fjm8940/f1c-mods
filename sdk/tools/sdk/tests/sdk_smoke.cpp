#include <cstddef>
#include "f1c_vehicle_struct.h"
#include "f1c_sdk.hpp"
#include "f1c_runtime.h"
#include "f1c_hook.h"
#include "f1c_trace.h"
#include "f1c_reviewed_api.hpp"

static_assert(sizeof(F1CVehicle) == 0x48C0, "vehicle stride drift");
static_assert(offsetof(F1CVehicle, RearWingIntactFlag) == 0x0D58, "rear wing offset drift");
static_assert(sizeof(F1CVehicle::RearWingIntactFlag) == 1, "rear wing must be byte-wide");
static_assert(offsetof(F1CVehicle, PitStatus) == 0x3450, "pit status offset drift");
static_assert(offsetof(F1CVehicle, PitStopEndTime) == 0x3470, "stop deadline drift");
static_assert(offsetof(F1CVehicle, PitStopSchedule0) == 0x4590, "schedule offset drift");
static_assert(sizeof(F1CPitMachine) == 0x1D8, "pit-machine layout drift");
static_assert(offsetof(F1CPitMachine, refuel_active) == 0x1C0, "refuel flag drift");
static_assert(offsetof(F1CPitMachine, tire_change_active) == 0x1C4, "tire flag drift");

int sdk_cpp_smoke(void *vehicle)
{
    f1c::VehicleRef car(vehicle);
    return static_cast<int>(car.pit().status());
}
