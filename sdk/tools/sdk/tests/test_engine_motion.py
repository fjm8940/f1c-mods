from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3]
o=r/'tools/sdk/tests/evidence'/('engine_motion_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r'''
#include "f1c_engine_motion.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static float value;static int readable=1;
static int rd(uintptr_t a,void*p,size_t n){assert(a==0x1039a0&&n==4);if(!readable)return 0;memcpy(p,&value,n);return 1;}
int main(void){value=0;assert(f1c_engine_motion(rd,0x100000)==2);value=1000;assert(f1c_engine_motion(rd,0x100000)==1);value=NAN;assert(!f1c_engine_motion(rd,0x100000));value=INFINITY;assert(!f1c_engine_motion(rd,0x100000));value=-1;assert(!f1c_engine_motion(rd,0x100000));value=100;readable=0;assert(!f1c_engine_motion(rd,0x100000));assert(!f1c_engine_motion(0,0x100000));assert(!f1c_engine_motion(rd,UINT32_MAX));puts("PASS SDK engine motion exact field/zero/positive/invalid/unreadable/overflow");}
'''
(o/'test.c').write_text(s,encoding='utf-8')
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Werror','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);raise SystemExit(a.returncode)
