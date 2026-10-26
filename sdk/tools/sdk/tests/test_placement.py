"""Internal SDK gate; invoke through f1c_mod.py sdk-test."""
from pathlib import Path
import sys,datetime,subprocess,re,hashlib,io,contextlib,types
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools'))
import f1c_mod as cli
out=root/'tools/sdk/tests/evidence'/('placement_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));out.mkdir(parents=True)
wrong=out/'wrong.bin';wrong.write_bytes(b'not the supported game')
try:cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(wrong),address=0x4ef7c0,size=64));raise AssertionError('wrong hash accepted')
except cli.ModError:pass
exe=Path.home()/'Documents/F1 Challenge V10/F1 Challenge Online.exe'
if not exe.exists():exe=cli.DEFAULT_EXE
assert hashlib.sha256(exe.read_bytes()).hexdigest().upper()==cli.TARGET_SHA256
for address,size in [(0x4efb20,163),(0x4ef7c0,353),(0x4f2320,98),(0x42b789,36)]:
 b=io.StringIO()
 with contextlib.redirect_stdout(b):cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(exe),address=address,size=size))
 (out/f'{address:x}.txt').write_text(b.getvalue())
 if address in (0x4efb20,0x4ef7c0):assert 'ret 8' in b.getvalue()
for size in (0,4097):
 try:cli.cmd_exe_disassemble(types.SimpleNamespace(exe=str(exe),address=0x4ef7c0,size=size));raise AssertionError('bad size accepted')
 except cli.ModError:pass
h=(root/'tools/sdk/f1c_placement.h').read_text()
h=re.sub(r'static int f1c_placement_code_valid\(void\)\{.*?\n\}', 'static int f1c_placement_code_valid(void){return code_ok;}',h,flags=re.S)
h=h.replace('#define F1C_PLACEMENT_GET 0x004EFB20u','#define F1C_PLACEMENT_GET mock_get').replace('#define F1C_PLACEMENT_SET 0x004EF7C0u','#define F1C_PLACEMENT_SET mock_set').replace('#define F1C_MOTION_RESET 0x004F2320u','#define F1C_MOTION_RESET mock_reset')
prefix=r'''
#include <assert.h>
#include <stdio.h>
static unsigned char vehicle[0x48c0];static int code_ok=1,calls,reset_done;
static void __attribute__((thiscall)) mock_get(void *v,float*p,float*r){assert(v==vehicle+12);for(int i=0;i<3;i++){p[i]=10+i;r[i]=.1f*i;}calls++;}
static void __attribute__((thiscall)) mock_set(void *v,const float*p,const float*r){assert(v==vehicle+12&&reset_done);assert(p[0]==10&&r[0]==0);calls++;}
static void __attribute__((thiscall)) mock_reset(void*v){assert(v==vehicle+12);reset_done=1;calls++;}
'''
main=r'''
int main(void){F1CPlacement p;assert(f1c_placement_capture(vehicle,&p)&&calls==1);assert(f1c_placement_restore(vehicle,&p)&&calls==3);
code_ok=0;assert(!f1c_placement_capture(vehicle,&p)&&!f1c_placement_restore(vehicle,&p)&&calls==3);
code_ok=1;p.position[1]=NAN;assert(!f1c_placement_restore(vehicle,&p)&&calls==3);assert(!f1c_placement_capture(0,&p));
puts("PASS placement wrappers: x86 thiscall, receiver +12, get/reset/set order, invalid pose/code gate");}
'''
(out/'placement.c').write_text(prefix+h+main)
r=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-Wall','-Wextra',str(out/'placement.c'),'-o',str(out/'placement.exe')],capture_output=True,text=True);log=r.stdout+r.stderr
if not r.returncode:
 r=subprocess.run([str(out/'placement.exe')],capture_output=True,text=True);log+=r.stdout+r.stderr
(out/'RESULTS.txt').write_text(log);print(log);assert r.returncode==0
print('PASS canonical disassembly: wrong hash/invalid bounds refused, reviewed tails and placement wrappers. Evidence:',out)
