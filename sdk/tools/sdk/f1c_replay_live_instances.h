#ifndef F1C_REPLAY_LIVE_INSTANCES_H
#define F1C_REPLAY_LIVE_INSTANCES_H
/* Instant replay is synchronous inside TwoPathDispatch. Preserve physical
 * instances for that call only: ProcessDriverEntries must not reconstruct a
 * live race from incomplete post-restart recording history. Pose playback and
 * LoadingPhase2 stay native. Saved-replay loading is outside this lifetime.
 * Reviewed exact x86 target; first five bytes are three complete instructions.
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#define F1C_LIVE_DISPATCH 0x004BA6C0u
#define F1C_LIVE_MODE 0x008AB6E4u
#define F1C_LIVE_REQUEST 0x007CC4C8u
#define F1C_LIVE_BASE 0x005EC6D8u
#define F1C_LIVE_STRIDE 0x48C0u
#define F1C_LIVE_COUNT 0x005C329Cu
#define F1C_LIVE_ARRAY 0x007C54F0u
typedef int (__cdecl *F1CLiveReplayAuthorized)(void);
typedef char (__attribute__((thiscall)) *F1CLiveDispatch)(void*);
typedef struct F1CLiveInstance {
    uintptr_t scene,self;
    int physical,roster;
    char name[32];
} F1CLiveInstance;
static F1CLiveInstance f1c_live_instances[104];
static int f1c_live_active,f1c_live_installed;
static DWORD f1c_live_thread;
static F1CLiveReplayAuthorized f1c_live_authorized;
static F1CLiveDispatch f1c_live_original;
static unsigned char f1c_live_patch[5];
static const unsigned char f1c_live_expected[5]={0x56,0x8b,0xf1,0x8b,0x06};
static void f1c_live_read(uintptr_t v,F1CLiveInstance* p) {
    p->scene=*(uintptr_t*)(v+0x6f4);p->self=*(uintptr_t*)(v+0x4034);
    p->physical=*(int*)(v+0x4894);p->roster=*(int*)(v+0x4898);
    memcpy(p->name,(void*)(v+0x4620),sizeof p->name);
}
static int f1c_live_begin(void* context) {
    int count=*(int*)F1C_LIVE_COUNT,seen[104]={0};
    if(f1c_live_active || !f1c_live_installed || GetCurrentThreadId()!=f1c_live_thread ||
       context!=(void*)F1C_LIVE_MODE || *(int*)context!=1 || !*(int*)F1C_LIVE_REQUEST ||
       !f1c_live_authorized || !f1c_live_authorized() || count<1 || count>104) return 0;
    for(int i=0;i<104;i++) {
        uintptr_t v=F1C_LIVE_BASE+F1C_LIVE_STRIDE*i;
        f1c_live_read(v,&f1c_live_instances[i]);
        if(f1c_live_instances[i].scene &&
           (f1c_live_instances[i].self!=v || f1c_live_instances[i].physical!=i ||
            !memchr(f1c_live_instances[i].name,0,sizeof f1c_live_instances[i].name))) return 0;
    }
    for(int i=0;i<count;i++) {
        uintptr_t v=((uintptr_t*)F1C_LIVE_ARRAY)[i];
        if(v<F1C_LIVE_BASE || (v-F1C_LIVE_BASE)%F1C_LIVE_STRIDE) return 0;
        unsigned slot=(v-F1C_LIVE_BASE)/F1C_LIVE_STRIDE;
        if(slot>=104 || seen[slot]++ || !f1c_live_instances[slot].scene ||
           *(int*)(v+13000)<0 || *(int*)(v+13000)>1) return 0;
    }
    f1c_live_active=1;return 1;
}
static int f1c_live_instance_preserved(uintptr_t v) {
    F1CLiveInstance current;
    if(!f1c_live_active || GetCurrentThreadId()!=f1c_live_thread ||
       *(int*)F1C_LIVE_MODE!=2 || v<F1C_LIVE_BASE ||
       (v-F1C_LIVE_BASE)%F1C_LIVE_STRIDE) return 0;
    unsigned slot=(v-F1C_LIVE_BASE)/F1C_LIVE_STRIDE;
    if(slot>=104)return 0;
    f1c_live_read(v,&current);
    return !memcmp(&current,&f1c_live_instances[slot],sizeof current);
}
static __attribute__((noinline)) char __attribute__((thiscall)) f1c_live_dispatch(void* context) {
    int owner=f1c_live_begin(context);
    char result=f1c_live_original(context);
    if(owner)f1c_live_active=0; /* Nested playback dispatch must not clear it. */
    return result;
}
static int f1c_live_install(F1CLiveReplayAuthorized authorized,DWORD thread) {
    unsigned char* at=(unsigned char*)F1C_LIVE_DISPATCH;DWORD old,ignored;
    if(!authorized || !thread || GetCurrentThreadId()!=thread)return 0;
    if(f1c_live_installed)return authorized==f1c_live_authorized && thread==f1c_live_thread;
    if(memcmp(at,f1c_live_expected,5))return 0;
    unsigned char* tramp=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!tramp)return 0;
    memcpy(tramp,at,5);tramp[5]=0xe9;
    *(int32_t*)(tramp+6)=(int32_t)((uintptr_t)(at+5)-(uintptr_t)(tramp+10));
    f1c_live_patch[0]=0xe9;
    *(int32_t*)(f1c_live_patch+1)=(int32_t)((uintptr_t)f1c_live_dispatch-(uintptr_t)(at+5));
    if(!VirtualProtect(at,5,PAGE_EXECUTE_READWRITE,&old))return 0;
    f1c_live_original=(F1CLiveDispatch)tramp;f1c_live_authorized=authorized;f1c_live_thread=thread;
    f1c_live_installed=1;memcpy(at,f1c_live_patch,5);
    VirtualProtect(at,5,old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),tramp,10);FlushInstructionCache(GetCurrentProcess(),at,5);
    return 1;
}
static int f1c_live_remove(DWORD thread) {
    unsigned char* at=(unsigned char*)F1C_LIVE_DISPATCH;DWORD old,ignored;
    if(!f1c_live_installed)return 1;
    if(f1c_live_active || thread!=f1c_live_thread || GetCurrentThreadId()!=thread ||
       memcmp(at,f1c_live_patch,5) || !VirtualProtect(at,5,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy(at,f1c_live_expected,5);f1c_live_installed=0;f1c_live_authorized=NULL;
    FlushInstructionCache(GetCurrentProcess(),at,5);
    return VirtualProtect(at,5,old,&ignored)!=0;
}
#endif
