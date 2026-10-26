"""Execute reviewed SDK boundary and actual Safety Car predicate/hook against doubles."""
from pathlib import Path
import datetime,re,subprocess
root=Path(__file__).resolve().parents[3]
trial=root.parent/'development/mods/full_safety_car/trials/v0.9.8_local_yellow'
s=(trial/'safety_car_auditfix.c').read_text()
def function(name):
 m=re.search(r'^static [^\n;]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 b=s.index('{',m.start())+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[m.start():b]
out=trial/'evidence'/('local_yellow_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));out.mkdir(parents=True)
code=r'''
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "f1c_local_yellow.h"
#undef F1C_LOCAL_YELLOW_ARRAY_POINTER
#undef F1C_LOCAL_YELLOW_ZONE_COUNT
static int zones[8],native_count=4,foreign;
static uintptr_t native_base;
#define F1C_LOCAL_YELLOW_ARRAY_POINTER (&native_base)
#define F1C_LOCAL_YELLOW_ZONE_COUNT (&native_count)
#define logf_(...) ((void)0)
static int g_stop_requested,g_hooks_ready=1,g_engaged,sc_out,game_thread=1,racing=1;
static int on_game_thread(void){return game_thread;}
static int phase_racing(void){return racing;}
static int sc_is_out(void){return sc_out;}
typedef int (__attribute__((thiscall)) *local_yellow_set_t)(void *,int);
static local_yellow_set_t volatile g_local_yellow_original;
static volatile LONG g_local_yellow_filtered;
static int __attribute__((thiscall)) original(void *zone,int state){*(int*)zone=state;return state;}
typedef struct {int installed;void*trampoline;} F1CDetour;
typedef int F1CDetourResult;
#define F1C_DETOUR_OK 0
static F1CDetour g_local_yellow_hook;
static int install_result,install_calls;
static int f1c_detour_install(F1CDetour*h,void*address,void*replacement,const void*bytes,size_t size){
 (void)replacement;assert(address==F1C_LOCAL_YELLOW_SET_ADDRESS&&size==6);
 assert(!memcmp(bytes,"\x8b\x44\x24\x04\x89\x01",6));install_calls++;
 if(!install_result){h->installed=1;h->trampoline=(void*)original;}return install_result;
}
'''+ '\n'.join(function(n) for n in ['local_yellow_suppressed','local_yellow_detour','local_yellow_install'])+r'''
int main(void){
 uintptr_t base=(uintptr_t)zones;native_base=base;
 install_result=3;assert(!local_yellow_install()&&!g_local_yellow_original);
 install_result=0;assert(local_yellow_install()&&g_local_yellow_original==original);
 assert(local_yellow_install()&&install_calls==2);
 assert(local_yellow_detour(zones,2)==2&&zones[0]==2); /* normal yellow */
 g_engaged=1;assert(local_yellow_detour(zones,2)==0&&zones[0]==0);
 assert(local_yellow_detour(zones+6,2)==0); /* last zone */
 assert(local_yellow_detour(zones,0)==0&&local_yellow_detour(zones,1)==1);
 assert(local_yellow_detour(&foreign,2)==2&&foreign==2);
 assert(local_yellow_detour(zones+1,2)==2); /* end-distance, not state */
 for(int mode=0;mode<5;mode++){
  g_stop_requested=mode==0;g_hooks_ready=mode!=1;game_thread=mode!=2;racing=mode!=3;g_engaged=mode!=4;
  assert(local_yellow_detour(zones,2)==2);
 }
 g_stop_requested=0;g_hooks_ready=game_thread=racing=1;g_engaged=0;sc_out=1;
 assert(local_yellow_detour(zones,2)==0); /* SC still out after pace release */
 sc_out=0;assert(local_yellow_detour(zones,2)==2); /* green resumes normal detector */
 assert(f1c_local_yellow_filter_state(base+32,base,4,2,1)==2);
 assert(f1c_local_yellow_filter_state(base-8,base,4,2,1)==2);
 assert(f1c_local_yellow_filter_state(base,0,4,2,1)==2);
 assert(f1c_local_yellow_filter_state(base,base,0,2,1)==2);
 assert(f1c_local_yellow_filter_state(base,base,513,2,1)==2);
 assert(f1c_local_yellow_filter_state(base,base,4,2,0)==2);
 assert(g_local_yellow_filtered==3);
 puts("PASS: native thiscall receiver/state/result, only current zone states filtered, managed pace/SC out, green/stop/session/wrong-thread pass-through, install failure and idempotence");
}
'''
assert 'if (!local_yellow_install()) return;' in function('hook_install')
assert 'g_hooks_ready = 0;' in function('hook_remove')
assert 'f1c_detour_remove' not in function('hook_remove')
assert 'yellow_set(1)' in function('sc_deploy')
(out/'local_yellow.c').write_text(code);logs=[]
for cmd in [[r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'tools/sdk'),str(out/'local_yellow.c'),'-o',str(out/'local_yellow.exe')],[str(out/'local_yellow.exe')]]:
 p=subprocess.run(cmd,capture_output=True,text=True);logs.append(p.stdout+p.stderr);(out/'RESULTS.txt').write_text('\n'.join(logs))
 if p.returncode:print('\n'.join(logs));raise SystemExit(p.returncode)
print('\n'.join(logs));print(out)
