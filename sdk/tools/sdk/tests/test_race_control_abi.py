"""Race-control ABI 1 + separate MF block test (option 3, 2026-10-08).

Proves the frozen ABI 1 SC/RF block layout and the new SEPARATE Mechanical
Failures "car limited" mapping:
  - F1CRaceControl is exactly ABI 1 (size 464; owner@12, controlled@48).
  - F1CMfLimited is its own block (magic/abi/size; mf_revision, mf_limited[104]).
  - the MF helpers drive only the MF block and are inert when it is absent.
No SC/RF lease behaviour is changed. Creates the mappings in-process and exits.
"""
from pathlib import Path
import subprocess, datetime

root = Path(__file__).resolve().parents[3]
out = root / 'tools/sdk/tests/evidence' / ('race_control_abi_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
out.mkdir(parents=True)
cc = [r'C:\msys64\mingw32\bin\gcc.exe', '-std=c11', '-O2', '-Wall', '-Werror',
      '-Wno-unused-function', '-I' + str(root / 'tools/sdk')]
exe = r"""
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "f1c_race_control.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
_Static_assert(offsetof(F1CRaceControl,owner)==12,"owner offset");
_Static_assert(offsetof(F1CRaceControl,controlled)==48,"controlled offset");
_Static_assert(F1C_RC_SIZE_V1==464,"v1 size");
_Static_assert(sizeof(F1CRaceControl)==F1C_RC_SIZE_V1,"frozen ABI 1 size");
_Static_assert(offsetof(F1CMfLimited,magic)==0,"mf magic offset");
_Static_assert(offsetof(F1CMfLimited,abi)==4,"mf abi offset");
_Static_assert(offsetof(F1CMfLimited,size)==8,"mf size offset");
_Static_assert(offsetof(F1CMfLimited,mf_limited)==16,"mf_limited offset");
int main(void){
    F1CRaceControl *rc; F1CMfLimited *mf;
    CHECK(F1C_MF_ABI==2);
    /* inert when the MF block is absent */
    CHECK(f1c_rc_mf_supported(NULL)==0);
    CHECK(f1c_rc_mf_limited(NULL,0x2000)==0);
    f1c_rc_mf_set(NULL,0x2000); f1c_rc_mf_clear(NULL,0x2000); f1c_rc_mf_clear_all(NULL);

    rc=f1c_rc_open(); CHECK(rc);
    CHECK(rc->abi==1 && rc->size==F1C_RC_SIZE_V1);   /* frozen ABI 1 block */

    mf=f1c_rc_mf_open(); CHECK(mf);
    CHECK(mf->magic==F1C_MF_MAGIC && mf->abi==F1C_MF_ABI);
    CHECK(f1c_rc_mf_supported(mf)==1);
    f1c_rc_mf_set(mf,0x2000); CHECK(f1c_rc_mf_limited(mf,0x2000)==1);
    f1c_rc_mf_set(mf,0x2000);                        /* idempotent */
    f1c_rc_mf_set(mf,0x3000); CHECK(f1c_rc_mf_limited(mf,0x3000)==1);
    f1c_rc_mf_clear(mf,0x2000);
    CHECK(f1c_rc_mf_limited(mf,0x2000)==0 && f1c_rc_mf_limited(mf,0x3000)==1);
    f1c_rc_mf_clear_all(mf); CHECK(f1c_rc_mf_limited(mf,0x3000)==0);
    /* the SC/RF lease is a different mapping and is unaffected */
    CHECK(f1c_rc_sc_enter(rc)); CHECK(f1c_rc_read(&rc->owner)==F1C_RC_SC);
    f1c_rc_mf_set(mf,0x4000); CHECK(f1c_rc_mf_limited(mf,0x4000)==1);
    f1c_rc_sc_release(rc); CHECK(f1c_rc_read(&rc->owner)==F1C_RC_NONE);
    puts("PASS frozen ABI1 SC/RF block (464) + separate MF ABI2 block; MF helpers "
         "use only the MF block and are inert when absent");
    return 0;
}
"""
(out / 'test.c').write_text(exe)
log = ''
for cmd in [cc + [str(out / 'test.c'), '-o', str(out / 'test.exe')], [str(out / 'test.exe')]]:
    a = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    log += a.stdout + a.stderr
    if a.returncode:
        break
(out / 'RESULTS.txt').write_text(log, encoding='utf-8')
print(log)
raise SystemExit(a.returncode)
