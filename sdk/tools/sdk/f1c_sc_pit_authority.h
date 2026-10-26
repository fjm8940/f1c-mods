#ifndef F1C_SC_PIT_AUTHORITY_H
#define F1C_SC_PIT_AUTHORITY_H
#include <math.h>
#pragma push_macro("VEH")
#pragma push_macro("VEH_BASE")
#pragma push_macro("VEH_STRIDE")
#pragma push_macro("VEH_SLOTS")
#pragma push_macro("VEH_ACTIVE")
#pragma push_macro("VEH_PLAYER")
#pragma push_macro("VEH_PTR_ARRAY")
#undef VEH
#undef VEH_BASE
#undef VEH_STRIDE
#undef VEH_SLOTS
#undef VEH_ACTIVE
#undef VEH_PLAYER
#undef VEH_PTR_ARRAY
#include "f1c_hook.h"
#pragma pop_macro("VEH_PTR_ARRAY")
#pragma pop_macro("VEH_PLAYER")
#pragma pop_macro("VEH_ACTIVE")
#pragma pop_macro("VEH_SLOTS")
#pragma pop_macro("VEH_STRIDE")
#pragma pop_macro("VEH_BASE")
#pragma pop_macro("VEH")
/* Reviewed native AIStateFlagSetter 0042AC70. First seven bytes are whole
   instructions: mov eax,[esp+4]; cmp eax,2. No PC-relative stolen operands.
   Exact target code: 8b44240483f8027d0a85c07c0689814c450000c20400.
   CallToBox native setter is intercepted BEFORE AIRacingLine consumes it.
   Unfiltered calls always use original native code/return/stack semantics. */
typedef int (*F1CSCPitBlock)(uintptr_t vehicle,unsigned request);
typedef unsigned (__attribute__((thiscall)) *F1CNativePitSetter)(void*,unsigned);
static F1CDetour f1c_sc_pit_hook;
static F1CSCPitBlock f1c_sc_pit_block;
static unsigned __attribute__((fastcall)) f1c_sc_pit_detour(void *vehicle,void *unused,unsigned request){
    (void)unused;
    if(f1c_sc_pit_block && f1c_sc_pit_block((uintptr_t)vehicle,request))return request;
    return ((F1CNativePitSetter)f1c_sc_pit_hook.trampoline)(vehicle,request);
}
static int f1c_sc_pit_install(F1CSCPitBlock block,DWORD thread,int layout_verified){
    static const unsigned char code[]={0x8b,0x44,0x24,0x04,0x83,0xf8,0x02,0x7d,0x0a,0x85,0xc0,0x7c,0x06,0x89,0x81,0x4c,0x45,0,0,0xc2,4,0};
    if(!layout_verified || !thread || GetCurrentThreadId()!=thread || !block)return 0;
    if(f1c_sc_pit_hook.installed)return f1c_sc_pit_block==block;
    if(memcmp((void*)0x0042ac70u,code,sizeof(code)))return 0;
    f1c_sc_pit_block=block;
    return f1c_detour_install_prevalidated_layout(&f1c_sc_pit_hook,(void*)0x0042ac70u,
        (void*)f1c_sc_pit_detour,code,7)==F1C_DETOUR_OK;
}
static int f1c_sc_pit_remove(DWORD thread){
    if(!thread || GetCurrentThreadId()!=thread)return 0;
    f1c_sc_pit_block=NULL;
    return !f1c_sc_pit_hook.installed || f1c_detour_remove(&f1c_sc_pit_hook)==F1C_DETOUR_OK;
}
/* Authorized owned SC deployment only. FuelLevel is the reviewed amount at
   +3984; refill once before release, never during normal running. */
static int f1c_sc_fuel_fill(uintptr_t vehicle,DWORD thread,int authorized){
    float current;
    if(!authorized || !thread || GetCurrentThreadId()!=thread || !vehicle ||
       vehicle>UINT32_MAX-0x48c0u || IsBadReadPtr((void*)(vehicle+0x3984),4) ||
       IsBadWritePtr((void*)(vehicle+0x3984),4))return 0;
    memcpy(&current,(void*)(vehicle+0x3984),4);
    if(!isfinite(current) || current<0 || current>1000)return 0;
    *(volatile float*)(vehicle+0x3984)=135.0f;
    return 1;
}
#endif
