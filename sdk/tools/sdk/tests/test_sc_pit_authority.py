from pathlib import Path
import subprocess,time,pefile
root=Path(__file__).resolve().parents[4]
p=root/'development/mods/full_safety_car/trials/v0.10.58_stint_control'
o=p/('pit-authority-tests-'+str(time.time_ns()));o.mkdir()
expected=bytes.fromhex('8b44240483f8027d0a85c07c0689814c450000c20400')
pe=pefile.PE(str(Path.home()/'Documents/F1 Challenge V10/F1 Challenge Online.exe'))
assert pe.get_data(0x2ac70,len(expected))==expected
code=r'''
#include <stdio.h>
#include "f1c_sc_pit_authority.h"
static unsigned char car[0x48c0],other[0x48c0];static int block_on=1;
static int filter(uintptr_t v,unsigned cmd){return block_on && v==(uintptr_t)car && cmd==1;}
#define CHECK(x) do{if(!(x)){printf("FAIL line%d\n",__LINE__);return 1;}}while(0)
int main(void){
 unsigned char bytes[]={0x8b,0x44,0x24,0x04,0x83,0xf8,0x02,0x7d,0x0a,0x85,0xc0,0x7c,0x06,0x89,0x81,0x4c,0x45,0,0,0xc2,4,0};
 void*site=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(site);memcpy(site,bytes,sizeof(bytes));
 F1CNativePitSetter call=(F1CNativePitSetter)site;
 CHECK(call(car,1)==1 && *(int*)(car+0x454c)==1); /* baseline native request */
 f1c_sc_pit_block=filter;
 CHECK(f1c_detour_install_prevalidated_layout(&f1c_sc_pit_hook,site,(void*)f1c_sc_pit_detour,bytes,7)==F1C_DETOUR_OK);
 *(int*)(car+0x454c)=0;
 for(int i=0;i<1000;i++)CHECK(call(car,1)==1 && *(int*)(car+0x454c)==0);
 CHECK(call(other,1)==1 && *(int*)(other+0x454c)==1);
 block_on=0;CHECK(call(car,1)==1 && *(int*)(car+0x454c)==1);
 block_on=1;CHECK(call(car,0)==0 && *(int*)(car+0x454c)==0);
 CHECK(call(car,2)==2 && *(int*)(car+0x454c)==0);
 CHECK(call(car,0xffffffffu)==0xffffffffu && *(int*)(car+0x454c)==0);
 CHECK(f1c_sc_pit_remove(GetCurrentThreadId()));CHECK(!memcmp(site,bytes,sizeof(bytes)));
 CHECK(call(car,1)==1 && *(int*)(car+0x454c)==1);
 memset(car,0,sizeof(car));*(float*)(car+0x3984)=44.977337f;
 CHECK(!f1c_sc_fuel_fill((uintptr_t)car,GetCurrentThreadId(),0));
 CHECK(!f1c_sc_fuel_fill((uintptr_t)car,GetCurrentThreadId()+1,1));
 CHECK(f1c_sc_fuel_fill((uintptr_t)car,GetCurrentThreadId(),1));
 CHECK(*(float*)(car+0x3984)==135.0f);
 for(int i=0;i<sizeof(car);i++)if(i<0x3984||i>=0x3988)CHECK(!car[i]);
 *(float*)(car+0x3984)=NAN;CHECK(!f1c_sc_fuel_fill((uintptr_t)car,GetCurrentThreadId(),1));
 puts("PASS actual native setter bytes: SC block, racer unchanged, authorized return, zero/invalid args, thiscall stack, restore;135 fuel only authorized amount bytes");return 0;
}
'''
(o/'test.c').write_text(code)
subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-O2','-std=c11','-I'+str(root/'f1c_ida_decompiled/tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],check=True)
subprocess.run([str(o/'test.exe')],check=True)
