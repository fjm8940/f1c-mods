/* Broker-owned installation/teardown. Validate every complete body before
 * installing any of the optional hooks. Partial startup remains pass-through;
 * teardown still detaches each installed hook and drains all native calls. */
static int broker_penalty_code_matches(const void *address,const void *expected,size_t size) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t start=(uintptr_t)address,end;
    if(!VirtualQuery(address,&region,sizeof region) || region.State!=MEM_COMMIT ||
       (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
       !(region.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return 0;
    end=(uintptr_t)region.BaseAddress+region.RegionSize;
    return end>=(uintptr_t)region.BaseAddress && start>=(uintptr_t)region.BaseAddress &&
        start<=end && size<=end-start && !memcmp(address,expected,size);
}
static void broker_penalty_install(void) {
    F1CDetourResult r;
#define PENALTY_MATCH(name) broker_penalty_code_matches(BROKER_PENALTY_##name##_ADDRESS, BROKER_PENALTY_##name##_BYTES,sizeof BROKER_PENALTY_##name##_BYTES)
    if(!PENALTY_MATCH(ISSUE) || !PENALTY_MATCH(SERVICE) || !PENALTY_MATCH(CONSUME) || !PENALTY_MATCH(LAP) ||
       !f1c_restart_penalty_mode_code_valid((void*)F1C_RESTART_PENALTY_MODE_ADDRESS,65)) {
        log_line("penalty-hold capability refused: native layout mismatch");return;
    }
#undef PENALTY_MATCH
    r=f1c_detour_install_prevalidated_layout(&g_penalty_issue_detour,BROKER_PENALTY_ISSUE_ADDRESS,
        broker_penalty_issue_hook,BROKER_PENALTY_ISSUE_BYTES,6);
    if(r!=F1C_DETOUR_OK)goto refused;
    g_penalty_issue_original=(BrokerPenaltyIssueFn)g_penalty_issue_detour.trampoline;
    r=f1c_detour_install_prevalidated_layout(&g_penalty_service_detour,BROKER_PENALTY_SERVICE_ADDRESS,
        broker_penalty_service_hook,BROKER_PENALTY_SERVICE_BYTES,5);
    if(r!=F1C_DETOUR_OK)goto refused;
    g_penalty_service_original=(BrokerPenaltyServiceFn)g_penalty_service_detour.trampoline;
    r=f1c_detour_install_prevalidated_layout(&g_penalty_consume_detour,BROKER_PENALTY_CONSUME_ADDRESS,
        broker_penalty_consume_hook,BROKER_PENALTY_CONSUME_BYTES,5);
    if(r!=F1C_DETOUR_OK)goto refused;
    g_penalty_consume_original=(BrokerPenaltyServiceFn)g_penalty_consume_detour.trampoline;
    r=f1c_detour_install_prevalidated_layout(&g_penalty_lap_detour,BROKER_PENALTY_LAP_ADDRESS,
        broker_penalty_lap_hook,BROKER_PENALTY_LAP_BYTES,6);
    if(r!=F1C_DETOUR_OK)goto refused;
    g_penalty_lap_original=(BrokerPenaltyLapFn)g_penalty_lap_detour.trampoline;
    g_penalty_supported=1;
    log_line("penalty-hold capability enabled: issue/service/consume/lap bodies validated");return;
refused:
    log_line("penalty-hold unavailable: hook result=%d; installed hooks retained as pass-throughs",(int)r);
}
static F1CDetour *const broker_penalty_hooks[]={&g_penalty_issue_detour,&g_penalty_service_detour,
    &g_penalty_consume_detour,&g_penalty_lap_detour};
static int broker_penalty_detach(void) {
    for(unsigned i=0;i<sizeof broker_penalty_hooks/sizeof broker_penalty_hooks[0];i++)
        if(broker_penalty_hooks[i]->installed &&
           detach_preserving_trampoline(broker_penalty_hooks[i])!=F1C_DETOUR_OK)return 0;
    return 1;
}
static void broker_penalty_free_trampolines(void) {
    for(unsigned i=0;i<sizeof broker_penalty_hooks/sizeof broker_penalty_hooks[0];i++)
        if(broker_penalty_hooks[i]->trampoline) {
            VirtualFree(broker_penalty_hooks[i]->trampoline,0,MEM_RELEASE);
            broker_penalty_hooks[i]->trampoline=NULL;
        }
}
