from pathlib import Path
import subprocess, datetime
r=Path(__file__).resolve().parents[3];o=r/'tools/sdk/tests/evidence'/('session_guard_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r'''#include "f1c_session_guard.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int session=7, game=5, race=5, pause=0, out=0, net=0, readable=1; static uint32_t mgr=0x1000;
static int rd(uintptr_t a,void*d,size_t n){ if(!readable)return 0; if(a==0x007C56A4u){memcpy(d,&session,n);return 1;} if(a==0x007C569Cu){memcpy(d,&game,n);return 1;} if(a==0x007C56A8u){memcpy(d,&race,n);return 1;} if(a==0x007C56ACu){memcpy(d,&pause,n);return 1;} if(a==0x007CE258u){memcpy(d,&out,n);return 1;} if(a==0x007CD25Cu){memcpy(d,&mgr,n);return 1;} if(a==mgr+16324u){memcpy(d,&net,n);return 1;} return 0; }
int main(void){F1CSessionGuard g=f1c_session_guard_read(rd);assert(g.valid&&g.offline&&g.settled&&!g.player_out); net=1;g=f1c_session_guard_read(rd);assert(g.valid&&!g.offline);net=0;pause=1;g=f1c_session_guard_read(rd);assert(!g.settled);pause=0;out=1;g=f1c_session_guard_read(rd);assert(g.player_out);out=0;mgr=0;g=f1c_session_guard_read(rd);assert(!g.valid&&!g.offline);mgr=0xfffffff0u;g=f1c_session_guard_read(rd);assert(!g.valid&&!g.settled);mgr=0x1000;readable=0;g=f1c_session_guard_read(rd);assert(!g.valid&&!g.offline&&!g.settled);puts("PASS session guard: offline, pause/menu, null/overflow/unreadable state fail closed");}'''
(o/'session_guard.c').write_text(s);x=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(r/'tools/sdk'),str(o/'session_guard.c'),'-o',str(o/'session_guard.exe')],capture_output=True,text=True);log=x.stdout+x.stderr
if not x.returncode:x=subprocess.run([str(o/'session_guard.exe')],capture_output=True,text=True);log+=x.stdout+x.stderr
(o/'RESULTS.txt').write_text(log);print(log);raise SystemExit(x.returncode)
