#ifndef F1C_RACE_CONTROL_H
#define F1C_RACE_CONTROL_H
/* Optional SC/RF ownership ABI 1. No game addresses or hooks. The owner is a
 * lease, not a heartbeat: an unresponsive owner is NEVER timed out into idle.
 * All fields are individually atomic; authority comes solely from owner CAS.
 * SC acknowledges only after game-thread restoration; RF releases only after
 * cleanup. A generation number is diagnostic and never revokes another owner.
 *
 * The SC/RF block `Local\F1C_RACE_CONTROL_V1_<pid>_<created>` is frozen at
 * ABI 1 (abi=1, size=464, exact-struct check). It must stay byte-identical so
 * any already-shipped reader keeps opening it.
 *
 * ABI history: the Batch C ABI 2 attempt appended Mechanical Failures' "car
 * limited" record to this block. That broke ABI 1 readers that create the block
 * first (Safety Car 0.10.91), so it is reverted. The MF record now lives in its
 * OWN mapping (`Local\F1C_RACE_CONTROL_MF_V2_<pid>_<created>`, magic/abi/size
 * below); the f1c_rc_mf_* helpers use only that block and are inert when it is
 * absent. See development/mods/ABI2_MIXED_SET_CHECK_2026-10-08.md.
 */
#include <windows.h>
#include <stdint.h>
#define F1C_RC_ABI 1
#define F1C_RC_MAGIC 0x52434331L
#define F1C_RC_NONE 0
#define F1C_RC_SC 1
#define F1C_RC_RF 2
#define F1C_RC_SLOTS 104
#define F1C_RC_HDR_LONGS 12
#define F1C_RC_SIZE_V1 ((LONG)((F1C_RC_HDR_LONGS + F1C_RC_SLOTS) * 4))

typedef struct F1CRaceControl {
    volatile LONG magic, abi, size, owner, rf_registered, sc_registered;
    volatile LONG rf_request; /* 0 idle, 1 qualified warning/manual prompt, 2 takeover */
    volatile LONG revision, request_id, ack_id, sc_generation, sc_vehicle;
    volatile LONG controlled[F1C_RC_SLOTS]; /* exact vehicle identity, SC owned */
} F1CRaceControl;

static inline LONG f1c_rc_read(volatile LONG *p){return InterlockedCompareExchange(p,0,0);}
static inline F1CRaceControl *f1c_rc_open(void){
    static F1CRaceControl *v; static HANDLE mapping;
    char name[100]; FILETIME created,exit_time,kernel,user;
    if(v)return v;
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exit_time,&kernel,&user))return NULL;
    wsprintfA(name,"Local\\F1C_RACE_CONTROL_V1_%lu_%08lX%08lX",GetCurrentProcessId(),
        created.dwHighDateTime,created.dwLowDateTime);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*v),name);
    if(!mapping)return NULL;
    DWORD existing=GetLastError();
    F1CRaceControl *candidate=(F1CRaceControl*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*v));
    if(!candidate){CloseHandle(mapping);mapping=NULL;return NULL;}
    if(existing!=ERROR_ALREADY_EXISTS){
        candidate->abi=F1C_RC_ABI;candidate->size=sizeof(*candidate);
        InterlockedExchange(&candidate->magic,F1C_RC_MAGIC);
    }
    if(f1c_rc_read(&candidate->magic)!=F1C_RC_MAGIC || candidate->abi!=F1C_RC_ABI || candidate->size!=(LONG)sizeof(*candidate)){
        UnmapViewOfFile(candidate);CloseHandle(mapping);mapping=NULL;return NULL;
    }
    v=candidate;return v;
}
static inline void f1c_rc_request(F1CRaceControl *s,int request){
    if(f1c_rc_read(&s->rf_request)!=request){
        InterlockedIncrement(&s->request_id);
        InterlockedExchange(&s->rf_request,request);
        InterlockedIncrement(&s->revision);
    }
}
static inline int f1c_rc_sc_enter(F1CRaceControl *s){
    if(!s)return 0;
    LONG owner=InterlockedCompareExchange(&s->owner,F1C_RC_SC,F1C_RC_NONE);
    return owner==F1C_RC_NONE || owner==F1C_RC_SC;
}
static inline int f1c_rc_rf_acquire(F1CRaceControl *s){
    if(!s)return 0;
    f1c_rc_request(s,2);
    LONG owner=InterlockedCompareExchange(&s->owner,F1C_RC_RF,F1C_RC_NONE);
    return owner==F1C_RC_NONE || owner==F1C_RC_RF;
}
static inline void f1c_rc_sc_release(F1CRaceControl *s){
    if(!s || f1c_rc_read(&s->owner)!=F1C_RC_SC)return;
    for(int i=0;i<F1C_RC_SLOTS;i++)InterlockedExchange(&s->controlled[i],0);
    InterlockedExchange(&s->ack_id,f1c_rc_read(&s->request_id));
    InterlockedIncrement(&s->revision);
    InterlockedCompareExchange(&s->owner,F1C_RC_NONE,F1C_RC_SC);
}
static inline void f1c_rc_rf_release(F1CRaceControl *s){
    if(!s)return;
    f1c_rc_request(s,0);
    InterlockedIncrement(&s->revision);
    InterlockedCompareExchange(&s->owner,F1C_RC_NONE,F1C_RC_RF);
}
static inline int f1c_rc_controlled(F1CRaceControl *s,int slot,uintptr_t vehicle){
    (void)slot;
    if(!s || !vehicle || f1c_rc_read(&s->owner)!=F1C_RC_SC)return 0;
    for(int i=0;i<F1C_RC_SLOTS;i++)if(f1c_rc_read(&s->controlled[i])==(LONG)vehicle)return 1;
    return 0;
}

