#ifndef F1C_NATIVE_REMOVAL_REPAIR_H
#define F1C_NATIVE_REMOVAL_REPAIR_H
#include <windows.h>
#include "f1c_vehicle_lifecycle.h"
/* Exact reviewed F1C only. Evidence: red_flag/trials/2026-08-19/
   VEHICLE_REMOVAL_COMPLETE_FAILURE_TRACE.md and SC56 live Trulli capture.
   Complete the native expiration of an already expired AI pit trigger; never
   change removal thresholds, finish/retirement flags, physics or coordinates.
   Caller must hold SC race-control authority in a settled offline session and
   exclude player/SC/pit-owned vehicles. Successful expiry is a completed native
   state transition, not a temporary loan: do not restore an expired command. */
static int f1c_native_removal_repair(F1CLifecycleRead rd,uintptr_t vehicle,
                                    DWORD game_thread,int authorized) {
    int role=0,finish=0,active=0,condition=0,reason=0,pitting=0,pitstate=0;
    unsigned char retired=0;uint32_t trigger_bits=0,flags=0;
    float trigger=0,now=0,speed=0;
    if(!authorized || !game_thread || GetCurrentThreadId()!=game_thread ||
       !vehicle || vehicle>UINT32_MAX-0x48C0u)return 0;
#define SC_RR(off,v) if(!f1c_lifecycle_read_at(rd,vehicle+(off),&(v),sizeof(v)))return 0
    SC_RR(0x32C8,role);SC_RR(0x4064,finish);SC_RR(0x4410,retired);
    SC_RR(0x45F4,active);SC_RR(0x3D6C,condition);SC_RR(0x4068,reason);
    SC_RR(0x3450,pitting);SC_RR(0x4040,pitstate);SC_RR(0x48BC,flags);
    SC_RR(0x4170,trigger_bits);SC_RR(0x4480,now);SC_RR(0x4440,speed);
#undef SC_RR
    memcpy(&trigger,&trigger_bits,4);
    /* Narrow repair of the observed complete wheel-loss failure only. Other
       failures/pit states require separate evidence, not a broad DNF guess. */
    if(role!=1 || finish!=0 || retired || active!=0 || condition!=2 || reason!=6 ||
       pitting!=0 || pitstate!=0 || !(flags&8u) ||
       !isfinite(speed) || speed<0 || speed>=0.5f ||
       !isfinite(now) || !isfinite(trigger) || trigger<0 || trigger>=now ||
       trigger_bits==0x7f7fffffu)return 0;
    volatile LONG *address=(volatile LONG*)(vehicle+0x4170);
    if(IsBadWritePtr((void*)address,4))return 0;
    return (uint32_t)InterlockedCompareExchange(address,(LONG)0x7f7fffffu,
                                               (LONG)trigger_bits)==trigger_bits;
}
#endif
