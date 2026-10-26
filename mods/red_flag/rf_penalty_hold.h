/* Included after RF state declarations. Native event interception belongs to
 * the broker. RF owns only the restart transaction and copied queue ledger. */
static F1CBrokerPenaltyBeginFn g_broker_penalty_begin;
static F1CBrokerPenaltyReadFn g_broker_penalty_read;
static F1CBrokerPenaltyEndFn g_broker_penalty_end;
static F1CBrokerPenaltySealFn g_broker_penalty_seal;
static BOOL g_penalty_hold_active;
static uint32_t g_penalty_hold_generation;
static volatile LONG g_penalty_capture_state,g_penalty_capture_result;
static DWORD g_penalty_capture_epoch;

static BOOL rf_penalty_end(void) {
    if(!g_penalty_hold_active)return TRUE;
    if(!g_broker_penalty_end)return FALSE;
    F1CBrokerResult r=g_broker_penalty_end(g_broker_control_token);
    if(r!=F1C_BROKER_OK && r!=F1C_BROKER_STALE_TOKEN && r!=F1C_BROKER_STALE_SESSION)return FALSE;
    g_penalty_hold_active=FALSE;g_penalty_hold_generation=0;return TRUE;
}
static BOOL rf_penalty_refresh(void) {
    if(!g_penalty_valid)return FALSE;
    if(!g_penalty_hold_active)return TRUE; /* checkpoint replay */
    if(!g_broker_penalty_read)return FALSE;
    /* Entries retired after announcement are deliberately excluded. */
    for(int i=0;i<g_active_count;i++) {
        F1CRestartPenalties *s=penalty_snapshot_for(g_active[i]);
        F1CBrokerResult result=s ? g_broker_penalty_read(g_broker_control_token,(uintptr_t)g_active[i],
             g_penalty_hold_generation,s) : F1C_BROKER_STALE_OBJECT;
        if(result!=F1C_BROKER_OK){
            char message[160];
            wsprintfA(message,"PENALTY LEDGER READ FAILED: survivor=%d result=%d ticket=%u phase=%d.",
                      i,(int)result,g_penalty_hold_generation,RACE_PHASE);
            log_w(message);g_penalty_valid=FALSE;return FALSE;
        }
    }
    return TRUE;
}
static BOOL rf_penalty_seal(void) {
    /* The broker authorizes exactly this constructor boundary and validates
       rebuilt identities; new issue events cannot disappear before green. */
    if(!rf_penalty_refresh())return FALSE;
    if(!g_penalty_hold_active)return g_checkpoint_replay;
    return g_broker_penalty_seal &&
        g_broker_penalty_seal(g_broker_control_token)==F1C_BROKER_OK;
}
static BOOL rf_penalty_begin_on_game_thread(void) {
    uintptr_t vehicles[104];F1CRestartPenalties queues[104];
    uint32_t generation=0;
    if(!g_broker_penalty_begin || g_penalty_hold_active || !g_penalty_snapshot ||
       g_active_count<=0 || g_active_count>104 || SESSION_TYPE!=7 || RACE_PHASE!=5)return FALSE;
    for(int i=0;i<g_active_count;i++)vehicles[i]=(uintptr_t)g_active[i];
    if(g_broker_penalty_begin(g_broker_control_token,vehicles,(uint32_t)g_active_count,
         queues,&generation)!=F1C_BROKER_OK)return FALSE;
    for(int i=0;i<g_active_count;i++) {
        g_penalty_snapshot[i].vehicle=g_active[i];g_penalty_snapshot[i].state=queues[i];
    }
    g_penalty_count=g_active_count;g_penalty_hold_generation=generation;
    g_penalty_hold_active=TRUE;g_penalty_valid=TRUE;return TRUE;
}
static void __cdecl rf_penalty_capture_task(void *unused) {
    (void)unused;
    if(InterlockedCompareExchange(&g_penalty_capture_state,2,1)!=1)return;
    BOOL okay=g_running && g_session_active && g_penalty_capture_epoch==g_session_epoch_id &&
        rf_penalty_begin_on_game_thread();
    InterlockedExchange(&g_penalty_capture_result,okay?1:-1);
    InterlockedExchange(&g_penalty_capture_state,3);
}
static BOOL rf_penalty_capture_sync(void) {
    if(!g_broker_registered || !g_broker_post || !g_broker_control_token ||
       g_penalty_hold_active || InterlockedCompareExchange(&g_penalty_capture_state,1,0)!=0)return FALSE;
    g_penalty_capture_epoch=g_session_epoch_id;
    InterlockedExchange(&g_penalty_capture_result,0);MemoryBarrier();
    if(g_broker_post(g_broker_control_token,rf_penalty_capture_task,NULL)!=RF_BROKER_OK) {
        InterlockedExchange(&g_penalty_capture_state,0);return FALSE;
    }
    DWORD deadline=GetTickCount()+2000u;
    for(;;) {
        LONG state=InterlockedCompareExchange(&g_penalty_capture_state,0,0);
        if(state==3)break;
        if(state==1 && (LONG)(GetTickCount()-deadline)>=0 &&
           InterlockedCompareExchange(&g_penalty_capture_state,0,1)==1)return FALSE;
        Sleep(1); /* claimed callback must finish before snapshot storage moves */
    }
    BOOL okay=InterlockedCompareExchange(&g_penalty_capture_result,0,0)==1;
    InterlockedExchange(&g_penalty_capture_state,0);return okay;
}
