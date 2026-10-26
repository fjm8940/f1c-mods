/* Included after broker API/lifecycle helpers. All hooks belong to the broker. */
#ifndef BROKER_PENALTY_SESSION_TYPE
#define BROKER_PENALTY_SESSION_TYPE (*(volatile int32_t*)0x007C56A4u)
#define BROKER_PENALTY_RACE_PHASE (*(volatile int32_t*)0x007C56A8u)
#endif
static void broker_penalty_forget_locked(void) {
    g_penalty_owner=0;g_penalty_count=0;g_penalty_generation=0;
    g_penalty_live_generation=0;g_penalty_restart_sealed=0;g_penalty_awaiting_restart=0;
    memset(g_penalty_entries,0,sizeof g_penalty_entries);
}
static BrokerPenaltyEntry *broker_penalty_entry_locked(void *score,int issue_event) {
    if(g_penalty_owner && !client_for_token_locked(g_penalty_owner)) {
        broker_penalty_forget_locked();return NULL;
    }
    if(!g_penalty_supported || !g_penalty_owner ||
       (!g_session_active && !g_penalty_awaiting_restart) ||
       g_penalty_live_generation!=g_session_generation ||
       BROKER_PENALTY_SESSION_TYPE!=7 || (BROKER_PENALTY_RACE_PHASE!=5 &&
        !(issue_event && BROKER_PENALTY_RACE_PHASE==3)))return NULL;
    for(uint32_t i=0;i<g_penalty_count;i++) {
        BrokerPenaltyEntry *e=&g_penalty_entries[i];
        if((uintptr_t)score!=e->vehicle+BROKER_PENALTY_SCORE_OFFSET)continue;
        struct F1CVehicle *v=f1c_vehicle_slot((int)e->slot);
        if((uintptr_t)v!=e->vehicle || !v || v->DriverID!=e->driver ||
           *(uintptr_t*)score!=e->vehicle){e->fault=1;return NULL;}
        return e;
    }
    return NULL;
}
__declspec(dllexport) F1CBrokerResult __cdecl F1CBrokerPenaltyBegin(
        F1CBrokerToken token,const uintptr_t *vehicles,uint32_t count,
        F1CRestartPenalties *out,uint32_t *generation) {
    BrokerPenaltyEntry pending[VEH_SLOTS];
    F1CBrokerResult result=F1C_BROKER_OK;
    if(!vehicles || !out || !generation || !count || count>VEH_SLOTS ||
       GetCurrentThreadId()!=(DWORD)g_hook_thread_id)return F1C_BROKER_BAD_ARGUMENT;
    if(!begin_api_call())return F1C_BROKER_NOT_READY;
    EnterCriticalSection(&g_lock);
    if(!client_for_token_locked(token))result=F1C_BROKER_STALE_TOKEN;
    else if(!g_penalty_supported || !g_session_active ||
        BROKER_PENALTY_SESSION_TYPE!=7 || BROKER_PENALTY_RACE_PHASE!=5)
        result=F1C_BROKER_NOT_READY;
    else if(g_penalty_owner || g_active_penalty_calls)result=F1C_BROKER_CAPABILITY_IN_USE;
    memset(pending,0,sizeof pending);
    for(uint32_t i=0;result==F1C_BROKER_OK && i<count;i++) {
        int slot=-1;
        for(int n=0;n<VEH_SLOTS;n++)if((uintptr_t)f1c_vehicle_slot(n)==vehicles[i]){slot=n;break;}
        for(uint32_t n=0;n<i;n++)if(vehicles[n]==vehicles[i])slot=-1;
        struct F1CVehicle *v=(struct F1CVehicle*)vehicles[i];
        if(!v || slot<0 || *(uintptr_t*)((char*)v+BROKER_PENALTY_SCORE_OFFSET)!=(uintptr_t)v ||
           !f1c_restart_penalties_capture(v,VEH_STRIDE,&pending[i].queue)) {
            result=F1C_BROKER_STALE_OBJECT;break;
        }
        pending[i].vehicle=vehicles[i];pending[i].slot=(uint32_t)slot;pending[i].driver=v->DriverID;
    }
    if(result==F1C_BROKER_OK) {
        memcpy(g_penalty_entries,pending,count*sizeof pending[0]);
        g_penalty_count=count;g_penalty_generation=g_session_generation;g_penalty_owner=token;
        g_penalty_live_generation=g_session_generation;g_penalty_restart_sealed=0;g_penalty_awaiting_restart=0;
        for(uint32_t i=0;i<count;i++)out[i]=pending[i].queue;
        *generation=g_penalty_generation;
    }
    LeaveCriticalSection(&g_lock);end_api_call();return result;
}
__declspec(dllexport) F1CBrokerResult __cdecl F1CBrokerPenaltyRead(
        F1CBrokerToken token,uintptr_t vehicle,uint32_t generation,F1CRestartPenalties *out) {
    F1CBrokerResult result=F1C_BROKER_STALE_OBJECT;
    if(!out || !vehicle || GetCurrentThreadId()!=(DWORD)g_hook_thread_id)return F1C_BROKER_BAD_ARGUMENT;
    if(!begin_api_call())return F1C_BROKER_NOT_READY;
    EnterCriticalSection(&g_lock);
    if(!client_for_token_locked(token) || token!=g_penalty_owner)result=F1C_BROKER_STALE_TOKEN;
    /* The update hook can run before RestartGate reports re-entry. A sealed
       constructor boundary is still the same owned ledger: permit reading it
       on the rebuilt grid/first green, with the usual per-car checks below. */
    else if(generation!=g_penalty_generation || g_penalty_live_generation!=g_session_generation ||
        (!g_session_active && !(g_penalty_restart_sealed && g_penalty_awaiting_restart &&
          BROKER_PENALTY_SESSION_TYPE==7 && BROKER_PENALTY_RACE_PHASE>=3 &&
          BROKER_PENALTY_RACE_PHASE<=5)))
        result=F1C_BROKER_STALE_SESSION;
    else for(uint32_t i=0;i<g_penalty_count;i++)if(g_penalty_entries[i].vehicle==vehicle) {
        BrokerPenaltyEntry *e=&g_penalty_entries[i];
        struct F1CVehicle *v=f1c_vehicle_slot((int)e->slot);
        if((uintptr_t)v!=e->vehicle || !v || v->DriverID!=e->driver ||
           *(uintptr_t*)((char*)v+BROKER_PENALTY_SCORE_OFFSET)!=e->vehicle)e->fault=1;
        if(e->fault)result=F1C_BROKER_STALE_OBJECT;
        else {*out=e->queue;result=F1C_BROKER_OK;}
        break;
    }
    LeaveCriticalSection(&g_lock);end_api_call();return result;
}
__declspec(dllexport) F1CBrokerResult __cdecl F1CBrokerPenaltySeal(F1CBrokerToken token) {
    F1CBrokerResult result=F1C_BROKER_OK;
    if(GetCurrentThreadId()!=(DWORD)g_hook_thread_id)return F1C_BROKER_BAD_ARGUMENT;
    if(!begin_api_call())return F1C_BROKER_NOT_READY;
    EnterCriticalSection(&g_lock);
    if(!client_for_token_locked(token) || token!=g_penalty_owner)result=F1C_BROKER_STALE_TOKEN;
    else if(!g_session_active || g_active_penalty_calls || g_penalty_restart_sealed ||
            g_penalty_live_generation!=g_session_generation || BROKER_PENALTY_SESSION_TYPE!=7 ||
            BROKER_PENALTY_RACE_PHASE!=5)result=F1C_BROKER_NOT_READY;
    else g_penalty_restart_sealed=1;
    LeaveCriticalSection(&g_lock);end_api_call();return result;
}
/* RestartGate owns the session transition. Only a sealed RF transaction may
   cross one constructor return (4) and re-entry (5); exit and every other
   boundary invalidate it. The ticket stays stable, but native identities are
   checked again on each event/read before any restored scoring write. */
