from pathlib import Path
import re, subprocess, datetime
ROOT=Path(__file__).resolve().parents[3]
C=ROOT.parent/'development/mods/full_safety_car/trials/v0.10.16_guardrails'
s=(C/'safety_car_guardrails.c').read_text(encoding='utf-8')
def f(name):
 m=re.search(r'^static [^\n;]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 a=s.index('{',m.start());d=1;b=a+1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[m.start():b]
assert 'sc_action_authorized()' in f('sc_deploy')
assert 'sc_offline_authorized()' in f('aidcs_detour') and 'sc_offline_authorized()' in f('dynrestart_detour') and 'sc_offline_authorized()' in f('sc_pump')
assert 'g_rf_handoff' in f('release') and 'hotkeys_allowed && fresh_edge[i]' in f('game_tick')
h=f('sc_hotkey_foreground_plain');o=ROOT/'tools/sdk/tests/evidence'/('sc_guardrails_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
code='''#include <assert.h>\ntypedef void* HWND;typedef unsigned long DWORD;typedef short SHORT;\n#define VK_CONTROL 1\n#define VK_MENU 2\n#define VK_SHIFT 3\n#define VK_LWIN 4\n#define VK_RWIN 5\nstatic HWND w;static DWORD p;static unsigned m;static HWND GetForegroundWindow(void){return w;}static DWORD GetWindowThreadProcessId(HWND x,DWORD*y){(void)x;*y=p;return 1;}static DWORD GetCurrentProcessId(void){return 7;}static SHORT GetAsyncKeyState(int x){return(m&(1u<<x))?0x8000:0;}\n'''+h+'''\nint main(){int q=0;w=(HWND)1;p=7;assert(!sc_hotkey_foreground_plain(&q));assert(sc_hotkey_foreground_plain(&q));m=1u<<VK_SHIFT;assert(!sc_hotkey_foreground_plain(&q));m=0;p=8;assert(!sc_hotkey_foreground_plain(&q));p=7;assert(!sc_hotkey_foreground_plain(&q));return 0;}'''
(o/'a.c').write_text(code);x=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11',str(o/'a.c'),'-o',str(o/'a.exe')],capture_output=True,text=True);log=x.stdout+x.stderr
if not x.returncode:x=subprocess.run([str(o/'a.exe')],capture_output=True,text=True);log+=x.stdout+x.stderr
(o/'RESULTS.txt').write_text(log);assert not x.returncode,log
print('Safety Car action adapters: deploy/loader/pump bindings and focus/modifier edge adapter PASS')
