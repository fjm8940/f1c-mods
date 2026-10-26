from pathlib import Path
import subprocess,time,json,pefile
root=Path(__file__).resolve().parents[4]; p=root/'development/mods/full_safety_car/trials/v0.10.60_inverted_engine'
o=p/('engine-tests-'+str(time.time_ns()));o.mkdir()
expected=bytes.fromhex('8b81bc32000085c0750ab928747c00e99c77f5ffc7819439000000000000c3')
pe=pefile.PE(str(Path.home()/'Documents/F1 Challenge V10/F1 Challenge Online.exe'));assert pe.get_data(0xefac0,31)==expected
capture=json.loads((p.parent/'v0.10.58_stint_control/malcharek-inverted-2.json').read_text(encoding='utf-8-sig'))
row=next(r for r in capture['position_inventory']['rows'] if r['slot']==21)
(o/'malcharek.bin').write_bytes(bytes.fromhex(row['vehicle_bytes_hex']))
code=r"""
#include <stdio.h>
#include "f1c_ai_engine_stop.h"
#include "sc_engine_stop_watch.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line%d\n",__LINE__);return 1;}}while(0)
static unsigned char car[0x48c0],before[0x48c0];
static int rd(uintptr_t a,void *out,size_t n){if(a<(uintptr_t)car||a+n>(uintptr_t)car+sizeof(car))return 0;memcpy(out,(void*)a,n);return 1;}
int main(int argc,char**argv){
 FILE*f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(car,1,sizeof(car),f)==sizeof(car));fclose(f);memcpy(before,car,sizeof(car));
 unsigned char bytes[]={0x8b,0x81,0xbc,0x32,0,0,0x85,0xc0,0x75,0x0a,0xb9,0x28,0x74,0x7c,0,0xe9,0x9c,0x77,0xf5,0xff,0xc7,0x81,0x94,0x39,0,0,0,0,0,0,0xc3};
 void*site=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(site);memcpy(site,bytes,sizeof(bytes));
 CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),0,(F1CAIEngineStopFn)site));CHECK(!memcmp(car,before,sizeof(car)));
 CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId()+1,1,(F1CAIEngineStopFn)site));
 CHECK(f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));CHECK(*(float*)(car+0x39a0)==0);
 for(int i=0;i<sizeof(car);i++)if(i<0x39a0||i>=0x39a4)CHECK(car[i]==before[i]);
 for(int i=0;i<1000;i++){*(float*)(car+0x39a0)=800;CHECK(f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));}
 *(int*)(car+0x32c8)=0;CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));
 memcpy(car,before,sizeof(car));car[0x4410]=1;CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));
 memcpy(car,before,sizeof(car));*(int*)(car+0x4064)=2;CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));
 memcpy(car,before,sizeof(car));*(float*)(car+0x39a0)=NAN;CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));
 memcpy(car,before,sizeof(car));((unsigned char*)site)[0]=0x90;CHECK(!f1c_ai_engine_stop_at(rd,(uintptr_t)car,GetCurrentThreadId(),1,(F1CAIEngineStopFn)site));
 SCEngineStopWatch s={0};CHECK(!sc_engine_stop_confirm(&s,1,2,0,1,3));
 for(int i=1;i<30;i++)CHECK(!sc_engine_stop_confirm(&s,1,2,i*.1,1,3));
 CHECK(sc_engine_stop_confirm(&s,1,2,3,1,3));CHECK(!sc_engine_stop_confirm(&s,1,2,3,1,3));
 CHECK(!sc_engine_stop_confirm(&s,1,2,3.1,0,3));CHECK(!s.valid);
 CHECK(!sc_engine_stop_confirm(&s,1,2,4,1,3));CHECK(!sc_engine_stop_confirm(&s,1,3,5,1,3));CHECK(s.elapsed==0);
 CHECK(!sc_engine_stop_confirm(&s,2,3,6,1,3));CHECK(s.elapsed==0);
 CHECK(!sc_engine_stop_confirm(&s,2,3,10,1,3));CHECK(s.elapsed==0);
 puts("PASS actual native engine-off bytes on captured Malcharek struct: only RPM field changes;1000 ABI calls; player/retired/finished/thread/authority/NaN/signature refusal;3s confirmation and history resets");return 0;}
"""
(o/'test.c').write_text(code)
subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-O2','-std=c11','-I'+str(root/'f1c_ida_decompiled/tools/sdk'),'-I'+str(p),str(o/'test.c'),'-o',str(o/'test.exe')],check=True)
subprocess.run([str(o/'test.exe'),str(o/'malcharek.bin')],check=True)
s=(p/'safety_car_guardrails.c').read_text()
assert 'f1c_inverted_install' not in s and 'f1c_inverted_removal.h' not in s
assert 'i!=g_sc_slot&&slot_live(i)&&!pit_owned(i)&&life.valid' in s
assert 'life.role==1&&life.finish==0&&!life.retired&&orientation.valid' in s
assert s.index('if(!fresh_sim)goto done;')<s.index('sc_inverted_engine_pump(); /*')
h=s[s.index('static void sc_handover_pump'):s.index('#include "sc_progress_watch.h"')]
assert h.index('SCORE_SCAR05')<h.index('g_pit_handover=1')
a=s.index('/* Pit ownership, never damage/retirement, triggers the return. */');r=s[a:s.index('static void engage(void)',a)]
assert r.index('pit_owned(g_sc_slot)')<r.index('SCORE_SCAR04')<r.index('sc_initial_restore()')
print('PASS fresh callback/exclusion integration, timer hook absent and pit audio retained')
