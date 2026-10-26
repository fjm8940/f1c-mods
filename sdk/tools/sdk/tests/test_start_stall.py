from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3]
o=r/'tools/sdk/tests/evidence'/('start_stall_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r'''
#include "f1c_start_stall.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int32_t pairs[10];static int readable=1;
static int rd(uintptr_t a,void*p,size_t n){assert(a==0x104144&&n==40);if(!readable)return 0;memcpy(p,pairs,n);return 1;}
int main(void){assert(!f1c_start_stall_dns(rd,0x100000));pairs[1]=4;assert(!f1c_start_stall_dns(rd,0x100000));pairs[1]=5;assert(f1c_start_stall_dns(rd,0x100000));pairs[0]=1;assert(!f1c_start_stall_dns(rd,0x100000));pairs[9]=5;assert(f1c_start_stall_dns(rd,0x100000));readable=0;assert(!f1c_start_stall_dns(rd,0x100000));assert(!f1c_start_stall_dns(0,0x100000));assert(!f1c_start_stall_dns(rd,0));assert(!f1c_start_stall_dns(rd,UINT32_MAX));puts("PASS SDK start-stall reader exact bounds, type/condition, last pair, unreadable/null/overflow");}
'''
(o/'test.c').write_text(s,encoding='utf-8')
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Werror','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);raise SystemExit(a.returncode)
