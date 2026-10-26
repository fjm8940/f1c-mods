"""Internal gate: real native grid countdown + broker GUI click policy and ABI."""
from pathlib import Path
import contextlib, hashlib, io, os, re, subprocess, sys, time, types

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
import f1c_mod as cli
out = root / 'tools/sdk/tests/evidence' / ('garage_results_' + str(time.time_ns()))
out.mkdir(parents=True)
exe = Path(os.environ.get('F1C_SDK_TEST_EXE', cli.DEFAULT_EXE))
assert hashlib.sha256(exe.read_bytes()).hexdigest().upper() == cli.TARGET_SHA256
pe = cli.VariantPEImage(exe)
compatible_exe = os.environ.get('F1C_SDK_COMPAT_TEST_EXE')
compatible_pe = cli.VariantPEImage(Path(compatible_exe)) if compatible_exe else None
layout = (root / 'mods/sdk_sandbox/game_update_broker/broker_garage_results_layout.h').read_text()
for name, address, size in [('release',0x475050,58),('arm_grid',0x46c030,11),
        ('grid_tick',0x46c040,72),('page_name',0x497980,35),('show_page',0x497940,49),
        ('find_page',0x4976c0,15),('build_grid',0x46ba20,14),('native_schedule',0x4a02c2,53)]:
    body = re.search(r'garage_' + name + r'_bytes\[\]=\{([^}]+)\}', layout)[1]
    expected = bytes(int(v,16) for v in body.split(','))
    assert expected == pe.read_va(address,size), name
    if compatible_pe:
        assert expected == compatible_pe.read_va(address,size), ('compatible',name)

# New front-door modes must reject the wrong binary and invalid address/size.
wrong = out / 'unsupported.bin'; wrong.write_bytes(b'unsupported')
for fmt in ('u32', 'pointer-matches'):
    for path, address, size in [(wrong,0x8d81c0,4),(exe,0xFFFFFFFF,4),(exe,0x8d81c0,4097)]:
        try:
            cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(path),address=address,size=size,format=fmt))
            raise AssertionError('invalid review accepted')
        except cli.ModError:
            pass
    result = io.StringIO()
    with contextlib.redirect_stdout(result):
        cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(exe),address=0x8d80e0 if fmt=='pointer-matches' else 0x8d81c0,size=4,format=fmt))
    assert ('0047FE63' if fmt=='pointer-matches' else '005CB9CC') in result.getvalue()
try:
    cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(exe),address=0x8d81c0,size=3,format='u32'))
    raise AssertionError('misaligned size accepted')
except cli.ModError:
    pass

code = r'''
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "broker_garage_results.h"
'''
# Include captured machine code, not a reimplementation of the native timer.
for name, address, size in [('tick',0x46c040,72),('arm',0x46c030,11),('release',0x475050,58)]:
    code += 'static const unsigned char '+name+'_code[]={'+','.join(str(v) for v in pe.read_va(address,size))+'};\n'
