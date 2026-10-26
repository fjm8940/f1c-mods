#ifndef F1C_AI_ENGINE_STOP_H
#define F1C_AI_ENGINE_STOP_H
#include <windows.h>
#include "f1c_vehicle_lifecycle.h"
/* Exact native ConditionalGate 004EFAC0, object=vehicle+12, thiscall ret0.
   AIMainUpdate calls this operation for engine shutdown. Reviewed AI branch
   writes object+3994 (vehicle EngineRPM+39A0) to zero. Never enter player branch.
   Full31-byte executable fingerprint includes native player tail jump. */
typedef int (__attribute__((thiscall)) *F1CAIEngineStopFn)(void*);
static int f1c_ai_engine_stop_at(F1CLifecycleRead rd,uintptr_t vehicle,DWORD thread,
                               int authorized,F1CAIEngineStopFn native) {
    static const unsigned char bytes[]={0x8b,0x81,0xbc,0x32,0,0,0x85,0xc0,0x75,0x0a,
        0xb9,0x28,0x74,0x7c,0,0xe9,0x9c,0x77,0xf5,0xff,
        0xc7,0x81,0x94,0x39,0,0,0,0,0,0,0xc3};
    int role,finish;unsigned char retired;float rpm;
    if(!authorized||!thread||GetCurrentThreadId()!=thread||!native||!vehicle||
       vehicle>UINT32_MAX-0x48c0u)return 0;
    if(!f1c_lifecycle_read_at(rd,vehicle+0x32c8,&role,4)||role!=1||
       !f1c_lifecycle_read_at(rd,vehicle+0x4064,&finish,4)||finish!=0||
       !f1c_lifecycle_read_at(rd,vehicle+0x4410,&retired,1)||retired||
       !f1c_lifecycle_read_at(rd,vehicle+0x39a0,&rpm,4)||!isfinite(rpm)||rpm<0||
       IsBadWritePtr((void*)(vehicle+0x39a0),4)||IsBadReadPtr((void*)native,sizeof(bytes))||
       memcmp((void*)native,bytes,sizeof(bytes)))return 0;
    if(rpm==0)return 1;
    native((void*)(vehicle+12));
    return *(volatile float*)(vehicle+0x39a0)==0;
}
static int f1c_ai_engine_stop(F1CLifecycleRead rd,uintptr_t vehicle,DWORD thread,int authorized) {
    return f1c_ai_engine_stop_at(rd,vehicle,thread,authorized,(F1CAIEngineStopFn)0x004efac0u);
}
#endif
