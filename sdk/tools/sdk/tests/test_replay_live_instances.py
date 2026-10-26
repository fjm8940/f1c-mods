"""Native dispatch and reconciliation regression; external scene services stubbed.
Does not simulate or claim native vehicle-physics acceptance.
"""
from pathlib import Path
import ast,sys,subprocess,hashlib,pefile,os,tempfile
sdk=Path(__file__).resolve().parents[1]
exe=Path(sys.argv[1]) if len(sys.argv)>1 else Path(os.environ.get('F1C_SDK_TEST_EXE',sdk.parents[2]/'F1 Challenge V10/F1 Challenge Online.exe'))
out=Path(sys.argv[2]) if len(sys.argv)>2 else Path(tempfile.mkdtemp(prefix='f1c-live-replay-'))
out.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256(exe.read_bytes()).hexdigest().upper()=='1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003'
pe=pefile.PE(str(exe))
tree=ast.parse((sdk/'tests/test_sc_replay_ownership.py').read_text())
code=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='code' for t in n.targets))
code=code[:code.index('int main(void)')]
code=code.replace('#include "f1c_sc_replay_ownership.h"','#include "f1c_sc_replay_ownership.h"\n#include "f1c_replay_live_instances.h"')
code=code.replace('static int owned(uintptr_t v){return skip_enabled','static int owned(uintptr_t v){return f1c_live_instance_preserved(v) || (skip_enabled').replace('*(int*)(v+0x4898)==272;}','*(int*)(v+0x4898)==272);}')
code+=r'''
static int allow=1,builders=0,plays=0,nested=0,bad=0;
static int authorized(void){return allow;}
/* Representative constructor-cleared race state. No claim to emulate physics. */
#define DNF(i) (*(int*)(CAR(i)+0x2000))
static void __attribute__((thiscall)) constructor(void*v,int n){render(v,n);DNF(slot(v))=0;}
static int __attribute__((thiscall)) offline(void*v){(void)v;return 0;}
static void __attribute__((thiscall)) builder(void*v,void*a,void*b,float t){(void)v;(void)a;(void)b;(void)t;builders++;}
static char __attribute__((thiscall)) playback_poll(void*v){(void)v;nested++;return 1;}
static char __attribute__((thiscall)) playback(void*v,float seconds,void*mode){
 (void)v;(void)seconds;plays++;MODE=2;
 char result=((F1CLiveDispatch)F1C_LIVE_DISPATCH)(mode);(void)result;
 if(!f1c_live_active)bad++;
 for(int k=0;k<12;k++){
  for(int i=0;i<24;i++){
   if(i==5||i==11||i==13||i==15||i==20){
    if(k%3==0)*(uintptr_t*)(recorder+64+16*i)=0;
    else entry(i,NAME(i),PATH(i),k%3==1?150.f:0.f);
   }
  }
  /* An old record for an empty slot must not construct another entrant. */
  entry(30,"Historical extra","old.veh",0);
  run();
 }
 MODE=1;return 73;
}
static void setup(void){
 reset(0);MODE=1;*(int*)F1C_LIVE_REQUEST=1;
 for(int i=0;i<25;i++){ROLE(i)=i==24?-1:i==0?0:1;*(int*)(CAR(i)+0x4894)=i;}
 COUNT=rebuild();DNF(5)=DNF(11)=DNF(13)=DNF(15)=DNF(20)=1;
}
static const unsigned char dispatch_bytes[]={ @DISPATCH@ };
int main(void){
 DWORD old;CHECK(VirtualProtect(game_memory,sizeof game_memory,PAGE_EXECUTE_READWRITE,&old));
 memcpy((void*)0x4b8da0,native_bytes,sizeof native_bytes);*(float*)0x5acd10=-1e32f;
 memcpy((void*)0x4b50c0,history_bytes,sizeof history_bytes);
 memcpy((void*)F1C_LIVE_DISPATCH,dispatch_bytes,sizeof dispatch_bytes);
 jump(0x4c69e0,path);jump(0x588410,strcmp);jump(0x4c6e80,destroy);
 jump(0x4ef5a0,state);jump(0x42bc80,rebuild);jump(0x4c7b20,lookup);jump(0x566650,copystr);
 jump(0x4c6c40,load);jump(0x4c6d90,constructor);jump(0x4c6980,name);
 jump(0x461db0,offline);jump(0x4b9870,builder);jump(0x4b9890,playback);jump(0x4b9a00,playback_poll);
 CHECK(f1c_sc_replay_install(owned,GetCurrentThreadId(),1));
 setup();MODE=2;*(uintptr_t*)(recorder+64+16*20)=0;run();
 entry(20,"Panis","racer.veh",0);run();CHECK(destroys[20] && loads[20] && DNF(20)==0);
 puts("OLD: actual absent/present native branches reconstruct Panis and invoke race-state reset");
 CHECK(!f1c_live_install(authorized,GetCurrentThreadId()+1));
 *(BYTE*)F1C_LIVE_DISPATCH=0xcc;CHECK(!f1c_live_install(authorized,GetCurrentThreadId()));
 memcpy((void*)F1C_LIVE_DISPATCH,dispatch_bytes,5);
 CHECK(f1c_live_install(authorized,GetCurrentThreadId()));
 for(int pause=0;pause<2;pause++)for(int cycle=0;cycle<3;cycle++){
  setup();*(BYTE*)0x7c56ac=pause;builders=plays=nested=bad=0;
  unsigned char before[25*0x48c0];memcpy(before,CAR(0),sizeof before);
  CHECK(((F1CLiveDispatch)F1C_LIVE_DISPATCH)((void*)F1C_LIVE_MODE)==73);
  CHECK(plays==1 && builders==!pause && nested==1 && !bad && !f1c_live_active);
  CHECK(COUNT==24 && !memcmp(before,CAR(0),sizeof before) && !ROOT(30));
  for(int i=0;i<104;i++)CHECK(!destroys[i]&&!loads[i]&&!renders[i]&&!names[i]);
 }
 setup();allow=0;CHECK(!f1c_live_begin((void*)F1C_LIVE_MODE));allow=1;
 *(int*)F1C_LIVE_REQUEST=0;CHECK(!f1c_live_begin((void*)F1C_LIVE_MODE));*(int*)F1C_LIVE_REQUEST=1;
 MODE=2;CHECK(!f1c_live_begin((void*)F1C_LIVE_MODE));MODE=1;
 ((uintptr_t*)F1C_LIVE_ARRAY)[23]=(uintptr_t)CAR(0);CHECK(!f1c_live_begin((void*)F1C_LIVE_MODE));
 setup();CHECK(f1c_live_begin((void*)F1C_LIVE_MODE));CHECK(!f1c_live_remove(GetCurrentThreadId()));
 MODE=2;CHECK(f1c_live_instance_preserved((uintptr_t)CAR(20)));
 ROOT(20)++;CHECK(!f1c_live_instance_preserved((uintptr_t)CAR(20)));ROOT(20)--;
 NAME(20)[0]^=1;CHECK(!f1c_live_instance_preserved((uintptr_t)CAR(20)));NAME(20)[0]^=1;
 f1c_live_active=0;CHECK(!f1c_live_instance_preserved((uintptr_t)CAR(20)));
 BYTE patch=*(BYTE*)F1C_LIVE_DISPATCH;*(BYTE*)F1C_LIVE_DISPATCH=0xcc;
 CHECK(!f1c_live_remove(GetCurrentThreadId()) && *(BYTE*)F1C_LIVE_DISPATCH==0xcc);
 *(BYTE*)F1C_LIVE_DISPATCH=patch;CHECK(f1c_live_remove(GetCurrentThreadId()));
 CHECK(!memcmp((void*)F1C_LIVE_DISPATCH,dispatch_bytes,sizeof dispatch_bytes));
 CHECK(f1c_sc_replay_remove(GetCurrentThreadId()));
 puts("PASS native dispatch trampoline/return, paused and unpaused entry, nested playback, repeated absent/older/future seeks, 5 DNFs, all instance bytes unchanged, empty historical slot, identity refusal, exact unhook; physics acceptance remains native");return 0;
}
'''
for name,addr,size in [('NATIVE',0x4b8da0,0x25d),('HISTORY',0x4b50c0,0x60),('DISPATCH',0x4ba6c0,0x77)]:
 code=code.replace('@'+name+'@',','.join('0x%02x'%x for x in pe.get_data(addr-pe.OPTIONAL_HEADER.ImageBase,size)))
(out/'native-test.c').write_text(code)
for cmd in [[r'C:/msys64/mingw32/bin/gcc.exe','-O2','-std=c11','-Wall','-Wl,--image-base,0x400000,--disable-dynamicbase','-I'+str(sdk),str(out/'native-test.c'),'-o',str(out/'native-test.exe')],[str(out/'native-test.exe')]]:
 p=subprocess.run(cmd,capture_output=True,text=True);print(p.stdout+p.stderr)
 with (out/'results.txt').open('a') as f:f.write(p.stdout+p.stderr)
 p.check_returncode()
