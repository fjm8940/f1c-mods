from pathlib import Path
import subprocess,time,pefile
root=Path(__file__).resolve().parents[4]
p=root/'development/mods/full_safety_car/trials/v0.10.59_inverted_audio'
o=p/('inverted-tests-'+str(time.time_ns()));o.mkdir()
pe=pefile.PE(str(Path.home()/'Documents/F1 Challenge V10/F1 Challenge Online.exe'))
assert pe.get_data(0xf1fe0,13)==bytes.fromhex('83ec1c55568bf18b8604400000')
code=r"""
#include <stdio.h>
#include "f1c_inverted_removal.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line%d\n",__LINE__);return 1;}}while(0)
static unsigned char car[64];static float requested;
static float request(uintptr_t v){return v==(uintptr_t)car?requested:0;}
int main(void){F1CInvertedRemoval s={0};
 CHECK(!f1c_inverted_elapsed(&s,1,2,0,1,0,0,60));
 for(int i=1;i<480;i++)CHECK(!f1c_inverted_elapsed(&s,1,2,i*.125,1,sinf(i)*.5f,0,60));
 CHECK(f1c_inverted_elapsed(&s,1,2,60,1,.1f,0,60)>60);
 CHECK(!f1c_inverted_elapsed(&s,1,2,60,1,.1f,0,60)); /* pause */
 CHECK(!f1c_inverted_elapsed(&s,1,2,60.125,0,.1f,0,60));CHECK(!s.valid);
 CHECK(!f1c_inverted_elapsed(&s,1,2,61,1,0,0,60));
 CHECK(!f1c_inverted_elapsed(&s,1,3,62,1,0,0,60));CHECK(s.elapsed==0);
 CHECK(!f1c_inverted_elapsed(&s,2,3,63,1,0,0,60));CHECK(s.elapsed==0);
 CHECK(!f1c_inverted_elapsed(&s,2,3,70,1,0,0,60));CHECK(s.elapsed==0);
 CHECK(!f1c_inverted_elapsed(&s,2,3,71,1,11,0,60));CHECK(s.elapsed==0);
 CHECK(!f1c_inverted_elapsed(&s,2,3,72,1,11,0,NAN));CHECK(!s.valid);
 /* Actual native stolen prologue, followed by isolated argument recorder.
    Verifies real detour/trampoline thiscall and ret4, not game cleanup. */
 unsigned char bytes[]={0x83,0xec,0x1c,0x55,0x56,0x8b,0xf1,0xd9,0x44,0x24,0x28,0xd9,0x59,0x04,0x5e,0x5d,0x83,0xc4,0x1c,0xc2,0x04,0x00};
 void*site=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(site);memcpy(site,bytes,sizeof(bytes));
 *(uint32_t*)(car+12)=(uint32_t)car;
 F1CNativeRemoval call=(F1CNativeRemoval)site;
 f1c_inverted_request=request;f1c_inverted_thread=GetCurrentThreadId();
 CHECK(f1c_detour_install_prevalidated_layout(&f1c_inverted_hook,site,(void*)f1c_inverted_detour,bytes,7)==F1C_DETOUR_OK);
 requested=60.25f;
 for(int i=0;i<1000;i++){call(car+12,.125f);CHECK(*(float*)(car+16)==60.25f);}
 requested=0;call(car+12,.125f);CHECK(*(float*)(car+16)==.125f);
 requested=60.25f;f1c_inverted_thread++;call(car+12,.125f);CHECK(*(float*)(car+16)==.125f);f1c_inverted_thread--;
 *(uint32_t*)(car+12)=0;call(car+12,.125f);CHECK(*(float*)(car+16)==.125f);
 CHECK(f1c_inverted_remove(GetCurrentThreadId()));CHECK(!memcmp(site,bytes,sizeof(bytes)));
 puts("PASS sustained inversion rocking, pause/recovery/identity/epoch/gap/movement/invalid resets; actual prologue detour and float thiscall argument, wrong thread/object, unhook");return 0;}
"""
(o/'test.c').write_text(code)
subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-O2','-std=c11','-I'+str(root/'f1c_ida_decompiled/tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],check=True)
subprocess.run([str(o/'test.exe')],check=True)
s=(p/'safety_car_guardrails.c').read_text()
h=s[s.index('static void sc_handover_pump'):s.index('#include "sc_progress_watch.h"')]
assert 'if(!in_range && !pit_owned(g_sc_slot))return;' in h
assert h.index('SCORE_SCAR05')<h.index('g_pit_handover=1')
r=s[s.index('/* Pit ownership, never damage/retirement, triggers the return. */'):s.index('static void engage(void)',s.index('/* Pit ownership, never damage/retirement, triggers the return. */'))]
assert r.index('pit_owned(g_sc_slot)')<r.index('SCORE_SCAR04')<r.index('sc_initial_restore()')
assert 'if(!g_sc_said05)' in h and 'if(!g_sc_said05)' in r
print('PASS announcement event placement and once-per-outing entering latch; reset success does not define lane entry')
