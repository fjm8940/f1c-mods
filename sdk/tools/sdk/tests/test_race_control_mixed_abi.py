"""Mixed-ABI race-control compatibility test (option 3, 2026-10-08).

Verifies that a Safety Car built on the pinned pre-Batch-C ABI 1 header
(SHA-256 5CA9D6C5..., the shipped SC 0.10.91 reader) and a Batch-C mod built on
the NEW header can share the frozen `F1C_RACE_CONTROL_V1_*` block in BOTH
creation orders: both opens succeed (no refusal). The MF "car limited" record is
now a SEPARATE mapping, so it cannot break SC.

Also proves the MF helpers work with and without the SC/RF block present.

One process per case (the mapping name embeds the PID and process creation
time). The ABI 1 translation unit uses the exact pre-Batch-C header, whose
SHA-256 is pinned below.
"""
from pathlib import Path
import subprocess, datetime, hashlib

root = Path(__file__).resolve().parents[3]
out = root / 'tools/sdk/tests/evidence' / ('race_control_mixed_abi_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
out.mkdir(parents=True)

ABI1_SHA = '5CA9D6C5C8D03877D484A40FB42F75EAD3B3C90D866E4E0237C7108CD879C33A'
abi1_src = root / 'tools/sdk/f1c_race_control.h.BEFORE_20261008_acC'
if not abi1_src.exists():
    raise SystemExit(f'ABI 1 before-copy missing: {abi1_src}')
got = hashlib.sha256(abi1_src.read_bytes()).hexdigest().upper()
if got != ABI1_SHA:
    raise SystemExit(f'ABI 1 before-copy hash changed: {got} != {ABI1_SHA}')
(out / 'abi1_snapshot.h').write_bytes(abi1_src.read_bytes())

A1 = '#include "abi1_snapshot.h"\nint abi1_open_ok(void){ return f1c_rc_open()!=0; }\n'
A2 = r'''#include <stdint.h>
#include "f1c_race_control.h"
int a2_sc_open_ok(void){ return f1c_rc_open()!=0; }
int a2_mf_open_ok(void){ return f1c_rc_mf_open()!=0; }
void a2_mf_set(uintptr_t v){ f1c_rc_mf_set(f1c_rc_mf_open(),v); }
int a2_mf_limited(uintptr_t v){ return f1c_rc_mf_limited(f1c_rc_mf_open(),v); }
void a2_mf_clear_all(void){ f1c_rc_mf_clear_all(f1c_rc_mf_open()); }
'''
MAIN = r'''#include <stdio.h>
#include <stdint.h>
int abi1_open_ok(void);
int a2_sc_open_ok(void);
int a2_mf_open_ok(void);
void a2_mf_set(uintptr_t v);
int a2_mf_limited(uintptr_t v);
void a2_mf_clear_all(void);
int main(int argc,char**argv){
    uintptr_t A=(uintptr_t)0x5EC6D8u, B=(uintptr_t)0x5F0F98u;
    if(argc<2)return 2;
    if(argv[1][0]=='1'){
        int a2=a2_sc_open_ok();
        int a1=abi1_open_ok();
        printf("order=abi2-first abi2_open=%d abi1_open=%d\n",a2,a1);
        if(!a2||!a1){printf("FAIL: shared block refused in abi2-first order\n");return 1;}
        printf("RESULT: both readers open the frozen V1 block (abi2-first)\n");
        return 0;
    } else if(argv[1][0]=='2'){
        int a1=abi1_open_ok();
        int a2=a2_sc_open_ok();
        printf("order=abi1-first abi1_open=%d abi2_open=%d\n",a1,a2);
        if(!a1||!a2){printf("FAIL: shared block refused in abi1-first order\n");return 1;}
        printf("RESULT: both readers open the frozen V1 block (abi1-first)\n");
        return 0;
    } else {
        /* MF helpers work WITH and WITHOUT the SC block present */
        if(!a2_mf_open_ok()){printf("FAIL: MF block did not open\n");return 1;}
        a2_mf_set(A);
        if(!a2_mf_limited(A)){printf("FAIL: MF not limited without SC block\n");return 1;}
        if(a2_mf_limited(B)){printf("FAIL: MF over-limited without SC block\n");return 1;}
        printf("MF without SC: open=1 limited(A)=1 limited(B)=0\n");
        if(!a2_sc_open_ok()){printf("FAIL: SC block did not open\n");return 1;}
        if(!a2_mf_limited(A)){printf("FAIL: MF record lost with SC block present\n");return 1;}
        a2_mf_clear_all();
        if(a2_mf_limited(A)){printf("FAIL: MF clear_all failed\n");return 1;}
        printf("MF with SC: record intact, clear_all ok\n");
        printf("RESULT: MF helpers are independent of the SC block\n");
        return 0;
    }
}
'''

(out / 'a1.c').write_text(A1)
(out / 'a2.c').write_text(A2)
(out / 'mixed_main.c').write_text(MAIN)

cc = [r'C:\msys64\mingw32\bin\gcc.exe', '-std=c11', '-O2', '-Wall', '-Werror',
      '-Wno-unused-function', '-I' + str(root / 'tools/sdk')]
exe = out / 'mixed.exe'
log = ''
for cmd in [cc + [str(out / 'a1.c'), str(out / 'a2.c'), str(out / 'mixed_main.c'), '-o', str(exe)],
            [str(exe), '1'], [str(exe), '2'], [str(exe), '3']]:
    a = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    log += '$ ' + ' '.join(cmd) + '\n' + a.stdout + a.stderr
    if a.returncode:
        log += f'[exit {a.returncode}]\n'
        break
(out / 'RESULTS.txt').write_text(log, encoding='utf-8')
print(log)
raise SystemExit(a.returncode)