code += r'''
static unsigned char manager[128], grid[512], button[256];
static int pending, finds, grid_gets, builds, shown, finalized, resets, updates;
static int page_exists=1, grid_exists=1, valid_grid=1, show_ok=1, ui_state=0;
static int step;
static int (__attribute__((thiscall)) *native_tick)(void*);
static void (__attribute__((thiscall)) *native_arm)(void*);
static int (__attribute__((thiscall)) *native_release)(void*);
static void * __attribute__((thiscall)) find_page(void *m,const char *s){assert(m==manager&&!strcmp(s,"POST RACE MONITOR"));finds++;return page_exists?grid:0;}
static void * __attribute__((thiscall)) get_grid(void *m,int id){assert(m==manager&&id==33);grid_gets++;return grid_exists?grid:0;}
static int grid_valid(void*g,void*m){assert(g==grid&&m==manager);return valid_grid;}
static int __attribute__((thiscall)) build(void*g,int mode){assert(g==grid&&mode==0);builds++;if(step==0)step=1;return 0;}
static void __attribute__((thiscall)) arm(void*g){assert(g==grid&&step==1);native_arm(g);assert(*(int*)(grid+0x174)==2);step=2;}
static int __attribute__((thiscall)) show(void*m,const char*s,int n){assert(m==manager&&!strcmp(s,"POST RACE MONITOR")&&n==-1&&step==2);shown++;step=3;return show_ok;}
static int __attribute__((thiscall)) finish(void*g,char draw){assert(g==grid&&draw==1&&*(int*)(grid+0x174)==0);finalized++;return 0;}
static int __attribute__((thiscall)) update(void*g){assert(g==grid);updates++;return 0;}
static int __attribute__((thiscall)) destructive_main(void*g){assert(g==(void*)0x7ce24c);resets++;return 0;}
static void rel(unsigned char*p,int off,void*fn){int32_t delta=(int32_t)((uintptr_t)fn-(uintptr_t)(p+off+5));assert(p[off]==0xe8);memcpy(p+off+1,&delta,4);}
static void address(unsigned char*p,int off,void*value){uintptr_t v=(uintptr_t)value;memcpy(p+off,&v,4);}
static void *lift(const unsigned char*src,size_t n){void*p=VirtualAlloc(0,n,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(p);memcpy(p,src,n);return p;}
static BrokerGarageResultsOps ops={find_page,get_grid,grid_valid,build,arm,show};
static BrokerGarageResultsContext c={56,7,3,5,2,"MONITORPAGE",manager};
static int click(BrokerGarageResultsContext*ctx){int r=broker_garage_results_click(ctx,&ops,&pending);return r==BROKER_GARAGE_PASS?native_release(button):-1;}
int main(void){
 unsigned char *t=lift(tick_code,sizeof tick_code),*r=lift(release_code,sizeof release_code);
 native_arm=lift(arm_code,sizeof arm_code);
 rel(t,0x1a,finish);rel(t,0x36,build);rel(t,0x3f,update);address(t,0x21,&ui_state);
 native_tick=(void*)t;rel(r,0x18,destructive_main);address(r,0x23,&ui_state);address(r,0x2e,&ui_state);native_release=(void*)r;
 *(int*)(button+0xe4)=56;*(int*)(grid+0x178)=-1;
 /* Reproduce the original icon's destructive outcome; it never finalizes. */
 assert(native_release(button)==-1&&resets==1&&finalized==0);resets=0;
 assert(click(&c)==-1&&resets==0&&shown==1&&finalized==0&&pending==1);
 assert(click(&c)==-1&&shown==1&&finalized==0); /* duplicate cannot rearm */
 native_tick(grid);assert(*(int*)(grid+0x174)==1&&finalized==0);
 native_tick(grid);assert(*(int*)(grid+0x174)==0&&finalized==1);
 native_tick(grid);assert(finalized==1); /* no repeated finalization */
 /* Native ACCEPT uses another vtable/handler and never enters kind-56 hook. */
 BrokerGarageResultsContext x=c;x.page="POST RACE MONITOR";
 assert(!broker_garage_results_eligible(&x));
 for(int kind=54;kind<=57;kind++){x=c;x.button_kind=kind;assert(broker_garage_results_eligible(&x)==(kind==56));}
 for(int session=0;session<=8;session++){x=c;x.session_type=session;assert(broker_garage_results_eligible(&x)==(session==7));}
 for(int mode=0;mode<=6;mode++){x=c;x.race_mode=mode;assert(broker_garage_results_eligible(&x)==(mode>=1&&mode<=4));}
 for(int phase=0;phase<=8;phase++){x=c;x.race_phase=phase;assert(broker_garage_results_eligible(&x)==(phase>4));}
 for(int finish_state=-1;finish_state<=4;finish_state++){x=c;x.finish_state=finish_state;assert(broker_garage_results_eligible(&x)==(finish_state==2||finish_state==3));}
 x=c;x.page="MAIN MENU";assert(!broker_garage_results_eligible(&x));
 x=c;x.manager=0;assert(!broker_garage_results_eligible(&x));
 x=c;x.page=0;assert(!broker_garage_results_eligible(&x));
 pending=0;page_exists=0;assert(broker_garage_results_click(&c,&ops,&pending)==BROKER_GARAGE_REFUSED&&pending==0&&shown==1);
 page_exists=1;grid_exists=0;assert(broker_garage_results_click(&c,&ops,&pending)==BROKER_GARAGE_REFUSED&&pending==0&&shown==1);
 grid_exists=1;valid_grid=0;assert(broker_garage_results_click(&c,&ops,&pending)==BROKER_GARAGE_REFUSED&&pending==0&&shown==1);
 valid_grid=1;show_ok=0;step=0;assert(broker_garage_results_click(&c,&ops,&pending)==BROKER_GARAGE_REFUSED&&pending==1);
 assert(broker_garage_results_click(&c,&ops,&pending)==BROKER_GARAGE_DUPLICATE&&shown==2&&resets==0);
 puts("PASS GUI release: native old handler reproduces reset; new route arms real native grid; second tick finalizes once; phase/session/button/player guards; missing resources preserve race; x86 thiscall ABI.");
 return 0;
}
'''
source = out/'fixture.c'; source.write_text(code)
result = subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror',
    '-I'+str(root/'mods/sdk_sandbox/game_update_broker'),str(source),'-o',str(out/'fixture.exe')],capture_output=True,text=True)
