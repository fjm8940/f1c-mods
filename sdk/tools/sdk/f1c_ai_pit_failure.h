#pragma once
/* Exact-target game-thread AI pit mechanics. See AI_PIT_FAILURE_REVIEW.md.
 * Native calls own engine and retirement transitions; no player/session writes. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "f1c_ai_engine_stop.h"
#pragma GCC diagnostic pop
#include "f1c_pit.h"
typedef void (__attribute__((thiscall)) *F1CAIPitStartFn)(void*,int);
typedef unsigned char (__attribute__((thiscall)) *F1CAIPitDecisionFn)(void*);
static inline int f1c_ai_pit_valid(F1CLifecycleRead rd,uintptr_t v,DWORD thread) {
    int role=0,finish=0,status=0;unsigned char retired=0;
    return thread && GetCurrentThreadId()==thread &&
        f1c_lifecycle_read_at(rd,v+0x32c8,&role,4) && role==1 &&
        f1c_lifecycle_read_at(rd,v+0x4064,&finish,4) && finish==0 &&
        f1c_lifecycle_read_at(rd,v+0x4410,&retired,1) && !retired &&
        f1c_lifecycle_read_at(rd,v+0x3450,&status,4) && status==3;
}
static inline int f1c_ai_pit_restart(F1CLifecycleRead rd,uintptr_t v,DWORD thread) {
    static const unsigned char prefix[]={0x8b,0x81,0xbc,0x32,0,0,0x85,0xc0,0x75,0x12};
    const uintptr_t fn=0x004efa40u;
    float idle=0;
    if(!f1c_ai_pit_valid(rd,v,thread) ||
       !f1c_lifecycle_read_at(rd,v+0x39a8,&idle,4) || !isfinite(idle) || idle<=0 ||
       IsBadReadPtr((void*)fn,sizeof prefix) || memcmp((void*)fn,prefix,sizeof prefix))return 0;
    /* Argument 1 selects native restart-to-idle; argument 0 consumes native RNG. */
    ((F1CAIPitStartFn)fn)((void*)(v+12),1);
    return isfinite(VEHF_EngineRPM((void*)v)) && VEHF_EngineRPM((void*)v)>0;
}
static inline int f1c_ai_pit_retire(F1CLifecycleRead rd,uintptr_t v,DWORD thread) {
    static const unsigned char prefix[]={0x83,0xec,0x0c,0x53,0x55,0x56,0x8b,0xf1,0x8b,0x86,0xe0,0x45,0,0};
    const uintptr_t fn=0x0042a3d0u;
    uint32_t back=0,wing=0;int index=0;unsigned char intact=0;
    if(!f1c_ai_pit_valid(rd,v,thread) || !f1c_pit_service_active((void*)v) ||
       *(volatile int*)G_SessionType!=7 ||
       !f1c_lifecycle_read_at(rd,v,&back,4) || back!=v ||
       !f1c_lifecycle_read_at(rd,v+0xd18,&wing,4) || !wing ||
       !f1c_lifecycle_read_at(rd,v+0xd58,&intact,1) || intact!=1 ||
       !f1c_lifecycle_read_at(rd,v+0x45e0,&index,4) || index<0 || index>3 ||
       IsBadWritePtr((void*)(v+0xd58),1) || IsBadReadPtr((void*)fn,sizeof prefix) ||
       memcmp((void*)fn,prefix,sizeof prefix))return 0;
    /* Reuse the SDK's organic garage predicate only for this synchronous native
     * decision. No damaged-wing state is left behind and no player flags change.
     * The terminal plan has already finished diagnosis; normal service will end. */
    *(volatile unsigned char*)(v+0xd58)=0;
    ((F1CAIPitDecisionFn)fn)((void*)v);
    *(volatile unsigned char*)(v+0xd58)=intact;
    unsigned char retired=0;
    return f1c_lifecycle_read_at(rd,v+0x4410,&retired,1) && retired==1;
}
