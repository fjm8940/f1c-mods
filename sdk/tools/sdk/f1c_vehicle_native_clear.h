#ifndef F1C_VEHICLE_NATIVE_CLEAR_H
#define F1C_VEHICLE_NATIVE_CLEAR_H
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "f1c_vehicle_lifecycle.h"

/* Native sub_4C6E80, used by ProcessDriverEntries when an existing vehicle
 * is replaced or removed. It releases the vehicle's scene/physics/UI state
 * and VehicleDriverInfoClear sets roster index +0x4898 to -1. The caller must
 * own a dormant vehicle outside the active array on the reviewed game thread.
 * This does not alter the active count or authorize a new vehicle insertion. */
typedef int (__attribute__((thiscall)) *F1CVehicleNativeClearFn)(void *);
static int f1c_vehicle_native_clear_dormant_at(F1CLifecycleRead rd,
        uintptr_t vehicle, int expected_roster, DWORD game_thread,
        int authorized, const void *site, F1CVehicleNativeClearFn native) {
    static const unsigned char entry[] = {
        0x56,0x8b,0xf1,0x57,0x8d,0x7e,0x0c,0x57,0xb9,0x60,0xa5,0x5e,
        0x00,0xe8,0xae,0x9c,0xf4,0xff
    };
    int role, roster;
    if (!authorized || !rd || !game_thread || GetCurrentThreadId()!=game_thread ||
        !vehicle || vehicle>UINT32_MAX-0x48c0u || expected_roster<0 ||
        !site || !native || IsBadReadPtr(site,sizeof(entry)) ||
        memcmp(site,entry,sizeof(entry)) ||
        IsBadWritePtr((void*)vehicle,0x48c0u) ||
        !f1c_lifecycle_read_at(rd,vehicle+0x32c8u,&role,4) || role!=-1 ||
        !f1c_lifecycle_read_at(rd,vehicle+0x4898u,&roster,4) ||
        roster!=expected_roster) return 0;
    native((void*)vehicle);
    return f1c_lifecycle_read_at(rd,vehicle+0x4898u,&roster,4) &&
           roster==-1 &&
           f1c_lifecycle_read_at(rd,vehicle+0x32c8u,&role,4) && role==-1;
}
static int f1c_vehicle_native_clear_dormant(F1CLifecycleRead rd,
        uintptr_t vehicle, int expected_roster, DWORD game_thread,
        int authorized) {
    const void *site=(const void*)(uintptr_t)0x004C6E80u;
    return f1c_vehicle_native_clear_dormant_at(rd,vehicle,expected_roster,
        game_thread,authorized,site,(F1CVehicleNativeClearFn)site);
}
#endif
