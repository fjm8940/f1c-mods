"""Internal SDK gate; invoke via the canonical f1c_mod.py sdk-test."""
from pathlib import Path
import datetime
import subprocess
import pefile

root = Path(__file__).resolve().parents[3]
sdk = root / "tools" / "sdk"
out = sdk / "tests" / "evidence" / ("vehicle_clear_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
out.mkdir(parents=True)
exe = Path.home() / "Documents" / "F1 Challenge V10" / "F1 Challenge Online.exe"
expected = bytes.fromhex("56 8b f1 57 8d 7e 0c 57 b9 60 a5 5e 00 e8 ae 9c f4 ff")
pe = pefile.PE(str(exe))
assert pe.get_data(0xC6E80, len(expected)) == expected
candidate = root.parent / "development" / "mods" / "full_safety_car" / "trials" / "v0.10.67_session_reentry_cleanup" / "safety_car_guardrails.c"
source = candidate.read_text(encoding="utf-8")
end = source[source.index("static void __cdecl sc_session_end"):source.index("static void sc_finish_pump")]
assert end.index("f1c_vehicle_native_clear_dormant") < end.index("abandon_session();")
aidcs = source[source.index("static int __stdcall aidcs_detour"):source.index("static int __cdecl dynrestart_detour")]
assert "stale Safety Car at slot" in aidcs and "dynrestart_detour re-arms once" not in aidcs
code = r'''
#include "f1c_vehicle_native_clear.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static unsigned char car[0x5000];
static unsigned char site[]={0x56,0x8b,0xf1,0x57,0x8d,0x7e,0x0c,0x57,0xb9,0x60,0xa5,0x5e,0,0xe8,0xae,0x9c,0xf4,0xff};
static int calls;
static int rd(uintptr_t a,void*out,size_t n){
 uintptr_t b=(uintptr_t)car;
 if(a<b||a+n>b+sizeof(car))return 0;
 memcpy(out,(void*)a,n);return 1;
}
static int __attribute__((thiscall)) clear_mock(void*v){
 ++calls;*(int*)((unsigned char*)v+0x4898)=-1;return 1;
}
static void set(int role,int roster){*(int*)(car+0x32c8)=role;*(int*)(car+0x4898)=roster;calls=0;}
int main(void){
 DWORD thread=GetCurrentThreadId();uintptr_t v=(uintptr_t)car;
 set(-1,272);CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,272,thread,0,site,clear_mock));CHECK(calls==0);
 CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,272,thread+1,1,site,clear_mock));CHECK(calls==0);
 CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,271,thread,1,site,clear_mock));CHECK(calls==0);
 set(1,272);CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,272,thread,1,site,clear_mock));CHECK(calls==0);
 set(-1,272);site[0]=0x90;CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,272,thread,1,site,clear_mock));CHECK(calls==0);site[0]=0x56;
 CHECK(f1c_vehicle_native_clear_dormant_at(rd,v,272,thread,1,site,clear_mock));CHECK(calls==1);
 CHECK(*(int*)(car+0x4898)==-1&&*(int*)(car+0x32c8)==-1);
 CHECK(!f1c_vehicle_native_clear_dormant_at(rd,v,272,thread,1,site,clear_mock));CHECK(calls==1);
 puts("PASS exact native entry bytes; dormant identity, authority, thread and once-only guards");return 0;
}
'''
(out / "test.c").write_text(code)
compiler = Path("C:/msys64/mingw32/bin/gcc.exe")
subprocess.run([str(compiler), "-O2", "-std=c11", "-I" + str(sdk), str(out / "test.c"), "-o", str(out / "test.exe")], check=True)
subprocess.run([str(out / "test.exe")], check=True)
print("PASS vehicle native clear candidate source and exact-target guards")
