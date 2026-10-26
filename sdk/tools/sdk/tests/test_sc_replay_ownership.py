"""Execute native ProcessDriverEntries bytes with the real ownership gate.
External loader services are bounded stubs; reconciliation branches, name/count
writes and native 104-slot loop are the copied reviewed executable instructions.
No live process access. A dedicated PE section reserves the native addresses;
all test executable code is outside that section.
"""
from pathlib import Path
import subprocess, sys, tempfile, hashlib, os
import pefile
sdk=Path(__file__).resolve().parents[1]
exe=Path(sys.argv[1]) if len(sys.argv)>1 else Path(os.environ.get('F1C_SDK_TEST_EXE', sdk.parents[2]/'F1 Challenge V10/F1 Challenge Online.exe'))
assert hashlib.sha256(exe.read_bytes()).hexdigest().upper()=='1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003'
pe=pefile.PE(str(exe));native=pe.get_data(0x4b8da0-pe.OPTIONAL_HEADER.ImageBase,0x25d)
assert native[-1:]==b'\xc3',native[-16:].hex()
code=r''' 
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "f1c_sc_replay_ownership.h"
static unsigned char __attribute__((section(".game_memory"),used)) game_memory[0x500000]={1};
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)
#define COUNT (*(int*)0x5c329c)
#define MODE (*(int*)0x8ab6e4)
#define CAR(i) ((char*)(0x5ec6d8u+0x48c0u*(i)))
#define ROLE(i) (*(int*)(CAR(i)+13000))
#define ROOT(i) (*(uintptr_t*)(CAR(i)+0x6f4))
#define NAME(i) (CAR(i)+0x4620)
#define PATH(i) (CAR(i)+0x4700)
static unsigned char recorder[4096],records[104][144];
static uintptr_t nodes[104][3];
static int destroys[104],loads[104],renders[104],names[104],skip_enabled=1;
static int slot(void*v){return ((uintptr_t)v-0x5ec6d8u)/0x48c0u;}
static int owned(uintptr_t v){return skip_enabled && v==(uintptr_t)CAR(24) &&
 *(uintptr_t*)(v+0x4034)==v && ROOT(24)==0xdead1800u && !strcmp(NAME(24),"Bernd Maylander") &&
 *(int*)(v+0x4898)==272;}
static const char* __attribute__((thiscall)) path(void*v){return (char*)v+0x4700;}
static void __attribute__((thiscall)) destroy(void*v){int i=slot(v);destroys[i]++;ROOT(i)=0;NAME(i)[0]=0;}
static void __attribute__((thiscall)) state(void*v,int n,int unused){(void)unused;*(int*)((char*)v+13000-12)=n;}
static int rebuild(void){int n=0;for(int i=0;i<104;i++)if(ROLE(i)!=-1)((void**)0x7c54f0)[n++]=CAR(i);return n;}
static void __attribute__((thiscall)) lookup(void*v,const char*p){strcpy((char*)v+0x4700,p);}
static void __attribute__((thiscall)) copystr(void*v,const char*p){(void)v;(void)p;}
static int __attribute__((thiscall)) load(void*v,int n){int i=slot(v);(void)n;loads[i]++;ROOT(i)=0xdead0000u+0x100u*i;return 1;}
static void __attribute__((thiscall)) render(void*v,int n){(void)n;renders[slot(v)]++;}
static void __attribute__((thiscall)) name(void*v,const char*p){int i=slot(v);names[i]++;strcpy(NAME(i),p);}
static void jump(uintptr_t at,void*fn){*(BYTE*)at=0xe9;*(int32_t*)(at+1)=(int32_t)((uintptr_t)fn-at-5);}
static void entry(int i,const char*nm,const char*path,float start){
 memset(records[i],0,144);strcpy((char*)records[i]+1,nm);strcpy((char*)records[i]+65,path);
 *(float*)(records[i]+132)=start;*(float*)(records[i]+136)=-1e32f;
 nodes[i][0]=(uintptr_t)records[i];nodes[i][1]=0;*(uintptr_t*)(recorder+64+16*i)=(uintptr_t)nodes[i];}
static void reset(int deployed){
 memset((void*)0x5ec6d8,0,104*0x48c0);memset(recorder,0,sizeof recorder);
 memset(destroys,0,sizeof destroys);memset(loads,0,sizeof loads);memset(renders,0,sizeof renders);memset(names,0,sizeof names);
 for(int i=0;i<104;i++)ROLE(i)=-1;
 for(int i=0;i<25;i++){
  ROLE(i)=i==24&&!deployed?-1:3;ROOT(i)=0xdead0000u+0x100u*i;
  *(uintptr_t*)(CAR(i)+0x4034)=(uintptr_t)CAR(i);*(int*)(CAR(i)+0x4898)=i==24?272:i;
  sprintf(NAME(i),"Driver%d",i);strcpy(PATH(i),"racer.veh");
  if(i<24)entry(i,NAME(i),PATH(i),-1e32f);
 }
 strcpy(NAME(24),"Bernd Maylander");strcpy(PATH(24),"safety.veh");
 COUNT=deployed?25:24;MODE=2;skip_enabled=1;*(float*)(recorder+3896)=100.0f;
}
static void run(void){((void(__attribute__((thiscall))*)(void*))0x4b8da0)(recorder);}
static DWORD WINAPI other_thread(void*p){(void)p;run();return 0;}
static const unsigned char native_bytes[]={ @NATIVE@ };
static const unsigned char history_bytes[]={ @HISTORY@ };
int main(void){
 DWORD old;CHECK(VirtualProtect(game_memory,sizeof game_memory,PAGE_EXECUTE_READWRITE,&old));
 memcpy((void*)0x4b8da0,native_bytes,sizeof native_bytes);*(float*)0x5acd10=-1e32f;
 memcpy((void*)0x4b50c0,history_bytes,sizeof history_bytes);jump(0x4c69e0,path);jump(0x588410,strcmp);jump(0x4c6e80,destroy);
 jump(0x4ef5a0,state);jump(0x42bc80,rebuild);jump(0x4c7b20,lookup);jump(0x566650,copystr);
 jump(0x4c6c40,load);jump(0x4c6d90,render);jump(0x4c6980,name);
 reset(0);run();printf("old count%d destroy%d root%lx name%s\n",COUNT,destroys[24],(unsigned long)ROOT(24),NAME(24));CHECK(COUNT==23 && destroys[24]==1 && !ROOT(24) && !NAME(24)[0]);
 puts("OLD native caller: parked SC removed, count24->23, name cleared");
 CHECK(!f1c_sc_replay_install(owned,GetCurrentThreadId()+1,1));
 CHECK(!f1c_sc_replay_install(owned,GetCurrentThreadId(),0));
 *(BYTE*)0x4b8fbf ^=1;CHECK(!f1c_sc_replay_install(owned,GetCurrentThreadId(),1));*(BYTE*)0x4b8fbf ^=1;
 CHECK(f1c_sc_replay_install(owned,GetCurrentThreadId(),1));
 for(int deployed=0;deployed<2;deployed++){
  reset(deployed);unsigned char before[0x48c0];memcpy(before,CAR(24),sizeof before);
  for(int k=0;k<20;k++){
   /* history absent, older matching, entry-time future, different historical identity */
   if(k%4==0)*(uintptr_t*)(recorder+64+16*24)=0;
   else entry(24,k%4==3?"Older owner":"Bernd Maylander",k%4==3?"older.veh":"safety.veh",k%4==2?150.0f:0.0f);
   run();CHECK(COUNT==(deployed?25:24));CHECK(!memcmp(before,CAR(24),sizeof before));
   CHECK(!destroys[24]&&!loads[24]&&!renders[24]&&!names[24]);
  }
  /* Native counted restore now visits EVERY original entrant. */
  for(int i=0;i<COUNT;i++)if(ROLE(i)==3)ROLE(i)=1;
  CHECK(ROLE(23)==1);if(deployed)CHECK(ROLE(24)==1);
  /* Later owner deployment/park still uses the same scene instance. */
  ROLE(24)=1;COUNT=rebuild();CHECK(COUNT==25 && ROOT(24)==0xdead1800u);
  ROLE(24)=-1;COUNT=rebuild();CHECK(COUNT==24);
 }
 reset(0);*(uintptr_t*)(recorder+64+16*5)=0;run();CHECK(destroys[5]==1 && COUNT==23 && !NAME(5)[0]);CHECK(ROOT(24));
 reset(0);entry(5,"Other racer","changed.veh",0);run();CHECK(destroys[5]==1 && loads[5]==1 && renders[5]==1 && !strcmp(NAME(5),"Other racer"));CHECK(ROOT(24));
 reset(0);MODE=1;run();CHECK(destroys[24]==1); /* no protection outside native replay */
 reset(0);skip_enabled=0;run();CHECK(destroys[24]==1);
 reset(0);*(uintptr_t*)(CAR(24)+0x4034)=0;run();CHECK(destroys[24]==1);
 reset(0);HANDLE th=CreateThread(NULL,0,other_thread,NULL,0,NULL);CHECK(th);CHECK(WaitForSingleObject(th,3000)==WAIT_OBJECT_0);CloseHandle(th);CHECK(destroys[24]==1);
 CHECK(!f1c_sc_replay_remove(GetCurrentThreadId()+1));
 BYTE original=*(BYTE*)F1C_SC_REPLAY_SITE;*(BYTE*)F1C_SC_REPLAY_SITE=0xcc;CHECK(!f1c_sc_replay_remove(GetCurrentThreadId()));CHECK(*(BYTE*)F1C_SC_REPLAY_SITE==0xcc);*(BYTE*)F1C_SC_REPLAY_SITE=original;
 CHECK(f1c_sc_replay_remove(GetCurrentThreadId()));CHECK(!memcmp((void*)F1C_SC_REPLAY_SITE,f1c_sc_replay_expected,sizeof f1c_sc_replay_expected));
 reset(0);run();CHECK(destroys[24]==1 && COUNT==23);
 puts("PASS actual native reconcile bytes: owned SC entirely unchanged across absent/older/future/replaced history, parked/deployed and repeated seeks; racers native; count/name/scene intact; later deployment; fingerprint/thread/ownership refusal; exact uninstall");
 return 0;
}
'''
if len(sys.argv)>3:
    source=Path(sys.argv[3]).read_text(encoding='utf-8')
    start=source.index('static int __cdecl sc_replay_instance_owned(')
    end=source.index('/* ---- 5-byte detour install/remove',start)
    callback=source[start:end].replace('sc_replay_instance_owned','owned')
    begin=code.index('static int owned(');end=code.index('static const char*',begin)
    stubs="""static int g_sc_slot=24,g_hooks_ready=1,g_stop_requested=0;
#define VEH_SLOTS 104
#define VEH(i) CAR(i)
static int on_game_thread(void){return GetCurrentThreadId()==f1c_sc_replay_thread;}
static int sc_offline_authorized(void){return 1;}
static int sc_slot_owned(void){return skip_enabled && ROOT(24)==0xdead1800u && !strcmp(NAME(24),\"Bernd Maylander\") && *(int*)(CAR(24)+0x4898)==272;}
"""
    code=code[:begin]+stubs+callback+code[end:]
code=code.replace('@HISTORY@',','.join('0x%02x'%x for x in pe.get_data(0x4b50c0-pe.OPTIONAL_HEADER.ImageBase,0x60)))
code=code.replace('@NATIVE@',','.join('0x%02x'%x for x in native))
out=Path(sys.argv[2]) if len(sys.argv)>2 else Path(tempfile.mkdtemp(prefix='sc-replay-ownership-'))
out.mkdir(parents=True,exist_ok=True);(out/'native-test.c').write_text(code)
for cmd in [[r'C:/msys64/mingw32/bin/gcc.exe','-O2','-std=c11','-Wall','-Wl,--image-base,0x400000,--disable-dynamicbase','-I'+str(sdk),str(out/'native-test.c'),'-o',str(out/'native-test.exe')],[str(out/'native-test.exe')]]:
 p=subprocess.run(cmd,capture_output=True,text=True);print(p.stdout+p.stderr)
 with (out/'results.txt').open('a') as f:f.write(p.stdout+p.stderr)
 p.check_returncode()
