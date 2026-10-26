"""Internal gate: use tools/f1c_mod.py sdk-test, not this file directly."""
from pathlib import Path
import datetime
import subprocess

sdk=Path(__file__).resolve().parents[1]
out=sdk/'tests/evidence'/('race_data_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
out.mkdir(parents=True)
source=r'''
#include "f1c_race_data_observation.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned char b[VEH_STRIDE]={0}; F1CRaceDataExtra out;
 float lap=89.123f; int hits=5;
 memcpy(b+(0x4714u-1736u),&lap,4);memcpy(b+(0x4714u-13408u),&hits,4);
 assert(!f1c_race_data_decode(NULL,sizeof(b),&out));
 assert(!f1c_race_data_decode(b,sizeof(b),NULL));
 assert(!f1c_race_data_decode(b,sizeof(b)-1,&out));
 assert(f1c_race_data_decode(b,sizeof(b),&out));
 assert(out.last_lap==lap&&out.hits==hits);
 memset(b+F1C_OFF_VehiclePath,'a',128);f1c_race_data_decode(b,sizeof(b),&out);
 assert(out.veh_file[127]==0);
 puts("PASS read-only race data buffer decoder: bounds, nulls, observed offsets, bounded strings");
}
'''
(out/'test.c').write_text(source)
cmd=[r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(sdk),str(out/'test.c'),'-o',str(out/'test.exe')]
r=subprocess.run(cmd,capture_output=True,text=True);log=r.stdout+r.stderr
if r.returncode==0:
    r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True);log+=r.stdout+r.stderr
(out/'RESULTS.txt').write_text(log);print(log);raise SystemExit(r.returncode)
