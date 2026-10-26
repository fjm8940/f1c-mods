"""Canonical SDK gate: compile actual broker callback dispatch with deterministic clock."""
from pathlib import Path
import datetime,subprocess
root=Path(__file__).resolve().parents[3]
s=(root/'mods/sdk_sandbox/game_update_broker/game_update_broker.c').read_text()
a=s.index('__declspec(dllexport) F1CBrokerResult __cdecl F1CBrokerNativeBudget(');b=s.index('F1CBrokerResult __cdecl F1CBrokerSetPreload(',a)
api=s[a:b]
a=s.index('        if (task.kind == BROKER_TASK_CALLBACK) {',s.index('while (executed < F1C_BROKER_UPDATE_BUDGET)'))
b=s.index(' else if (task.kind == BROKER_TASK_SET_CALL_TO_BOX',a)
dispatch=s[a:b]
prefix=r'''
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "f1c_broker_abi.h"
#include "broker_callback_timing.h"
static DWORD thread=7;static LONG g_hook_thread_id=7;
#define GetCurrentThreadId() thread
static F1CBrokerToken g_callback_token;
static uint32_t g_callback_budget_us;
static int locked,cleanup_count,mode,flight=1;
static uint64_t clock_value, ordinary_duration=20000;
static int g_lock;
#define EnterCriticalSection(p) (++locked)
#define LeaveCriticalSection(p) (--locked)
#define log_line(...) ((void)0)
static uint64_t qpc_now(void){return clock_value;}
static uint64_t qpc_elapsed_us(uint64_t a,uint64_t b){return b-a;}
typedef struct {char name[48];F1CBrokerTaskFn recovery;void *recovery_context; unsigned small_overrun;DWORD callback_warning_time;int callback_warning_seen;} BrokerClient;
#define BROKER_TASK_CALLBACK 1
static void recovery(void *ctx){assert(ctx==(void*)42);assert(!locked && flight==1);cleanup_count++;}
'''
main=r'''
static void work(void *ctx){(void)ctx;
 if(mode){assert(F1CBrokerNativeBudget(11)==F1C_BROKER_OK);}
 clock_value += mode==2?250001:(mode?20000:ordinary_duration);
}
static int run(int which){mode=which;uint64_t callback_start=0,callback_us=0;
 uint32_t callback_budget_us=F1C_BROKER_SLOW_CALLBACK_US;
 static BrokerClient c={"test",recovery,(void*)42,0}; BrokerClient *client=&c;
 int callback_quarantine=0;
 struct {int kind; F1CBrokerToken token;F1CBrokerTaskFn function;void *context;} task={1,11,work,0};
 DISPATCH
 assert(g_callback_token==0 && g_callback_budget_us==0);
 return callback_quarantine;
}
int main(void){
 assert(F1CBrokerNativeBudget(11)==F1C_BROKER_BAD_ARGUMENT);
 g_callback_token=11;thread=8;assert(F1CBrokerNativeBudget(11)==F1C_BROKER_BAD_ARGUMENT);
 thread=7;assert(F1CBrokerNativeBudget(12)==F1C_BROKER_BAD_ARGUMENT);g_callback_token=0;
 ordinary_duration=100001;assert(run(0) && cleanup_count==1);
 assert(!run(1) && cleanup_count==1); /* native20ms allowed */
 assert(run(2) && cleanup_count==2); /* native250ms hard bound */
 for(int i=0;i<100;i++){ordinary_duration=6196;assert(!run(0));}
 assert(cleanup_count==2); /* sustained mild overruns never shut down */
 ordinary_duration=20000;assert(!run(0));
 ordinary_duration=20001;
 for(int i=0;i<9;i++)assert(!run(0));
 assert(!run(1)); /* native call cannot reset ordinary streak */
 assert(run(0) && cleanup_count==3);
 ordinary_duration=20000;assert(!run(0));
 ordinary_duration=100000;assert(!run(0)); /* inclusive boundary */
 ordinary_duration=5000;assert(!run(0)); /* resets streak */
 ordinary_duration=20001;for(int i=0;i<9;i++)assert(!run(0));
 ordinary_duration=6196;assert(!run(0));
 ordinary_duration=20001;for(int i=0;i<9;i++)assert(!run(0));
 assert(run(0) && cleanup_count==4);
 puts("PASS callback budget scope, token/thread denial, ordinary/native containment and unlocked in-flight recovery");
}
'''.replace('DISPATCH',dispatch)
o=root/'tools/sdk/tests/evidence'/('callback_recovery_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
(o/'test.c').write_text(prefix+api+main)
x=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(root/'tools/sdk'),'-I'+str(root/'mods/sdk_sandbox/game_update_broker'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=x.stdout+x.stderr
if not x.returncode:x=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=x.stdout+x.stderr
(o/'RESULTS.txt').write_text(log);print(log);assert x.returncode==0
