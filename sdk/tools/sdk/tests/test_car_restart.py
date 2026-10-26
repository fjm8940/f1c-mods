"""Internal gate; invoke canonical f1c_mod.py sdk-test."""
from pathlib import Path
import datetime,re,subprocess
root=Path(__file__).resolve().parents[3]
o=root/'tools/sdk/tests/evidence'/('car_restart_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
h=(root/'tools/sdk/f1c_car_restart.h').read_text()
h=re.sub(r'static int f1c_car_restart_ready\(void\)\{.*?\n\}', 'static int f1c_car_restart_ready(void){return ready;}',h,flags=re.S)
h=h.replace('0x004C6D90u','mock_restart').replace('0x00424790u','mock_ai')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static int ready=1,native_calls,ai_calls;static unsigned char v[0x48c0];
static void __attribute__((thiscall)) mock_restart(void *p,uint32_t f){assert(p==v&&f==1);native_calls++;}
static void __attribute__((thiscall)) mock_ai(void *p,uint32_t f){assert(p==v&&f==1);ai_calls++;}
'''
main=r'''
int main(void){assert(f1c_car_restart(v)&&native_calls==1);assert(f1c_car_rebind_ai(v)&&ai_calls==1);
ready=0;assert(!f1c_car_restart(v)&&!f1c_car_rebind_ai(v));assert(native_calls==1&&ai_calls==1);
ready=1;assert(!f1c_car_restart(0)&&!f1c_car_rebind_ai(0));
puts("PASS per-car wrapper: x86 thiscall receiver, integer flag1, null/code refusal; native gameplay pending");}
'''
(o/'test.c').write_text(prefix+h+main)
r=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11',str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=r.stdout+r.stderr
if not r.returncode:r=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=r.stdout+r.stderr
(o/'RESULTS.txt').write_text(log);print(log);assert r.returncode==0