log = result.stdout+result.stderr
if result.returncode == 0:
    result = subprocess.run([str(out/'fixture.exe')],capture_output=True,text=True)
    log += result.stdout+result.stderr
(out/'RESULTS.txt').write_text(log)
print(log)
assert result.returncode == 0, result.returncode
print('PASS exact code layouts and bounded canonical pointer review. Evidence:',out)

# Compile the actual installed GUI hook and deferred dispatcher, not just policy.
# Map a small fake game image and execute the exact native ESC transition bytes.
native = root/'mods/sdk_sandbox/game_update_broker/broker_garage_results_native.h'
text = native.read_text()
for name, addr in [('leaf',0x4a115c),('transition',0x404f30)]:
    body = re.search(r'garage_esc_'+name+r'_bytes\[\] = \{([^}]+)\}',text)[1]
    values=bytes(int(v.strip(),16) for v in body.split(',') if v.strip())
    assert values == pe.read_va(addr,len(values))
    if compatible_pe:
        assert values == compatible_pe.read_va(addr,len(values)), ('compatible',name)
if compatible_pe:
    for addr, size in [(0x404f30,60),(0x4a0d30,30)]:
        assert pe.read_va(addr,size) == compatible_pe.read_va(addr,size), ('compatible',hex(addr))
    for addr, size in [(0x42bc80,8),(0x4a0fd0,8),(0x42c690,6),(0x46bf10,6)]:
        assert pe.read_va(addr,size) == compatible_pe.read_va(addr,size), ('compatible broker entry',hex(addr))
    print('PASS compatible variant: broker entry hooks, garage code spans, and native transition/setter match reviewed bytes.')