static void broker_penalty_session_leave_locked(int result) {
    if(result==4 && g_penalty_owner && g_penalty_restart_sealed &&
       !g_penalty_awaiting_restart && g_penalty_live_generation==g_session_generation)
        g_penalty_awaiting_restart=1;
    else broker_penalty_forget_locked();
}
static void broker_penalty_session_enter_locked(void) {
    if(g_penalty_owner && g_penalty_restart_sealed && g_penalty_awaiting_restart) {
        g_penalty_live_generation=g_session_generation;g_penalty_awaiting_restart=0;
        g_penalty_restart_sealed=0;
    } else if(g_penalty_owner)broker_penalty_forget_locked();
}
__declspec(dllexport) F1CBrokerResult __cdecl F1CBrokerPenaltyEnd(F1CBrokerToken token) {
    F1CBrokerResult result=F1C_BROKER_OK;
    if(!begin_api_call())return F1C_BROKER_NOT_READY;
    EnterCriticalSection(&g_lock);
    if(!client_for_token_locked(token) && token!=g_penalty_owner)result=F1C_BROKER_STALE_TOKEN;
    else if(g_penalty_owner && token!=g_penalty_owner)result=F1C_BROKER_CAPABILITY_IN_USE;
    else if(g_active_penalty_calls)result=F1C_BROKER_NOT_READY;
    else broker_penalty_forget_locked();
    LeaveCriticalSection(&g_lock);end_api_call();return result;
}
static int __attribute__((thiscall)) broker_penalty_issue_hook(void *score,const void *event) {
    InterlockedIncrement(&g_active_penalty_calls);
    while(!g_penalty_issue_original)Sleep(0);
    int seconds=*(const signed char*)((const char*)event+8);
    int result=g_penalty_issue_original(score,event);
    EnterCriticalSection(&g_lock);
    BrokerPenaltyEntry *e=broker_penalty_entry_locked(score,1);
    if(e && !f1c_penalty_hold_issue(&e->queue,seconds))e->fault=1;
    LeaveCriticalSection(&g_lock);
    InterlockedDecrement(&g_active_penalty_calls);return result;
}
static int __attribute__((thiscall)) broker_penalty_service_hook(void *score) {
    InterlockedIncrement(&g_active_penalty_calls);
    while(!g_penalty_service_original)Sleep(0);
    EnterCriticalSection(&g_lock);int held=broker_penalty_entry_locked(score,0)!=NULL;LeaveCriticalSection(&g_lock);
    int result=held?0:g_penalty_service_original(score);
    InterlockedDecrement(&g_active_penalty_calls);return result;
}
static int __attribute__((thiscall)) broker_penalty_consume_hook(void *score) {
    InterlockedIncrement(&g_active_penalty_calls);
    while(!g_penalty_consume_original)Sleep(0);
    EnterCriticalSection(&g_lock);int held=broker_penalty_entry_locked(score,0)!=NULL;LeaveCriticalSection(&g_lock);
    int result=held?0:g_penalty_consume_original(score);
    InterlockedDecrement(&g_active_penalty_calls);return result;
}
static void __attribute__((thiscall)) broker_penalty_lap_hook(void *score,float lap_time,float time,int sector,int lap) {
    InterlockedIncrement(&g_active_penalty_calls);
    while(!g_penalty_lap_original)Sleep(0);
    EnterCriticalSection(&g_lock);
    BrokerPenaltyEntry *e=broker_penalty_entry_locked(score,0);
    /* Three is the native fresh allowance. The native loop can neither expire
       nor disqualify it in this call. Restore the frozen ledger afterward;
       issue hooks include even identical or full-queue nested additions. The
       original still owns timing, classification and final time penalties. */
    int held=e && !e->fault;
    if(held) {
        f1c_restart_penalties_restore((void*)e->vehicle,VEH_STRIDE,&e->queue);
        for(int i=0;i<e->queue.count;i++)((int32_t*)((char*)score+0x40))[2*i]=3;
    }
    LeaveCriticalSection(&g_lock);
    g_penalty_lap_original(score,lap_time,time,sector,lap);
    EnterCriticalSection(&g_lock);
    e=held?broker_penalty_entry_locked(score,0):NULL;
    /* Native lap completion may have changed phase to results. Identity and
       generation remain owned; restore the queue without undoing results. */
    if(held && !e && g_penalty_owner && g_session_active && g_penalty_live_generation==g_session_generation)
        for(uint32_t i=0;i<g_penalty_count;i++)if(g_penalty_entries[i].vehicle+BROKER_PENALTY_SCORE_OFFSET==(uintptr_t)score) {
            BrokerPenaltyEntry *candidate=&g_penalty_entries[i];
            struct F1CVehicle *v=f1c_vehicle_slot((int)candidate->slot);
            if((uintptr_t)v==candidate->vehicle && v->DriverID==candidate->driver &&
               *(uintptr_t*)score==candidate->vehicle)e=candidate;
            break;
        }
    if(e && !e->fault)f1c_restart_penalties_restore((void*)e->vehicle,VEH_STRIDE,&e->queue);
    LeaveCriticalSection(&g_lock);
    InterlockedDecrement(&g_active_penalty_calls);
}
