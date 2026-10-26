"""Actual broker preload dispatch/registration/unregister and receiver ABI test."""
from pathlib import Path
import datetime,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'mods/sdk_sandbox/game_update_broker/game_update_broker.c').read_text()
def function(name):
    m=re.search(r'^(?:static )?[^\n;]*\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert m,name
    b=source.index('{',m.start())+1;depth=1
    while depth:depth+=(source[b]=='{')-(source[b]=='}');b+=1
    return source[m.start():b]
a=source.index('typedef struct BrokerClient {');b=source.index('} BrokerClient;',a)+len('} BrokerClient;')
# Per-run build output is disposable (governance/RETENTION_POLICY.md): keep it out of the workspace.
out=Path(tempfile.gettempdir())/'f1c_sdk_test_output'/'preload'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
out.mkdir(parents=True)
code=r'''
#define F1C_BROKER_BUILD 1
#include "f1c_broker_abi.h"
#include <assert.h>
#include <stdio.h>
'''+source[a:b]+r'''
static BrokerClient g_clients[F1C_BROKER_MAX_CLIENTS];
static int g_lock,depth,g_ready=1,g_stopping,g_preload_supported=1,g_preload_dispatch_depth;
static DWORD g_hook_thread_id;
static volatile LONG g_active_preload_calls;
static int g_garage_results_pending;
static int seen,expected,clear_other,native_calls;
static F1CBrokerToken g_high_gear_ratio_owner;
static F1CBrokerToken g_penalty_owner;
static double g_high_gear_ratio;
static int g_high_gear_ratio_active;
static int begin_api_call(void){return g_ready;}
static void end_api_call(void){}
static int decode_token(F1CBrokerToken t,uint32_t*i,uint32_t*g){*i=t-1;*g=1;return t>0&&t<=16;}
static BrokerClient *client_for_token_locked(F1CBrokerToken t){
 if(!t||t>16||!g_clients[t-1].active||g_clients[t-1].retiring)return NULL;
 return &g_clients[t-1];
}
static void purge_token_locked(F1CBrokerToken t){(void)t;}
static void reclaim_retired_locked(void){}
#define EnterCriticalSection(p) ((void)(p),++depth)
#define LeaveCriticalSection(p) ((void)(p),--depth)
#define log_line(...) ((void)0)
typedef int (__attribute__((thiscall)) *PreloadFn)(void*);
static PreloadFn g_preload_original;
'''+ '\n'.join(function(n) for n in ['F1CBrokerUnregister','F1CBrokerSetPreload','run_preload_callbacks','broker_preload_hook'])+r'''
static void __cdecl callback(void *ctx){
 int i=(int)(intptr_t)ctx;
 assert(depth==0&&g_preload_dispatch_depth==1&&g_clients[i].in_flight==1);
 assert(g_active_preload_calls==1);
 assert(F1CBrokerUnregister(i+1,0)==F1C_BROKER_WOULD_DEADLOCK);
 seen|=1<<i;
 if(i==0&&clear_other){
   assert(g_clients[1].in_flight==1);
   assert(F1CBrokerUnregister(2,0)==F1C_BROKER_WOULD_DEADLOCK);
   assert(F1CBrokerSetPreload(2,NULL,NULL)==0);
 }
}
static int __attribute__((thiscall)) native(void *receiver){
 assert(receiver==(void*)0x12345678&&seen==expected);
 assert(g_active_preload_calls==1&&!g_preload_dispatch_depth);
 native_calls++;return 0x76543210;
}
int main(void){
 g_preload_original=native;
 for(int i=0;i<3;i++){g_clients[i].active=1;assert(F1CBrokerSetPreload(i+1,callback,(void*)(intptr_t)i)==0);}
 assert(F1CBrokerSetPreload(99,callback,NULL)==F1C_BROKER_STALE_TOKEN);
 expected=7;clear_other=1;g_garage_results_pending=1;
 assert(broker_preload_hook((void*)0x12345678)==0x76543210);
 assert(!g_active_preload_calls&&!g_preload_dispatch_depth&&!depth);
 assert(!g_garage_results_pending);
 for(int i=0;i<3;i++)assert(!g_clients[i].in_flight);
 clear_other=0;seen=0;expected=5;broker_preload_hook((void*)0x12345678);
 g_clients[2].retiring=1;seen=0;expected=1;broker_preload_hook((void*)0x12345678);
 assert(F1CBrokerSetPreload(3,callback,NULL)==F1C_BROKER_STALE_TOKEN);
 assert(F1CBrokerUnregister(1,0)==0);assert(!g_clients[0].preload&&!g_clients[0].preload_context);
 seen=0;expected=0;g_stopping=1;broker_preload_hook((void*)0x12345678);
 g_ready=0;assert(F1CBrokerSetPreload(2,callback,NULL)==F1C_BROKER_STOPPING);
 g_stopping=0;assert(F1CBrokerSetPreload(2,callback,NULL)==F1C_BROKER_NOT_READY);
 g_ready=1;g_preload_supported=0;assert(F1CBrokerSetPreload(2,callback,NULL)==F1C_BROKER_NOT_READY);
 assert(native_calls==4);
 puts("PASS: preload before native; ECX receiver/EAX result; reserved clients; deadlock refusal; snapshot retention; isolated clear; retiring/stopping/unsupported guards");
}
'''
assert 'client->preload = NULL;' in function('reclaim_retired_locked')
worker=function('broker_worker')
assert worker.index('&g_preload_detour, PRELOAD_ADDRESS') < worker.index('InterlockedExchange(&g_ready, 1)')
assert 'g_active_preload_calls != 0' in worker
assert 'DeleteFileA' not in worker
(out/'preload.c').write_text(code)
p=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'tools/sdk'),str(out/'preload.c'),'-o',str(out/'preload.exe')],capture_output=True,text=True)
log=p.stdout+p.stderr
if p.returncode==0:
    p=subprocess.run([str(out/'preload.exe')],capture_output=True,text=True);log+=p.stdout+p.stderr
(out/'RESULTS.txt').write_text(log);print(log);print(out)
raise SystemExit(p.returncode)