/* --- Mechanical Failures "car limited" record: separate mapping (ABI 2) -----
 * The MF record is NOT part of the SC/RF block. It lives in its own mapping so
 * the frozen ABI 1 SC/RF block never changes. MF is the only writer
 * (f1c_rc_mf_set / _clear / _clear_all); every other mod may only read
 * (f1c_rc_mf_limited). If the MF mapping cannot be opened every helper is inert
 * (f1c_rc_mf_supported() = 0, writers no-ops, f1c_rc_mf_limited() = 0). */
#define F1C_MF_ABI     2
#define F1C_MF_ABI_MIN 2
#define F1C_MF_MAGIC 0x32464D32L /* '2','M','F','2' little-endian */
#define F1C_MF_SLOTS F1C_RC_SLOTS

typedef struct F1CMfLimited {
    volatile LONG magic, abi, size;
    volatile LONG mf_revision;
    volatile LONG mf_limited[F1C_MF_SLOTS]; /* exact vehicle identity MF is degrading; 0 = none */
} F1CMfLimited;

static inline F1CMfLimited *f1c_rc_mf_open(void){
    static F1CMfLimited *v; static HANDLE mapping;
    char name[100]; FILETIME created,exit_time,kernel,user;
    if(v)return v;
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exit_time,&kernel,&user))return NULL;
    wsprintfA(name,"Local\\F1C_RACE_CONTROL_MF_V2_%lu_%08lX%08lX",GetCurrentProcessId(),
        created.dwHighDateTime,created.dwLowDateTime);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*v),name);
    if(!mapping)return NULL;
    DWORD existing=GetLastError();
    F1CMfLimited *candidate=(F1CMfLimited*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,0);
    if(!candidate){CloseHandle(mapping);mapping=NULL;return NULL;}
    if(existing!=ERROR_ALREADY_EXISTS){
        candidate->abi=F1C_MF_ABI;candidate->size=sizeof(*candidate);
        InterlockedExchange(&candidate->magic,F1C_MF_MAGIC);
    }
    {
        LONG abi=f1c_rc_read(&candidate->abi), size=candidate->size;
        if(f1c_rc_read(&candidate->magic)!=F1C_MF_MAGIC || abi<F1C_MF_ABI_MIN ||
           abi>F1C_MF_ABI || size<(LONG)sizeof(F1CMfLimited)){
            UnmapViewOfFile(candidate);CloseHandle(mapping);mapping=NULL;return NULL;
        }
    }
    v=candidate;return v;
}
static inline int f1c_rc_mf_supported(F1CMfLimited *s){
    return s && f1c_rc_read(&s->magic)==F1C_MF_MAGIC &&
           f1c_rc_read(&s->abi)>=F1C_MF_ABI_MIN && s->size>=(LONG)sizeof(F1CMfLimited);
}
static inline void f1c_rc_mf_set(F1CMfLimited *s,uintptr_t vehicle){
    int i,free_slot=-1;
    if(!f1c_rc_mf_supported(s) || !vehicle)return;
    for(i=0;i<F1C_MF_SLOTS;i++){
        LONG v=f1c_rc_read(&s->mf_limited[i]);
        if(v==(LONG)vehicle)return;
        if(!v && free_slot<0)free_slot=i;
    }
    if(free_slot>=0){
        InterlockedExchange(&s->mf_limited[free_slot],(LONG)vehicle);
        InterlockedIncrement(&s->mf_revision);
    }
}
static inline void f1c_rc_mf_clear(F1CMfLimited *s,uintptr_t vehicle){
    int i;
    if(!f1c_rc_mf_supported(s))return;
    for(i=0;i<F1C_MF_SLOTS;i++)
        if(f1c_rc_read(&s->mf_limited[i])==(LONG)vehicle)
            InterlockedExchange(&s->mf_limited[i],0);
    InterlockedIncrement(&s->mf_revision);
}
static inline void f1c_rc_mf_clear_all(F1CMfLimited *s){
    int i;
    if(!f1c_rc_mf_supported(s))return;
    for(i=0;i<F1C_MF_SLOTS;i++)InterlockedExchange(&s->mf_limited[i],0);
    InterlockedIncrement(&s->mf_revision);
}
static inline int f1c_rc_mf_limited(F1CMfLimited *s,uintptr_t vehicle){
    int i;
    if(!f1c_rc_mf_supported(s) || !vehicle)return 0;
    for(i=0;i<F1C_MF_SLOTS;i++)
        if(f1c_rc_read(&s->mf_limited[i])==(LONG)vehicle)return 1;
    return 0;
}
#endif
