"""Internal gate; public invocation is f1c_mod.py sdk-test."""
from pathlib import Path
import subprocess,datetime
root=Path(__file__).resolve().parents[3]
o=root/'tools/sdk/tests/evidence'/('ai_interaction_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
code=r'''
#include "f1c_ai_interaction.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned char v[0x48c0]={0},before[0x48c0];F1CAIInteraction a={0};
 uint32_t ptr=0xdeadbeef;float side=-2.5f,passing=.9f;
 memcpy(v+0x45fc,&ptr,4);memcpy(v+0x4420,&side,4);memcpy(v+0x44cc,&passing,4);
 memcpy(before,v,sizeof(v));assert(f1c_ai_interaction_read(v,sizeof(v),&a));
 assert(a.target_address==ptr&&a.lateral_request==side&&a.passing==passing);
 assert(!memcmp(v,before,sizeof(v)));F1CAIInteraction saved=a;
 assert(!f1c_ai_interaction_read(0,sizeof(v),&a));assert(!f1c_ai_interaction_read(v,0x45ff,&a));
 assert(!f1c_ai_interaction_read(v,sizeof(v),0));
 passing=NAN;memcpy(v+0x44cc,&passing,4);assert(!f1c_ai_interaction_read(v,sizeof(v),&a));
 assert(!memcmp(&a,&saved,sizeof(a)));
 puts("PASS interaction accessor: bounded read, opaque target, finite fields, immutable input and failure output");
}
'''
(o/'test.c').write_text(code)
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(root/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True)
log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log);print(log);assert a.returncode==0
