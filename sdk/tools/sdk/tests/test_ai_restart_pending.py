from pathlib import Path
import subprocess, datetime
r=Path(__file__).resolve().parents[3]
o=r/'tools/sdk/tests/evidence'/('ai_restart_pending_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
o.mkdir(parents=True)
code=r'''
#include "f1c_ai_restart_pending.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static float deadline; static int readable=1;
static int rd(uintptr_t a,void*p,size_t n){assert(a==0x104170 && n==4);if(!readable)return 0;memcpy(p,&deadline,4);return 1;}
int main(void){
 deadline=130;assert(f1c_ai_restart_pending_read(rd,0x100000,100).pending);
 assert(f1c_ai_restart_pending_read(rd,0x100000,130).pending);
 assert(!f1c_ai_restart_pending_read(rd,0x100000,130.01).pending);
 deadline=FLT_MAX;assert(!f1c_ai_restart_pending_read(rd,0x100000,100).pending);
 deadline=0;assert(!f1c_ai_restart_pending_read(rd,0x100000,0).pending);
 deadline=NAN;assert(!f1c_ai_restart_pending_read(rd,0x100000,100).valid);
 deadline=INFINITY;assert(!f1c_ai_restart_pending_read(rd,0x100000,100).valid);
 deadline=-1;assert(!f1c_ai_restart_pending_read(rd,0x100000,100).valid);
 deadline=130;readable=0;assert(!f1c_ai_restart_pending_read(rd,0x100000,100).valid);
 assert(!f1c_ai_restart_pending_read(0,0x100000,100).valid);
 assert(!f1c_ai_restart_pending_read(rd,0,100).valid);
 assert(!f1c_ai_restart_pending_read(rd,UINT32_MAX,100).valid);
 assert(!f1c_ai_restart_pending_read(rd,0x100000,NAN).valid);
 puts("PASS native restart deadline: future/equality/expiry/sentinel/invalid/unreadable/overflow");
}
'''
(o/'test.c').write_text(code,encoding='utf-8')
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Werror','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True)
log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);raise SystemExit(a.returncode)
