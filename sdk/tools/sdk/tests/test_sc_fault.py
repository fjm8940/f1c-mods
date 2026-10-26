from pathlib import Path
import subprocess,time
r=Path(__file__).resolve().parents[3];out=r/'tools/sdk/tests/evidence'/('sc-fault-'+str(time.time_ns()));out.mkdir()
cc=[r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-I'+str(r/'tools/sdk')]
client=r'''
#include "f1c_sc_fault.h"
__declspec(dllexport) void* channel(void){return f1c_scf_open();}
__declspec(dllexport) int send_fault(int gen,int why){return f1c_scf_publish(f1c_scf_open(),gen,1234,why);}
__declspec(dllexport) int peek(int gen){F1CSCFaultRequest r={0};return f1c_scf_peek(f1c_scf_open(),gen,&r)?r.sequence:0;}
__declspec(dllexport) int ack(int seq){return f1c_scf_ack(f1c_scf_open(),seq);}
'''
test=r'''
#include "f1c_sc_fault.h"
#include <stdio.h>
#define CHECK(c) do{if(!(c)){printf("FAIL %d %s\n",__LINE__,#c);return 1;}}while(0)
int main(int argc,char**argv){HMODULE a=LoadLibraryA(argv[1]),b=LoadLibraryA(argv[2]);CHECK(a&&b);
 int(*send)(int,int)=(void*)GetProcAddress(a,"send_fault");int(*peek)(int)=(void*)GetProcAddress(b,"peek");int(*ack)(int)=(void*)GetProcAddress(b,"ack");
 F1CSCFault*(*channel)(void)=(void*)GetProcAddress(a,"channel");CHECK(send&&peek&&ack&&channel);
 CHECK(!peek(7));CHECK(!send(0,1));CHECK(!send(7,99));CHECK(send(7,2)==1);CHECK(peek(7)==1&&!peek(8));
 CHECK(!send(7,1));CHECK(!ack(2)&&peek(7)==1);CHECK(ack(1)&&!peek(7));
 CHECK(send(8,3)==2&&!ack(1)&&peek(8)==2);f1c_scf_reset(channel());CHECK(!peek(8));
 F1CSCFault local={0};F1CSCFaultRequest r={0};local.state=-1;CHECK(!f1c_scf_peek(&local,1,&r));
 local.state=1;local.generation=1;local.reason=99;CHECK(!f1c_scf_peek(&local,1,&r));
 local.state=0;local.sequence=0x3fffffff;CHECK(!f1c_scf_publish(&local,1,1,1)&&local.state==0);
 CHECK(!f1c_scf_ack(NULL,1)&&!f1c_scf_peek(NULL,1,&r));
 puts("PASS distinct cross-DLL SC fault channel: generation, once pending, stale ack, busy/malformed, wrap refusal and reset; no ownership side effects");return 0;}
'''
(out/'client.c').write_text(client);(out/'test.c').write_text(test)
commands=[cc+['-shared',str(out/'client.c'),'-o',str(out/n),'-luser32'] for n in ('sc.dll','rf.dll')]
commands+=[cc+[str(out/'test.c'),'-o',str(out/'test.exe'),'-luser32'],[str(out/'test.exe'),str(out/'sc.dll'),str(out/'rf.dll')]]
log=''
for cmd in commands:
 result=subprocess.run(cmd,capture_output=True,text=True,timeout=30);log+=result.stdout+result.stderr
 if result.returncode:break
(out/'RESULTS.txt').write_text(log);print(log);result.check_returncode()
