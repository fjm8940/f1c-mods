from pathlib import Path
import subprocess, time
r=Path(__file__).resolve().parents[3]
o=r/'tools/sdk/tests/evidence'/('restart_penalties_'+str(time.time_ns()));o.mkdir(parents=True)
code=r'''
#include <assert.h>
#include <stdio.h>
#include "f1c_restart_penalties.h"
int main(void){
 unsigned char v[0x48c0],before[0x48c0];memset(v,0xa5,sizeof v);
 F1CRestartPenalties saved={2,{{2,10},{1,20},{0,0},{0,0}}},got={0};
 memcpy(v+F1C_OFF_noOfPenaltiesToProcess,&saved,sizeof saved);
 assert(f1c_restart_penalties_capture(v,sizeof v,&got));assert(!memcmp(&got,&saved,sizeof got));
 memset(v+F1C_OFF_noOfPenaltiesToProcess,0,sizeof saved);memcpy(before,v,sizeof v);
 assert(f1c_restart_penalties_restore(v,sizeof v,&saved));
 assert(!memcmp(v,before,F1C_OFF_noOfPenaltiesToProcess));
 assert(!memcmp(v+F1C_OFF_noOfPenaltiesToProcess+sizeof saved,before+F1C_OFF_noOfPenaltiesToProcess+sizeof saved,sizeof v-F1C_OFF_noOfPenaltiesToProcess-sizeof saved));
 assert(f1c_restart_penalties_capture(v,sizeof v,&got));assert(got.count==2&&got.pair[0][0]==2&&got.pair[1][0]==1);
 /* The current lap can change across a restart: stored countdown stays two. */
 *(int*)(v+F1C_OFF_lapNumber)=51;assert(got.pair[0][0]==2);
 for(int n=-1;n<=5;n+=6){saved.count=n;memcpy(before,v,sizeof v);assert(!f1c_restart_penalties_restore(v,sizeof v,&saved));assert(!memcmp(v,before,sizeof v));}
 saved.count=1;saved.pair[0][0]=4;assert(!f1c_restart_penalties_restore(v,sizeof v,&saved));
 saved.pair[0][0]=2;saved.pair[0][1]=-1;assert(!f1c_restart_penalties_restore(v,sizeof v,&saved));
 saved.pair[0][1]=10;assert(!f1c_restart_penalties_restore(v,F1C_OFF_noOfPenaltiesToProcess+4,&saved));
 saved.count=0;assert(f1c_restart_penalties_restore(v,sizeof v,&saved));
 puts("PASS exact penalty-only roundtrip, remaining-lap deadline, untouched neighbours, malformed/short-buffer refusal");
 return 0;
}
'''
(o/'test.c').write_text(code)
commands=[['C:/msys64/mingw32/bin/gcc.exe','-std=c11','-O2','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],[str(o/'test.exe')]]
log=''
for cmd in commands:
 a=subprocess.run(cmd,capture_output=True,text=True);log+=a.stdout+a.stderr
 if a.returncode:break
(o/'RESULTS.txt').write_text(log);print(log);raise SystemExit(a.returncode)
