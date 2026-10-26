#ifndef F1C_RACE_PENALTY_ISSUE_H
#define F1C_RACE_PENALTY_ISSUE_H

/* Exact-target, ordinary-race penalty request. This is intentionally separate
 * from the broker's Red Flag restart-penalty hold. StopGoPenaltyHandler and
 * DisqualificationHandler enqueue native race events; enqueue is NOT scoring
 * acceptance. The caller must observe the later scoring transition before
 * advancing any sanction ladder. All entry points require the game thread.
 *
 * Evidence: StopGoPenaltyHandler_004bbd60 (thiscall, ret 8),
 * DisqualificationHandler_004bc0e0 (thiscall, ret 4),
 * AccumulatorWithFixedArray_004bbed0 and the exact canonical EXE bytes.
 * Native gameplay acceptance remains pending. Never call in a replay, menu,
 * multiplayer session, or an unverified vehicle generation.
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "f1c_runtime.h"
#include "f1c_restart_penalties.h"

enum {
    F1C_RACE_STOP_GO_ADDRESS=0x004BBD60u,
    F1C_RACE_DQ_ADDRESS=0x004BC0E0u,
    F1C_RACE_EVENT_COUNT_ADDRESS=0x0083EE60u,
    F1C_RACE_EVENT_CURSOR_ADDRESS=0x008AAE64u,
    F1C_RACE_PENALTIES_ENABLED_ADDRESS=0x007CEF84u,
    F1C_RACE_PENALTY_SCORE_OFFSET=F1C_OFF_lapNumber-8u,
    F1C_RACE_EVENT_BYTES=432,
    F1C_RACE_EVENT_CAPACITY=1023
};

typedef enum {
    F1C_RACE_PENALTY_REFUSED=0,
    F1C_RACE_PENALTY_QUEUED=1,
    F1C_RACE_PENALTY_STOP_GO=2,
    F1C_RACE_PENALTY_DQ=3,
    F1C_RACE_PENALTY_UNCERTAIN=4 /* call happened; never retry this request */
} F1CRacePenaltyResult;

typedef struct {
    uintptr_t vehicle;
    int32_t driver_id,roster_index;
    int32_t baseline_penalty_count;
    uint32_t game_thread;
    uint32_t kind;
} F1CRacePenaltyTicket;

static int f1c_race_penalty_code_valid(const void *stop_go,const void *dq) {
    static const uint8_t sg[]={
        0x81,0xec,0x40,0x01,0x00,0x00,0x55,0x8b,0xe9,0x8b,0x45,0x30,
        0x85,0xc0,0x0f,0x85,0x44,0x01,0x00,0x00};
    static const uint8_t d[]={
        0x81,0xec,0x40,0x01,0x00,0x00,0x57,0x8b,0xf9,0x83,0x7f,0x30,
        0x03,0x0f,0x84,0x34,0x01,0x00,0x00};
    return stop_go&&dq&&!memcmp(stop_go,sg,sizeof sg)&&!memcmp(dq,d,sizeof d);
}

static int f1c_race_penalty_ticket_matches(const F1CRacePenaltyTicket *t,
        int slot,uint32_t game_thread) {
    const uint8_t *v;
    uintptr_t self=0;
    int32_t driver=0,roster=0,finish=0;
    if(!t||!t->vehicle||!game_thread||t->game_thread!=game_thread||
       GetCurrentThreadId()!=game_thread||slot<0||slot>=VEH_SLOTS||
       (uintptr_t)f1c_vehicle_slot(slot)!=t->vehicle)return 0;
    v=(const uint8_t*)t->vehicle;
    memcpy(&self,v+F1C_RACE_PENALTY_SCORE_OFFSET,sizeof self);
    memcpy(&driver,v+F1C_OFF_DriverID,sizeof driver);
    memcpy(&roster,v+F1C_OFF_VehicleRosterIndex,sizeof roster);
    memcpy(&finish,v+F1C_OFF_FinishState,sizeof finish);
    return self==t->vehicle&&driver==t->driver_id&&roster==t->roster_index&&
           (finish==0||(t->kind==F1C_RACE_PENALTY_DQ&&finish==3));
}

/* The caller proves offline, race phase 5, active player control, Safety Car
 * ownership, and a current caution. reason=NULL deliberately avoids falsely
 * labelling this as an existing cut-track/blue-flag infraction. */
