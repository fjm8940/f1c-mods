#include <stddef.h>
#include "f1c_vehicle_struct.h"
#include "f1c_pit.h"
#include "f1c_runtime.h"
#include "f1c_hook.h"
#include "f1c_trace.h"

#define SDK_C_ASSERT(name, expr) typedef char assert_##name[(expr) ? 1 : -1]

SDK_C_ASSERT(vehicle_size, sizeof(struct F1CVehicle) == 0x48C0);
SDK_C_ASSERT(rear_wing_width, sizeof(((struct F1CVehicle*)0)->RearWingIntactFlag) == 1);
SDK_C_ASSERT(rear_wing_offset, offsetof(struct F1CVehicle, RearWingIntactFlag) == 0x0D58);
SDK_C_ASSERT(pit_status_offset, offsetof(struct F1CVehicle, PitStatus) == 0x3450);
SDK_C_ASSERT(service_latch_width, sizeof(((struct F1CVehicle*)0)->PitServiceReadyLatch) == 1);
SDK_C_ASSERT(stop_end_offset, offsetof(struct F1CVehicle, PitStopEndTime) == 0x3470);
SDK_C_ASSERT(schedule_offset, offsetof(struct F1CVehicle, PitStopSchedule0) == 0x4590);
SDK_C_ASSERT(pit_machine_size, sizeof(struct F1CPitMachine) == 0x1D8);
SDK_C_ASSERT(pit_machine_status, offsetof(struct F1CPitMachine, status) == 0x184);
SDK_C_ASSERT(pit_state_event_time, offsetof(struct F1CPitMachine, state_event_time) == 0x188);
SDK_C_ASSERT(pit_refuel_flag, offsetof(struct F1CPitMachine, refuel_active) == 0x1C0);
SDK_C_ASSERT(pit_tire_flag, offsetof(struct F1CPitMachine, tire_change_active) == 0x1C4);
SDK_C_ASSERT(pit_repair_flag, offsetof(struct F1CPitMachine, repair_active) == 0x1CC);
SDK_C_ASSERT(target_is_x86, F1C_TARGET_MACHINE == 0x14C);
SDK_C_ASSERT(trace_has_fields, F1C_TRACE_FIELD_COUNT >= 80);

int sdk_c_smoke(void *vehicle)
{
    if (f1c_pit_service_active(vehicle))
        return f1c_pit_hold_after_service(vehicle, 100.0f, 3.0f);
    return (int)f1c_pit_status(vehicle);
}
