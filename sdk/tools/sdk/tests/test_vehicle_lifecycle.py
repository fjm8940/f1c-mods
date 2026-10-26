from pathlib import Path
import datetime,subprocess,sys,struct
r=Path(__file__).resolve().parents[3];sys.path.insert(0,str(r/'tools'))
from vehicle_lifecycle_read import observe
o=r/'tools/sdk/tests/evidence'/('lifecycle_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
code=r'''
#include "f1c_vehicle_lifecycle.h"
#include <assert.h>
#include <stdio.h>
static unsigned char car[0x48c0],scene[0x168],table[28*156];
static int readable=1;
static int rd(uintptr_t a,void*out,size_t n){
 if(!readable)return 0;
 if(a>=0x100000 && a+n<=0x100000+sizeof(car)){memcpy(out,car+a-0x100000,n);return 1;}
 if(a>=0x5ea2c0 && a+n<=0x5ea2c0+sizeof(scene)){memcpy(out,scene+a-0x5ea2c0,n);return 1;}
 if(a>=0x200000 && a+n<=0x200000+sizeof(table)){memcpy(out,table+a-0x200000,n);return 1;}
 return 0;
}
static void put(unsigned char*p,int off,int x){memcpy(p+off,&x,4);}
int main(void){
 float body[3]={19.05737f,-20.03468f,-16.65424f},garage[3]={18.966f,-20.273f,-14.554f};
 put(car,0x32c8,1);put(car,0x4064,2);car[0x4410]=1;put(car,0x48a4,2);put(car,0x48a8,0);
 memcpy(car+0x34e0,body,12);put(scene,0x158,0x200000);put(scene,0x164,52);memcpy(table+28*2,garage,12);
 F1CVehicleLifecycle v=f1c_vehicle_lifecycle_read(rd,0x100000);assert(v.valid&&v.garage_clear&&!v.participant&&v.garage_distance>2&&v.garage_distance<2.2);
 body[0]+=100;memcpy(car+0x34e0,body,12);v=f1c_vehicle_lifecycle_read(rd,0x100000);assert(v.valid&&v.garage_valid&&!v.garage_clear);
 body[0]-=100;memcpy(car+0x34e0,body,12);
 car[0x45f4]=1;assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_clear);car[0x45f4]=0;
 put(car,0x48a4,52);assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_valid);put(car,0x48a4,2);
 put(car,0x48a8,3);assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_valid);put(car,0x48a8,0);
 put(scene,0x158,-8);assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_valid);put(scene,0x158,0x200000);

 put(car,0x32c8,-1);put(car,0x4064,0);car[0x4410]=0;
 assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_valid);
 assert(f1c_vehicle_dormant_garage_read(rd,0x100000).garage_clear);
 body[0]+=100;memcpy(car+0x34e0,body,12);
 assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).garage_clear);
 body[0]-=100;memcpy(car+0x34e0,body,12);
 car[0x45f4]=1;assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).garage_valid);car[0x45f4]=0;
 for(int role=0;role<=2;role++){put(car,0x32c8,role);assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).garage_valid);}
 put(car,0x32c8,-1);put(car,0x4064,1);assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).garage_valid);
 put(car,0x4064,0);readable=0;assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).valid);readable=1;
 body[0]=NAN;memcpy(car+0x34e0,body,12);assert(!f1c_vehicle_dormant_garage_read(rd,0x100000).garage_valid);
 put(car,0x32c8,1);put(car,0x4064,2);car[0x4410]=1;
 body[0]=NAN;memcpy(car+0x34e0,body,12);assert(!f1c_vehicle_lifecycle_read(rd,0x100000).garage_valid);
 assert(!f1c_vehicle_lifecycle_read(rd,0xfffffff0u).valid);readable=0;assert(!f1c_vehicle_lifecycle_read(rd,0x100000).valid);
 assert(f1c_lifecycle_participant(0,0,0)&&f1c_lifecycle_participant(1,0,0));
 assert(!f1c_lifecycle_participant(1,1,2)&&!f1c_lifecycle_participant(1,0,1)&&!f1c_lifecycle_participant(2,0,0));
 puts("PASS lifecycle: measured Gene garage geometry, road-wreck control, AI state, bounds/overflow/NaN/unreadable and participants");return 0;
}
'''
(o/'test.c').write_text(code,encoding='utf-8')
run=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-msse2','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=run.stdout+run.stderr
if not run.returncode:
 run=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=run.stdout+run.stderr
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);assert run.returncode==0
mem={}
def put(a,fmt,*vals):mem[a]=struct.pack(fmt,*vals)
veh=0x100000
for off,fmt,val in [(0x32c8,'<i',1),(0x4064,'<i',2),(0x4410,'<B',1),(0x45f4,'<B',0),(0x48a4,'<i',2),(0x48a8,'<i',0)]:put(veh+off,fmt,val)
put(veh+0x34e0,'<3f',19.05737,-20.03468,-16.65424);put(0x5ea2c0+0x158,'<I',0x200000);put(0x5ea2c0+0x164,'<I',52);put(0x200000+56,'<3f',18.966,-20.273,-14.554)
read=lambda a,n:mem.get(a)
assert observe(read,veh)['garage_clear']
put(veh+0x34e0,'<3f',119,-20,-16);assert not observe(read,veh)['garage_clear']
put(0x5ea2c0+0x158,'<I',0xfffffff8);assert not observe(read,veh)['garage_clear']
assert not observe(lambda a,n:None,veh)['valid']
print('PASS canonical lifecycle diagnostic parity: garage, road wreck, pointer overflow and unreadable')
