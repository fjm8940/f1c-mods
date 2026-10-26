"""Compile actual broker penalty hooks against private vehicles; no game writes."""
from pathlib import Path
import subprocess, time

root = Path(__file__).resolve().parents[3]
out = root/'tools/sdk/tests/evidence'/('broker_penalty_hold_'+str(time.time_ns()))
out.mkdir(parents=True)
broker = root/'mods/sdk_sandbox/game_update_broker'
code = r'''
#include <assert.h>
#include <stdio.h>
#include "f1c_hook.h"
#include "f1c_vehicle_struct.h"
#include "broker_penalty_hold.h"
static struct F1CVehicle cars[4];
static void *test_vehicle_slot(int n){return n>=0&&n<4?&cars[n]:NULL;}
#define f1c_vehicle_slot test_vehicle_slot
static int type=7,phase=5;
#define BROKER_PENALTY_SESSION_TYPE type
#define BROKER_PENALTY_RACE_PHASE phase
static CRITICAL_SECTION g_lock;
static DWORD g_hook_thread_id;
static uint32_t g_session_generation=9;
static int g_session_active=1,ready=1;
static int begin_api_call(void){return ready;}
static void end_api_call(void){}
static void *client_for_token_locked(F1CBrokerToken t){return t==17?(void*)1:NULL;}
#include "broker_penalty_hold_impl.h"
static int issue_calls,service_calls,consume_calls,lap_calls,nested,finish,deadline_at_native;
static F1CRestartPenalties *q(void *score){return (void*)((char*)score+0x3c);}
static int __attribute__((thiscall)) issue(void *score,const void *event){
    issue_calls++;assert(f1c_penalty_hold_issue(q(score),*((signed char*)event+8)));return 37;
}
static int __attribute__((thiscall)) service(void *score){(void)score;service_calls++;return 12;}
static int __attribute__((thiscall)) consume(void *score){
    consume_calls++;F1CRestartPenalties *s=q(score);
    if(s->count){s->count--;memmove(s->pair,s->pair+1,s->count*sizeof s->pair[0]);}return 13;
}
static void __attribute__((thiscall)) lap(void *score,float a,float b,int sector,int c){
    (void)a;(void)b;(void)c;lap_calls++;
    F1CRestartPenalties *s=q(score);deadline_at_native=s->pair[0][0];
    if(sector==0)for(int i=0;i<s->count;i++)if(s->pair[i][0])s->pair[i][0]--;
    if(nested){unsigned char event[9]={0};event[8]=10;assert(broker_penalty_issue_hook(score,event)==37);}
    if(finish)phase=8;
    assert(F1CBrokerPenaltyEnd(17)==F1C_BROKER_NOT_READY);
}
static void *score(int n){return (char*)&cars[n]+BROKER_PENALTY_SCORE_OFFSET;}
static void seed(void){
    memset(cars,0,sizeof cars);
    for(int n=0;n<4;n++){
        cars[n].DriverID=n+20;*(uintptr_t*)score(n)=(uintptr_t)&cars[n];
        *q(score(n))=(F1CRestartPenalties){1,{{2,10}}};
    }
}
static F1CRestartPenalties baseline[4],readback;
static uint32_t generation;
static uintptr_t vehicles[4];
static void begin(void){
    for(int i=0;i<4;i++)vehicles[i]=(uintptr_t)&cars[i];
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_OK);
    assert(generation==9&&baseline[0].pair[0][0]==2);
}
int main(void){
    InitializeCriticalSection(&g_lock);g_hook_thread_id=GetCurrentThreadId();g_penalty_supported=1;
    g_penalty_issue_original=issue;g_penalty_service_original=service;
    g_penalty_consume_original=consume;g_penalty_lap_original=lap;
    seed();begin();
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_CAPABILITY_IN_USE);
    assert(F1CBrokerPenaltyRead(99,vehicles[0],9,&readback)==F1C_BROKER_STALE_TOKEN);
    assert(F1CBrokerPenaltyRead(17,vehicles[0],8,&readback)==F1C_BROKER_STALE_SESSION);
    assert(broker_penalty_service_hook(score(0))==0&&service_calls==0);
    assert(broker_penalty_consume_hook(score(0))==0&&consume_calls==0);
    for(int i=0;i<5;i++)broker_penalty_lap_hook(score(0),90,100,0,4);
    assert(lap_calls==5&&deadline_at_native==3&&q(score(0))->pair[0][0]==2);
    unsigned char event[9]={0};event[8]=10;
    broker_penalty_issue_hook(score(0),event);broker_penalty_issue_hook(score(0),event);
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_OK);
    assert(readback.count==3&&readback.pair[0][0]==2&&readback.pair[1][0]==3);
    nested=1;broker_penalty_lap_hook(score(0),90,100,0,4);nested=0;
    assert(q(score(0))->count==4&&q(score(0))->pair[3][1]==10);
    broker_penalty_issue_hook(score(0),event);
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_OK);
    assert(readback.count==4&&readback.pair[3][1]==20);
    finish=1;broker_penalty_lap_hook(score(0),90,100,0,4);finish=0;
    assert(phase==8&&q(score(0))->pair[0][0]==2);phase=5;
    phase=3;q(score(0))->pair[0][0]=1;broker_penalty_lap_hook(score(0),90,100,0,4);
    assert(deadline_at_native==1&&q(score(0))->pair[0][0]==0);phase=5;
    cars[1].DriverID=88;
    assert(F1CBrokerPenaltyRead(17,vehicles[1],9,&readback)==F1C_BROKER_STALE_OBJECT);
    assert(broker_penalty_service_hook(score(1))==12&&service_calls==1);
    assert(F1CBrokerPenaltyEnd(17)==F1C_BROKER_OK);
    assert(broker_penalty_consume_hook(score(0))==13&&consume_calls==1);
    assert(!g_penalty_owner&&!g_penalty_count&&!g_active_penalty_calls);
    seed();vehicles[1]=vehicles[0];
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_STALE_OBJECT);
    assert(!g_penalty_owner);vehicles[1]=(uintptr_t)&cars[1];
    q(score(3))->count=5;
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_STALE_OBJECT);
    assert(!g_penalty_owner&&q(score(0))->pair[0][0]==2);
    seed();g_hook_thread_id++;
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_BAD_ARGUMENT);
    g_hook_thread_id--;ready=0;
    assert(F1CBrokerPenaltyBegin(17,vehicles,4,baseline,&generation)==F1C_BROKER_NOT_READY);ready=1;
    begin();g_session_generation++;
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_STALE_SESSION);
    assert(broker_penalty_service_hook(score(0))==12);
    assert(F1CBrokerPenaltyEnd(17)==F1C_BROKER_OK);
    g_session_generation=9;seed();begin();
    g_penalty_entries[0].queue=(F1CRestartPenalties){4,{{3,1},{3,1},{3,1},{3,3599}}};
    broker_penalty_issue_hook(score(0),event);
    assert(g_penalty_entries[0].fault);
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_STALE_OBJECT);
    broker_penalty_forget_locked();
    assert(!g_penalty_owner&&!g_penalty_count);
    seed();begin();
    assert(F1CBrokerPenaltySeal(17)==F1C_BROKER_OK);
    assert(F1CBrokerPenaltySeal(17)==F1C_BROKER_NOT_READY);
    broker_penalty_session_leave_locked(4);g_session_active=0;phase=3;
    assert(g_penalty_owner==17&&g_penalty_awaiting_restart);
    /* Native constructor clears scoring; a new event after that reset must
       append to the retained event ledger, even before first green callback. */
    *q(score(0))=(F1CRestartPenalties){0};
    broker_penalty_issue_hook(score(0),event);
    /* RF's first update may precede RestartGate's session-enter notification. */
    phase=4;
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_OK);
    phase=5;
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_OK);
    assert(readback.count==2&&readback.pair[0][0]==2);
    phase=8;
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_STALE_SESSION);
    phase=5;g_penalty_restart_sealed=0;
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_STALE_SESSION);
    g_penalty_restart_sealed=1;
    g_session_generation++;broker_penalty_session_enter_locked();g_session_active=1;
    assert(!g_penalty_awaiting_restart&&g_penalty_live_generation==10);
    assert(F1CBrokerPenaltyRead(17,vehicles[0],9,&readback)==F1C_BROKER_OK);
    assert(readback.count==2&&readback.pair[0][0]==2&&readback.pair[1][1]==10);
    phase=5;broker_penalty_lap_hook(score(0),90,100,0,4);
    assert(q(score(0))->count==2&&q(score(0))->pair[0][0]==2);
    broker_penalty_session_leave_locked(6);assert(!g_penalty_owner);
    g_session_generation=9;seed();begin();
    broker_penalty_session_leave_locked(4);assert(!g_penalty_owner); /* unsealed */
    puts("PASS actual broker hooks: deadline freeze, event accumulation, identical/new/full-queue issues, service suppression, nested calls, native finish, constructor passthrough, identity/session/token/thread refusal, atomic field capture, lifecycle release");
    return 0;
}
'''
(out/'test.c').write_text(code)
for i,cmd in enumerate([
    ['C:/msys64/mingw32/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra',
     '-I'+str(root/'tools/sdk'),'-I'+str(broker),str(out/'test.c'),'-o',str(out/'test.exe')],
    [str(out/'test.exe')]]):
    r=subprocess.run(cmd,capture_output=True,text=True)
    (out/(str(i)+'.txt')).write_text(r.stdout+r.stderr)
    print(r.stdout+r.stderr);r.check_returncode()
print(out)
