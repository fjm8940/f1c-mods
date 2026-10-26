#ifndef F1C_INVERTED_REMOVAL_H
#define F1C_INVERTED_REMOVAL_H
#include "f1c_sc_pit_authority.h"
/* Native sub_4F1FE0 receives vehicle+0x0c and a float delta (thiscall,
   ret4). It resets its stationary accumulator before checking movement.
   Supply independently confirmed inversion duration only for approved AI;
   every native movement/pit/session gate and cleanup operation remains native.
   First7 bytes are whole non-relative instructions, reviewed exact EXE. */
typedef struct { unsigned epoch,identity; int valid; double last,elapsed;
    float x,z; } F1CInvertedRemoval;
static float f1c_inverted_elapsed(F1CInvertedRemoval *s,unsigned epoch,unsigned identity,
        double now,int eligible,float x,float z,float threshold) {
    if(!eligible||!isfinite(now)||!isfinite(x)||!isfinite(z)||
       !isfinite(threshold)||threshold<0||threshold>3600){memset(s,0,sizeof(*s));return 0;}
    if(!s->valid||s->epoch!=epoch||s->identity!=identity||now<s->last||
       now-s->last>2 || (x-s->x)*(x-s->x)+(z-s->z)*(z-s->z)>100) {
        memset(s,0,sizeof(*s));s->valid=1;s->epoch=epoch;s->identity=identity;
        s->last=now;s->x=x;s->z=z;return 0;
    }
    double dt=now-s->last;s->last=now;
    if(dt<=0)return 0;
    s->elapsed+=dt;
    return s->elapsed>=fmax(15.0,threshold)?threshold+0.25f:0;
}
typedef float (*F1CInvertedRemovalRequest)(uintptr_t vehicle);
typedef void (__attribute__((thiscall)) *F1CNativeRemoval)(void*,float);
static F1CDetour f1c_inverted_hook;
static F1CInvertedRemovalRequest f1c_inverted_request;
static DWORD f1c_inverted_thread;
static void __attribute__((fastcall)) f1c_inverted_detour(void *object,void *unused,float dt) {
    (void)unused;
    if(f1c_inverted_request && GetCurrentThreadId()==f1c_inverted_thread &&
       (uintptr_t)object>=12 && !IsBadReadPtr(object,4)) {
        uintptr_t vehicle=(uintptr_t)object-12;
        if(*(uint32_t*)object==vehicle) {
            float elapsed=f1c_inverted_request(vehicle);
            if(isfinite(dt)&&dt>0&&dt<=2&&isfinite(elapsed)&&elapsed>dt&&elapsed<=3600.25f)dt=elapsed;
        }
    }
    ((F1CNativeRemoval)f1c_inverted_hook.trampoline)(object,dt);
}
static float f1c_inverted_native_threshold(void) {
    return *(volatile float*)0x007d0a8cu;
}
static int f1c_inverted_install(F1CInvertedRemovalRequest request,DWORD thread,int verified) {
    static const unsigned char bytes[]={0x83,0xec,0x1c,0x55,0x56,0x8b,0xf1,0x8b,0x86,0x04,0x40,0,0};
    if(!verified||!thread||thread!=GetCurrentThreadId()||!request)return 0;
    if(f1c_inverted_hook.installed)return f1c_inverted_request==request;
    if(memcmp((void*)0x004f1fe0u,bytes,sizeof(bytes)))return 0;
    f1c_inverted_thread=thread;f1c_inverted_request=request;
    return f1c_detour_install_prevalidated_layout(&f1c_inverted_hook,(void*)0x004f1fe0u,
        (void*)f1c_inverted_detour,bytes,7)==F1C_DETOUR_OK;
}
static int f1c_inverted_remove(DWORD thread) {
    if(!thread||thread!=GetCurrentThreadId())return 0;
    f1c_inverted_request=NULL;
    return !f1c_inverted_hook.installed||f1c_detour_remove(&f1c_inverted_hook)==F1C_DETOUR_OK;
}
#endif
