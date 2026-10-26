from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3]
o=r/'tools/sdk/tests/evidence'/('race_control_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
cc=[r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-Wall','-Werror','-Wno-unused-function','-I'+str(r/'tools/sdk')]
dll=r"""
#include "f1c_race_control.h"
__declspec(dllexport) int claim(int actor){F1CRaceControl*s=f1c_rc_open();return actor==1?f1c_rc_sc_enter(s):f1c_rc_rf_acquire(s);}
__declspec(dllexport) void relinquish(int actor){F1CRaceControl*s=f1c_rc_open();if(actor==1)f1c_rc_sc_release(s);else f1c_rc_rf_release(s);}
__declspec(dllexport) int owner(void){F1CRaceControl*s=f1c_rc_open();return s?(int)f1c_rc_read(&s->owner):-1;}
"""
exe=r"""
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char**argv){
 CHECK(argc==3);HMODULE a=LoadLibraryA(argv[1]),b=LoadLibraryA(argv[2]);CHECK(a&&b);
 int(*ca)(int)=(void*)GetProcAddress(a,"claim"),(*cb)(int)=(void*)GetProcAddress(b,"claim");
 void(*ra)(int)=(void*)GetProcAddress(a,"relinquish"),(*rb)(int)=(void*)GetProcAddress(b,"relinquish");
 int(*oa)(void)=(void*)GetProcAddress(a,"owner"),(*ob)(void)=(void*)GetProcAddress(b,"owner");
 CHECK(ca&&cb&&ra&&rb&&oa&&ob);CHECK(oa()==0 && ob()==0);
 CHECK(ca(1));CHECK(!cb(2));CHECK(oa()==1 && ob()==1);
 for(int i=0;i<1000;i++)CHECK(!cb(2)); /* no stale-owner timeout */
 ra(1);CHECK(cb(2));CHECK(!ca(1));CHECK(oa()==2 && ob()==2);
 rb(2);CHECK(ca(1));ra(1);CHECK(oa()==0 && ob()==0);
 FreeLibrary(a);FreeLibrary(b);puts("PASS SDK cross-DLL process-scoped mapping, exclusive acquisition, acknowledged handoff, stale owner retention, clean recovery");return 0;
}
"""
(o/'client.c').write_text(dll);(o/'test.c').write_text(exe)
log=''
for cmd in [cc+['-shared',str(o/'client.c'),'-o',str(o/'sc.dll'),'-luser32'],cc+['-shared',str(o/'client.c'),'-o',str(o/'rf.dll'),'-luser32'],cc+[str(o/'test.c'),'-o',str(o/'test.exe')],[str(o/'test.exe'),str(o/'sc.dll'),str(o/'rf.dll')]]:
 a=subprocess.run(cmd,capture_output=True,text=True,timeout=30);log+=a.stdout+a.stderr
 if a.returncode:break
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);raise SystemExit(a.returncode)