broker=(root/'mods/sdk_sandbox/game_update_broker/game_update_broker.c').read_text()
assert broker.index('result = g_restart_original(receiver);') < broker.index('result = broker_garage_escape_after_restart(result);') < broker.index('if(result == 6 && g_session_active)')
code=r"""
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#define SESSION_TYPE_ADDRESS 0x7c56a4u
static LONG g_active_garage_release_calls;
static int g_ready=1,g_stopping,g_garage_results_pending,g_results_supported,g_garage_release_supported;
typedef struct {void *trampoline;} F1CDetour;
typedef int F1CDetourResult;
#define F1C_DETOUR_OK 0
static int f1c_detour_install_prevalidated_layout(F1CDetour*d,void*a,void*b,const void*c,size_t n){(void)d;(void)a;(void)b;(void)c;(void)n;assert(0);return 1;}
static void log_line(const char *f,...){(void)f;}
#include "broker_garage_results_native.h"
static unsigned char manager[128], player[0x5000],button[256],session[128];
static const char *page="MONITORPAGE";
static int originals,scheduled,cleanup,tail,main_menu,in_release;
static const char *__attribute__((thiscall)) page_name(void*m){assert(m==manager);return page;}
static int __attribute__((thiscall)) original(void*b){assert(b==button);originals++;return -1;}
static void __attribute__((thiscall)) schedule(void*s){assert(s==(void*)0x7ce24c&&!in_release&&g_garage_results_pending==2);scheduled++;page="POST RACE MONITOR";}
static void __attribute__((thiscall)) clean(void*s){assert(s==(void*)0x7ce24c&&scheduled==cleanup+1);((char (__attribute__((stdcall)) *)(char))0x4a0d30)(0);cleanup++;}
static char __attribute__((thiscall)) condition(void*s){assert(s==(void*)0x5ea43c);return 0;}
static void __attribute__((thiscall)) finish_tail(void*s){assert(s==session&&cleanup==tail+1);tail++;}
static int __attribute__((thiscall)) show_main(void*m,const char*p,int n){(void)m;(void)p;(void)n;main_menu++;return 1;}
static void jump(uintptr_t site,void*fn){unsigned char*p=(void*)site;int32_t d=(int32_t)((uintptr_t)fn-site-5);p[0]=0xe9;memcpy(p+1,&d,4);}
static void setup(void){page="MONITORPAGE";g_ready=1;g_stopping=0;g_garage_results_pending=0;*(unsigned char*)0x7ce254=1;*(int*)0x7ce250=37;*(int*)0x7c56a4=7;*(int*)0x7d0410=3;*(int*)0x7c56a8=5;*(int*)0x7c569c=5;*(void**)0x7cd8c0=manager;*(void**)0x7c54e8=player;*(int*)(player+0x4064)=2;*(int*)(button+0xe4)=56;*(void**)(button+0x40)=manager;}
"""
code+='static const unsigned char transition[]={'+','.join(str(b) for b in pe.read_va(0x404f30,60))+'};\n'
code+=r"""
int main(void){
 uintptr_t regions[]={0x400000,0x460000,0x470000,0x490000,0x4a0000,0x5e0000,0x7c0000,0x7d0000};
 for(unsigned i=0;i<sizeof(regions)/sizeof(regions[0]);i++){void *r=VirtualAlloc((void*)regions[i],0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);if(r!=(void*)regions[i]){printf("map failed %lx error %lu\n",(unsigned long)regions[i],GetLastError());return 10;}}
 memcpy((void*)0x475050,garage_release_bytes,sizeof garage_release_bytes);
 memcpy((void*)0x46c030,garage_arm_grid_bytes,sizeof garage_arm_grid_bytes);
 memcpy((void*)0x46c040,garage_grid_tick_bytes,sizeof garage_grid_tick_bytes);
 memcpy((void*)0x497980,garage_page_name_bytes,sizeof garage_page_name_bytes);
 memcpy((void*)0x497940,garage_show_page_bytes,sizeof garage_show_page_bytes);
 memcpy((void*)0x4976c0,garage_find_page_bytes,sizeof garage_find_page_bytes);
 memcpy((void*)0x46ba20,garage_build_grid_bytes,sizeof garage_build_grid_bytes);
 memcpy((void*)0x4a02c2,garage_native_schedule_bytes,sizeof garage_native_schedule_bytes);
 memcpy((void*)0x4a115c,garage_esc_leaf_bytes,sizeof garage_esc_leaf_bytes);
 memcpy((void*)0x404f30,garage_esc_transition_bytes,sizeof garage_esc_transition_bytes);
 g_results_supported=1;assert(broker_garage_native_preflight());
 *(unsigned char*)0x475050^=1;assert(!broker_garage_native_preflight());*(unsigned char*)0x475050^=1;
 *(unsigned char*)0x404f30^=1;assert(!broker_garage_native_preflight());*(unsigned char*)0x404f30^=1;
 g_results_supported=0;assert(!broker_garage_native_preflight());
 memcpy((void*)0x404f30,transition,sizeof transition);memcpy((void*)0x4a0d30,ui_setter,sizeof ui_setter);
 jump(0x497980,page_name);jump(0x4a0250,schedule);jump(0x4a03f0,clean);jump(0x404f10,condition);jump(0x4049d0,finish_tail);jump(0x497940,show_main);
 *(void**)0x5ea43c=session;g_garage_release_original=original;
 broker_garage_release_install(); /* unsupported target refuses */
 for(int maximized=0;maximized<2;maximized++){
  setup();if(maximized)page="MONITOR MAXIMIZED";
  int before=scheduled;in_release=1;
  assert(broker_garage_release_hook(button)==-1&&g_garage_results_pending==1&&scheduled==before&&originals==0);
  assert(broker_garage_release_hook(button)==-1&&scheduled==before);
  in_release=0;
  assert(broker_garage_escape_after_restart(maximized?6:5)==5);
  assert(*(int*)0x7c569c==5);
  assert(*(unsigned char*)0x7ce254==1 && *(int*)0x7ce250==37); /* no Game_Post / scoring reset before grid ticks */
  assert(scheduled==before+1&&cleanup==scheduled&&tail==scheduled&&main_menu==0&&g_garage_results_pending==2);
  assert(!strcmp(page,"POST RACE MONITOR"));
  broker_garage_escape_after_restart(5);assert(scheduled==before+1);
  /* Simulate native completion then ACCEPT returning 6: never hold that exit. */
  g_garage_results_pending=0;page="MAIN MENU";*(int*)0x7c569c=6;
  assert(broker_garage_escape_after_restart(6)==6);
 }
 for(int guard=0;guard<7;guard++){
  setup();int before=originals;
  if(guard==0)page="MAIN MENU";
  if(guard==1)*(int*)0x7c56a4=5;
  if(guard==2)*(int*)0x7d0410=5;
  if(guard==3)*(int*)(player+0x4064)=0;
  if(guard==4)*(int*)(button+0xe4)=55;
  if(guard==5)g_stopping=1;
  if(guard==6)g_ready=0;
  broker_garage_release_hook(button);assert(originals==before+1&&!g_garage_results_pending);
 }
 for(int stale=0;stale<4;stale++){
  setup();broker_garage_release_hook(button);int before=scheduled;
  if(stale==0)page="MAIN MENU";
  if(stale==1)*(void**)0x7c54e8=player+4;
  if(stale==2)g_stopping=1;
  broker_garage_escape_after_restart(stale==3?4:5);
  assert(scheduled==before&&!g_garage_results_pending);
 }
 assert(!g_active_garage_release_calls);
 puts("PASS actual GUI hook/deferred dispatcher + exact native ESC transition: both monitors, no early call, duplicates, correct receiver/state, native cleanup ordering, no direct main menu, stale/teardown guards.");
 return 0;
}
"""
# Windows reserves low addresses in this harness. Relocate the fake game image
# uniformly; native relative calls retain their exact displacement. Only the two
# absolute session-object immediates in the captured transition are rebased.
rebased = bytearray(pe.read_va(0x404f30,60))
for off in (4,14):
    value=int.from_bytes(rebased[off:off+4],'little')
    assert value==0x7ce24c
    rebased[off:off+4]=(value+0x20000000).to_bytes(4,'little')
