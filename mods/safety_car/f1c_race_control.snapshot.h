#ifndef F1C_RACE_CONTROL_H
#define F1C_RACE_CONTROL_H
/* Optional SC/RF ownership ABI 1. No game addresses or hooks. The owner is a
 * lease, not a heartbeat: an unresponsive owner is NEVER timed out into idle.
 * All fields are individually atomic; authority comes solely from owner CAS.
 * SC acknowledges only after game-thread restoration; RF releases only after
 * cleanup. A generation number is diagnostic and never revokes another owner.
 */
#include <windows.h>
#include <stdint.h>
#define F1C_RC_ABI 1
#define F1C_RC_MAGIC 0x52434331L
#define F1C_RC_NONE 0
#define F1C_RC_SC 1
#define F1C_RC_RF 2
#define F1C_RC_SLOTS 104

typedef struct F1CRaceControl {
    volatile LONG magic, abi, size, owner, rf_registered, sc_registered;
    volatile LONG rf_request; /* 0 idle, 1 qualified warning/manual prompt, 2 takeover */
    volatile LONG revision, request_id, ack_id, sc_generation, sc_vehicle;
    volatile LONG controlled[F1C_RC_SLOTS]; /* exact vehicle identity, SC owned */
} F1CRaceControl;
static LONG f1c_rc_read(volatile LONG *p){return InterlockedCompareExchange(p,0,0);}
static F1CRaceControl *f1c_rc_open(void){
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
static void f1c_rc_request(F1CRaceControl *s,int request){
    if(f1c_rc_read(&s->rf_request)!=request){
        InterlockedIncrement(&s->request_id);
        InterlockedExchange(&s->rf_request,request);
        InterlockedIncrement(&s->revision);
    }
}
static int f1c_rc_sc_enter(F1CRaceControl *s){
    if(!s)return 0;
    LONG owner=InterlockedCompareExchange(&s->owner,F1C_RC_SC,F1C_RC_NONE);
    return owner==F1C_RC_NONE || owner==F1C_RC_SC;
}
static int f1c_rc_rf_acquire(F1CRaceControl *s){
    if(!s)return 0;
    f1c_rc_request(s,2);
    LONG owner=InterlockedCompareExchange(&s->owner,F1C_RC_RF,F1C_RC_NONE);
    return owner==F1C_RC_NONE || owner==F1C_RC_RF;
}
static void f1c_rc_sc_release(F1CRaceControl *s){
    if(!s || f1c_rc_read(&s->owner)!=F1C_RC_SC)return;
    for(int i=0;i<F1C_RC_SLOTS;i++)InterlockedExchange(&s->controlled[i],0);
    InterlockedExchange(&s->ack_id,f1c_rc_read(&s->request_id));
    InterlockedIncrement(&s->revision);
    InterlockedCompareExchange(&s->owner,F1C_RC_NONE,F1C_RC_SC);
}
static void f1c_rc_rf_release(F1CRaceControl *s){
    if(!s)return;
    f1c_rc_request(s,0);
    InterlockedIncrement(&s->revision);
    InterlockedCompareExchange(&s->owner,F1C_RC_NONE,F1C_RC_RF);
}
static int f1c_rc_controlled(F1CRaceControl *s,int slot,uintptr_t vehicle){
    (void)slot;
    if(!s || !vehicle || f1c_rc_read(&s->owner)!=F1C_RC_SC)return 0;
    for(int i=0;i<F1C_RC_SLOTS;i++)if(f1c_rc_read(&s->controlled[i])==(LONG)vehicle)return 1;
    return 0;
}
#endif
