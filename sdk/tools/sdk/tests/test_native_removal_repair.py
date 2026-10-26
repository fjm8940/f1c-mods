from pathlib import Path
import json, subprocess, time

root=Path(__file__).resolve().parents[4]
trial=root/'development/mods/full_safety_car/trials/v0.10.57_native_removal'
out=trial/('native-removal-tests-'+str(time.time_ns()));out.mkdir()
capture=json.loads((trial.parent/'v0.10.56_fault_redflag/live-trulli-positions-1.json').read_text())
row=next(x for x in capture['position_inventory']['rows'] if x['slot']==5)
(out/'trulli.bin').write_bytes(bytes.fromhex(row['vehicle_bytes_hex']))
code=r'''
#include <stdio.h>
#include "f1c_native_removal_repair.h"
static unsigned char car[0x48c0],baseline[0x48c0];
static int denied,race_change;
static int rd(uintptr_t a,void*v,size_t n){
 uintptr_t b=(uintptr_t)car;
 if(denied || a<b || a+n>b+sizeof(car))return 0;
 memcpy(v,(void*)a,n);
 if(race_change && a==b+0x4440){float x=500;memcpy(car+0x4170,&x,4);}
 return 1;
}
static void reset(void){memcpy(car,baseline,sizeof(car));denied=race_change=0;}
static void i32(int off,int x){memcpy(car+off,&x,4);}
static void f32(int off,float x){memcpy(car+off,&x,4);}
static int run(void){return f1c_native_removal_repair(rd,(uintptr_t)car,GetCurrentThreadId(),1);}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char**argv){
 FILE*f=fopen(argv[1],"rb");CHECK(f && fread(baseline,1,sizeof(baseline),f)==sizeof(baseline));fclose(f);
 reset();CHECK(run());CHECK(*(unsigned*)(car+0x4170)==0x7f7fffff);
 for(int i=0;i<sizeof(car);i++)if(i<0x4170||i>=0x4174)CHECK(car[i]==baseline[i]);
 CHECK(!run()); /* already expired: idempotent */
 reset();CHECK(!f1c_native_removal_repair(rd,(uintptr_t)car,GetCurrentThreadId(),0));
 CHECK(!f1c_native_removal_repair(rd,(uintptr_t)car,GetCurrentThreadId()+1,1));
 CHECK(!f1c_native_removal_repair(rd,0,GetCurrentThreadId(),1));
 int offs[]={0x32c8,0x4064,0x4410,0x45f4,0x3d6c,0x4068,0x3450,0x4040,0x48bc};
 int vals[]={0,2,1,1,1,0,2,1,0};
 for(int j=0;j<9;j++){reset();i32(offs[j],vals[j]);CHECK(!run());}
 float speeds[]={0.5f,10.0f,-1.0f,NAN,INFINITY};
 for(int j=0;j<5;j++){reset();f32(0x4440,speeds[j]);CHECK(!run());}
 float triggers[]={111.83143f,200.0f,-1.0f,NAN,INFINITY};
 for(int j=0;j<5;j++){reset();f32(0x4170,triggers[j]);CHECK(!run());}
 reset();f32(0x4480,NAN);CHECK(!run());
 reset();denied=1;CHECK(!run());
 reset();race_change=1;CHECK(!run());CHECK(*(float*)(car+0x4170)==500.0f);
 puts("PASS real Trulli bytes: only expired trigger changes; idempotent, healthy/pit/player/retired/moving/future/invalid/thread/read/CAS negatives");return 0;
}
'''
(out/'test.c').write_text(code)
subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-I'+str(root/'f1c_ida_decompiled/tools/sdk'),str(out/'test.c'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe'),str(out/'trulli.bin')],check=True)