code=re.sub(r'static const unsigned char transition\[\]=\{[^}]+\}',
    'static const unsigned char transition[]={'+','.join(str(v) for v in rebased)+'}',code)
def relocate_literals(src):
    return re.sub(r'0x[0-9a-fA-F]+',lambda m: hex(int(m[0],16)+0x20000000) if 0x400000<=int(m[0],16)<0x900000 else m[0],src)
(out/'broker_garage_results_native.h').write_text(relocate_literals(text))
(out/'broker_garage_results_layout.h').write_text(relocate_literals(layout))
ui_setter=bytearray(pe.read_va(0x4a0d30,30))
for off in (6,13,19):
    v=int.from_bytes(ui_setter[off:off+4],'little')
    assert v in (0x7ce254,0x7ce250)
    ui_setter[off:off+4]=(v+0x20000000).to_bytes(4,'little')
code='static const unsigned char ui_setter[]={'+','.join(str(v) for v in ui_setter)+'};\n'+code
code=relocate_literals(code)
# Exercise actual capability advertisement and ABI masking, including the
# older broker case; no ESC client or pause-menu arm state exists in this host.
getinfo='F1CBrokerResult __cdecl F1CBrokerGetInfo'+broker.split('F1CBrokerResult __cdecl F1CBrokerGetInfo',1)[1].split('F1CBrokerResult __cdecl F1CBrokerSetHighGearRatio',1)[0]
discovery=(root/'tools/sdk/f1c_broker.c').read_text()
mask='static uint64_t capabilities_for_abi'+discovery.split('static uint64_t capabilities_for_abi',1)[1].split('F1CBrokerDiscoveryResult f1c_broker_discover',1)[0]
cap_fixture='\n#define F1C_BROKER_BUILD 1\n#include "f1c_broker_abi.h"\n#define F1C_BROKER_CAP_PENALTY_HOLD (1ull << 13)\n#define VEH_SLOTS 104u\nstatic int g_penalty_supported,g_preload_supported,g_high_gear_layout_supported;\nstatic int begin_api_call(void){return 1;}\nstatic void end_api_call(void){}\n'+getinfo+mask
code=code.replace('int main(void){',cap_fixture+'\nint main(void){\n F1CBrokerInfo info={0};info.structure_size=sizeof info;\n assert(F1CBrokerGetInfo(&info)==F1C_BROKER_OK && !(info.capabilities & F1C_BROKER_CAP_GARAGE_RESULTS));\n g_garage_release_supported=1;\n assert(F1CBrokerGetInfo(&info)==F1C_BROKER_OK && (info.capabilities & F1C_BROKER_CAP_GARAGE_RESULTS));\n assert(!(capabilities_for_abi(info.capabilities,14)&F1C_BROKER_CAP_GARAGE_RESULTS));\n assert(capabilities_for_abi(info.capabilities,15)&F1C_BROKER_CAP_GARAGE_RESULTS);\n g_garage_release_supported=0;')
source=out/'native_escape_fixture.c';source.write_text(code)
result=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wl,--image-base,0x10000000','-Wl,--disable-dynamicbase','-I'+str(native.parent),'-I'+str(root/'tools/sdk'),str(source),'-o',str(out/'native_escape_fixture.exe')],capture_output=True,text=True)
log=result.stdout+result.stderr
if result.returncode==0:
    result=subprocess.run([str(out/'native_escape_fixture.exe')],capture_output=True,text=True)
    log+=result.stdout+result.stderr
(out/'NATIVE_ESCAPE_RESULTS.txt').write_text(log)
print(log)
assert result.returncode==0,result.returncode