static F1CRacePenaltyResult f1c_race_penalty_issue(int slot,
        uint32_t game_thread,int authorized,int kind,
        int32_t expected_driver,int32_t expected_roster,
        F1CRacePenaltyTicket *out) {
    F1CRestartPenalties before;
    F1CRacePenaltyTicket t;
    volatile int32_t *count=(volatile int32_t*)(uintptr_t)F1C_RACE_EVENT_COUNT_ADDRESS;
    volatile uintptr_t *cursor=(volatile uintptr_t*)(uintptr_t)F1C_RACE_EVENT_CURSOR_ADDRESS;
    int32_t n,finish;
    uintptr_t p;
    const uint8_t *v;
    MEMORY_BASIC_INFORMATION mbi;
    if(!out)return F1C_RACE_PENALTY_REFUSED;
    memset(out,0,sizeof *out);
    if(!authorized||!game_thread||GetCurrentThreadId()!=game_thread||
       (kind!=F1C_RACE_PENALTY_STOP_GO&&kind!=F1C_RACE_PENALTY_DQ)||
       slot<0||slot>=VEH_SLOTS||
       f1c_target_validate_headers()!=F1C_TARGET_OK||
       !*(volatile int32_t*)(uintptr_t)F1C_RACE_PENALTIES_ENABLED_ADDRESS||
       !f1c_race_penalty_code_valid((const void*)F1C_RACE_STOP_GO_ADDRESS,
                                    (const void*)F1C_RACE_DQ_ADDRESS))
        return F1C_RACE_PENALTY_REFUSED;
    v=(const uint8_t*)f1c_vehicle_slot(slot);
    if(!v||!f1c_restart_penalties_capture(v,VEH_STRIDE,&before)||
       (kind==F1C_RACE_PENALTY_STOP_GO&&before.count>=F1C_RESTART_PENALTY_CAPACITY))
        return F1C_RACE_PENALTY_REFUSED;
    memset(&t,0,sizeof t);
    t.vehicle=(uintptr_t)v;t.game_thread=game_thread;t.kind=(uint32_t)kind;
    t.baseline_penalty_count=before.count;
    t.driver_id=expected_driver;t.roster_index=expected_roster;
    memcpy(&finish,v+F1C_OFF_FinishState,sizeof finish);
    if(finish!=0||!f1c_race_penalty_ticket_matches(&t,slot,game_thread))
        return F1C_RACE_PENALTY_REFUSED;
    n=*count;p=*cursor;
    if(n<0||n>=F1C_RACE_EVENT_CAPACITY||!p||
       !VirtualQuery((const void*)p,&mbi,sizeof mbi)||
       mbi.State!=MEM_COMMIT||
       !(mbi.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE))||
       (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
       p<(uintptr_t)mbi.BaseAddress||
       p-(uintptr_t)mbi.BaseAddress>mbi.RegionSize||
       mbi.RegionSize-(p-(uintptr_t)mbi.BaseAddress)<F1C_RACE_EVENT_BYTES)
        return F1C_RACE_PENALTY_REFUSED;
    if(kind==F1C_RACE_PENALTY_STOP_GO) {
        typedef void (__attribute__((thiscall)) *Fn)(void*,const char*,char);
        ((Fn)F1C_RACE_STOP_GO_ADDRESS)((void*)(t.vehicle+F1C_RACE_PENALTY_SCORE_OFFSET),NULL,10);
    } else {
        typedef void (__attribute__((thiscall)) *Fn)(void*,const char*);
        ((Fn)F1C_RACE_DQ_ADDRESS)((void*)(t.vehicle+F1C_RACE_PENALTY_SCORE_OFFSET),NULL);
    }
    *out=t;
    if(*count!=n+1||*cursor!=p+F1C_RACE_EVENT_BYTES)
        return F1C_RACE_PENALTY_UNCERTAIN;
    return F1C_RACE_PENALTY_QUEUED;
}

/* Call on a later game tick, after the native event dispatcher. A queued
 * request is never counted as an accepted penalty until scoring reflects it. */
static F1CRacePenaltyResult f1c_race_penalty_observe(
        const F1CRacePenaltyTicket *t,int slot,uint32_t game_thread) {
    const uint8_t *v;
    F1CRestartPenalties after;
    int32_t finish;
    if(!f1c_race_penalty_ticket_matches(t,slot,game_thread))
        return F1C_RACE_PENALTY_REFUSED;
    v=(const uint8_t*)t->vehicle;
    if(t->kind==F1C_RACE_PENALTY_DQ){
        memcpy(&finish,v+F1C_OFF_FinishState,sizeof finish);
        return finish==3?F1C_RACE_PENALTY_DQ:F1C_RACE_PENALTY_QUEUED;
    }
    if(!f1c_restart_penalties_capture(v,VEH_STRIDE,&after))
        return F1C_RACE_PENALTY_REFUSED;
    if(after.count>t->baseline_penalty_count)
        for(int i=t->baseline_penalty_count;i<after.count;i++)
            if(after.pair[i][1]==10)return F1C_RACE_PENALTY_STOP_GO;
    return F1C_RACE_PENALTY_QUEUED;
}

#endif
