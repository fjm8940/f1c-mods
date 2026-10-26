#ifndef F1C_RESTART_FUEL_H
#define F1C_RESTART_FUEL_H
/* Restart service primitive. Static ABI/bytes reviewed 2026-09-21; native
 * acceptance pending. Capacity is the model's FuelRange upper endpoint,
 * initialized by 004E5F60 at body+14716. Native 004F1890 also updates the
 * separate player-physics fuel mirror. A vehicle-only float write does not.
 * Caller owns exact identity, writable extent, offline session and game thread.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
enum { F1C_RESTART_FUEL_ADDRESS=0x004F1890u,
       F1C_RESTART_FUEL_BODY_OFFSET=12u,
       F1C_RESTART_FUEL_LEVEL_OFFSET=0x3984u,
       F1C_RESTART_FUEL_CAPACITY_OFFSET=0x3988u };
typedef void (__attribute__((thiscall)) *F1CRestartFuelSet)(void*,float);
static int f1c_restart_fuel_code_valid(const void *code,size_t bytes) {
    static const uint8_t expected[]={
        0x8b,0x44,0x24,0x04,0x89,0x81,0x78,0x39,0x00,0x00,
        0x8b,0x81,0xbc,0x32,0x00,0x00,0x85,0xc0,0x75,0x0a,
        0xd9,0x44,0x24,0x04,0xdd,0x1d,0xc4,0x75,0x7c,0x00,
        0xc2,0x04,0x00};
    return code && bytes>=sizeof expected && !memcmp(code,expected,sizeof expected);
}
static int f1c_restart_fuel_value_valid(const void *vehicle,size_t bytes,float fuel) {
    float capacity;
    if(!vehicle || bytes<F1C_RESTART_FUEL_CAPACITY_OFFSET+sizeof capacity ||
       !isfinite(fuel) || fuel<0)return 0;
    memcpy(&capacity,(const uint8_t*)vehicle+F1C_RESTART_FUEL_CAPACITY_OFFSET,sizeof capacity);
    return isfinite(capacity) && capacity>0 && capacity<=1000 && fuel<=capacity;
}
/* Injectable native leaf enables tests of the actual argument/receiver contract.
 * Production passes only the reviewed address and exact bytes, after preflight.
 */
static int f1c_restart_fuel_target(const void *vehicle,size_t bytes,int refuel,float *out) {
    float target;
    if(!vehicle || !out || bytes<F1C_RESTART_FUEL_CAPACITY_OFFSET+sizeof target)return 0;
    memcpy(&target,(const uint8_t*)vehicle+(refuel?F1C_RESTART_FUEL_CAPACITY_OFFSET:
           F1C_RESTART_FUEL_LEVEL_OFFSET),sizeof target);
    if(!f1c_restart_fuel_value_valid(vehicle,bytes,target))return 0;
    *out=target;return 1;
}
static int f1c_restart_fuel_apply(void *vehicle,size_t bytes,float fuel,
        const void *code,size_t code_bytes,F1CRestartFuelSet setter) {
    if(!setter || !f1c_restart_fuel_code_valid(code,code_bytes) ||
       !f1c_restart_fuel_value_valid(vehicle,bytes,fuel))return 0;
    setter((uint8_t*)vehicle+F1C_RESTART_FUEL_BODY_OFFSET,fuel);
    return 1;
}
#endif
