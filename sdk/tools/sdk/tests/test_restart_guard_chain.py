"""Execute actual broker guard functions with isolated client/lock doubles."""
from pathlib import Path
import datetime, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[3]
SDK=ROOT/'tools/sdk'
source=(ROOT/'mods/sdk_sandbox/game_update_broker/game_update_broker.c').read_text()
def function(name):
    import re
    m=re.search(r'^(?:static )?[^\n;]*\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert m,name
    b=source.index('{',m.start())+1;depth=1
    while depth:
        depth+=(source[b]=='{')-(source[b]=='}');b+=1
    return source[m.start():b]
a=source.index('typedef struct BrokerClient {');b=source.index('} BrokerClient;',a)+len('} BrokerClient;')
# Per-run build output is disposable (governance/RETENTION_POLICY.md): keep it out of the workspace.
out=Path(tempfile.gettempdir())/'f1c_sdk_test_output'/'guard_chain'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
out.mkdir(parents=True)
code=r'''
#define F1C_BROKER_BUILD 1
#include "f1c_broker_abi.h"
#include <assert.h>
#include <stdio.h>
'''+source[a:b]+r'''
static BrokerClient g_clients[F1C_BROKER_MAX_CLIENTS];
static int g_lock,depth,g_stopping,ready=1,seen,block,clear_other;
static int begin_api_call(void){return ready;}
static void end_api_call(void){}
static BrokerClient *client_for_token_locked(F1CBrokerToken t){
    if(!t || t>F1C_BROKER_MAX_CLIENTS || !g_clients[t-1].active || g_clients[t-1].retiring)return NULL;
    return &g_clients[t-1];
}
#define EnterCriticalSection(p) ((void)(p),++depth)
#define LeaveCriticalSection(p) ((void)(p),--depth)
'''+function('F1CBrokerSetRestartGuard')+'\n'+function('run_restart_guards')+r'''
static F1CBrokerRestartDecision __cdecl guard(void *ctx,uint32_t gen,int32_t phase){
    int i=(int)(intptr_t)ctx;
    assert(depth==0 && gen==9 && phase==5);
    assert(g_clients[i].in_flight==1); seen|=1<<i;
    if(i==0 && clear_other){
        assert(g_clients[1].in_flight==1);
        assert(F1CBrokerSetRestartGuard(2,NULL,NULL)==F1C_BROKER_OK);
    }
    return block==i+1 ? F1C_BROKER_RESTART_BLOCK : F1C_BROKER_RESTART_ALLOW;
}
int main(void){
    assert(run_restart_guards(9,5)==F1C_BROKER_RESTART_ALLOW);
    for(int i=0;i<3;i++)g_clients[i].active=1;
    assert(F1CBrokerSetRestartGuard(99,guard,0)==F1C_BROKER_STALE_TOKEN);
    for(int i=0;i<3;i++)assert(F1CBrokerSetRestartGuard(i+1,guard,(void*)(intptr_t)i)==F1C_BROKER_OK);
    for(block=0;block<=3;block++){
        seen=0;assert(run_restart_guards(9,5)==(block?F1C_BROKER_RESTART_BLOCK:F1C_BROKER_RESTART_ALLOW));
        assert(seen==7);for(int i=0;i<3;i++)assert(g_clients[i].in_flight==0);
    }
    block=0;clear_other=1;seen=0;assert(run_restart_guards(9,5)==0 && seen==7);
    clear_other=0;seen=0;assert(run_restart_guards(9,5)==0 && seen==5);
    g_clients[2].retiring=1;seen=0;assert(run_restart_guards(9,5)==0 && seen==1);
    assert(F1CBrokerSetRestartGuard(3,guard,0)==F1C_BROKER_STALE_TOKEN);
    assert(F1CBrokerSetRestartGuard(1,NULL,NULL)==0);seen=0;assert(run_restart_guards(9,5)==0 && !seen);
    ready=0;assert(F1CBrokerSetRestartGuard(1,guard,0)==F1C_BROKER_NOT_READY);
    g_stopping=1;assert(F1CBrokerSetRestartGuard(1,guard,0)==F1C_BROKER_STOPPING);
    assert(depth==0);
    puts("PASS: multiple clients, all-allow/any-block, no short circuit, isolated clear, snapshot retention, in-flight drain, stale/retiring/not-ready/stopping");
}
'''
assert 'client->restart_guard = NULL;' in function('F1CBrokerUnregister')
assert 'client->restart_guard_context = NULL;' in function('F1CBrokerUnregister')
assert 'client->restart_guard = NULL;' in function('reclaim_retired_locked')
assert 'client->restart_guard_context = NULL;' in function('reclaim_retired_locked')
(out/'guard_chain.c').write_text(code)
p=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I',str(SDK),str(out/'guard_chain.c'),'-o',str(out/'guard_chain.exe')],capture_output=True,text=True)
log=p.stdout+p.stderr
if p.returncode==0:
    p=subprocess.run([str(out/'guard_chain.exe')],capture_output=True,text=True);log+=p.stdout+p.stderr
(out/'RESULTS.txt').write_text(log);print(log);print(out)
raise SystemExit(p.returncode)
