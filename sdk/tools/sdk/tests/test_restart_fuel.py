"""Exact native bytes, capacity bounds, player/AI receiver and refusal tests."""
from pathlib import Path
import subprocess, time, json

root=Path(__file__).resolve().parents[3]
out=root/'tools/sdk/tests/evidence'/('restart_fuel_'+str(time.time_ns()))
out.mkdir(parents=True)
exe=Path.home()/'Documents/F1 Challenge V10/F1 Challenge Online.exe'
native={}
for name,address,size in [('fuel','0x4F1890',33),('penalty','0x4A5660',65)]:
    result=subprocess.run(['python',str(root/'tools/f1c_mod.py'),'exe-disassemble',
        '--exe',str(exe),'--address',address,'--size',str(size)],capture_output=True,text=True)
    (out/(name+'-disassembly.txt')).write_text(result.stdout+result.stderr)
    result.check_returncode()
    native[name]=bytes.fromhex(''.join(line.split()[1] for line in result.stdout.splitlines() if line.strip()))
    assert len(native[name])==size
arrays='\n'.join('unsigned char '+name+'_code[]={'+','.join(str(b) for b in data)+'};' for name,data in native.items())
code=r'''
#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include "f1c_restart_fuel.h"
#include "f1c_restart_penalties.h"
static unsigned char car[2][0x48c0];
static int calls;
static double player_mirror;
static F1CRestartFuelSet native_setter;
static void __attribute__((thiscall)) setter(void *body,float fuel){
 assert(body==car[0]+12||body==car[1]+12);
 native_setter(body,fuel);
 calls++;
}
int main(void){
'''+arrays+r'''
 assert(f1c_restart_fuel_code_valid(fuel_code,sizeof fuel_code));
 assert(f1c_restart_penalty_mode_code_valid(penalty_code,sizeof penalty_code));
 /* Execute the reviewed leaf in this test process. Relocate only its absolute
    player-mirror destination; no game process or fixed game page is touched. */
 unsigned char *thunk=VirtualAlloc(NULL,sizeof fuel_code,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 assert(thunk);memcpy(thunk,fuel_code,sizeof fuel_code);
 uint32_t mirror_address=(uint32_t)(uintptr_t)&player_mirror;
 memcpy(thunk+26,&mirror_address,4);
 FlushInstructionCache(GetCurrentProcess(),thunk,sizeof fuel_code);
 native_setter=(F1CRestartFuelSet)thunk;
 *(int*)(car[1]+12+0x32bc)=1; /* native AI role: leave player mirror alone */
 for(size_t i=0;i<sizeof fuel_code;i++){
  fuel_code[i]^=1;assert(!f1c_restart_fuel_code_valid(fuel_code,sizeof fuel_code));fuel_code[i]^=1;
 }
 for(size_t i=0;i<sizeof penalty_code;i++){
  penalty_code[i]^=1;assert(!f1c_restart_penalty_mode_code_valid(penalty_code,sizeof penalty_code));penalty_code[i]^=1;
 }
 assert(!f1c_restart_fuel_code_valid(NULL,33));assert(!f1c_restart_fuel_code_valid(fuel_code,32));
 assert(!f1c_restart_penalty_mode_code_valid(NULL,65));assert(!f1c_restart_penalty_mode_code_valid(penalty_code,64));
 float capacity=80;memcpy(car[0]+F1C_RESTART_FUEL_CAPACITY_OFFSET,&capacity,4);
 capacity=150;memcpy(car[1]+F1C_RESTART_FUEL_CAPACITY_OFFSET,&capacity,4);
 assert(f1c_restart_fuel_apply(car[0],sizeof car[0],40,fuel_code,33,setter));
 assert(player_mirror==40&&calls==1);
 assert(f1c_restart_fuel_apply(car[1],sizeof car[1],150,fuel_code,33,setter));
 assert(player_mirror==40&&calls==2);
 float chosen=-1;
 assert(f1c_restart_fuel_target(car[0],sizeof car[0],1,&chosen)&&chosen==80);
 assert(f1c_restart_fuel_target(car[0],sizeof car[0],0,&chosen)&&chosen==40);
 float consumed=36.5f;memcpy(car[0]+F1C_RESTART_FUEL_LEVEL_OFFSET,&consumed,4);
 assert(f1c_restart_fuel_target(car[0],sizeof car[0],0,&chosen)&&chosen==36.5f);
 assert(f1c_restart_fuel_target(car[1],sizeof car[1],1,&chosen)&&chosen==150);
 assert(!f1c_restart_fuel_target(car[1],sizeof car[1],1,NULL));
 unsigned char before[sizeof car];memcpy(before,car,sizeof car);
 float invalid[]={NAN,INFINITY,-1,81,1001};
 for(size_t i=0;i<sizeof invalid/sizeof invalid[0];i++)
  assert(!f1c_restart_fuel_apply(car[0],sizeof car[0],invalid[i],fuel_code,33,setter));
 assert(!f1c_restart_fuel_apply(car[0],F1C_RESTART_FUEL_CAPACITY_OFFSET,40,fuel_code,33,setter));
 assert(!f1c_restart_fuel_apply(car[0],sizeof car[0],40,fuel_code,32,setter));
 assert(!f1c_restart_fuel_apply(car[0],sizeof car[0],40,fuel_code,33,NULL));
 assert(calls==2&&!memcmp(car,before,sizeof car));
 capacity=NAN;memcpy(car[1]+F1C_RESTART_FUEL_CAPACITY_OFFSET,&capacity,4);
 assert(!f1c_restart_fuel_apply(car[1],sizeof car[1],0,fuel_code,33,setter));
 puts("PASS complete native byte fingerprints, all single-byte mutations, model bounds, +12 receiver, executed native player/AI mirror leaf, refusal without writes");
 return 0;
}
'''
(out/'test.c').write_text(code)
commands=[['C:/msys64/mingw32/bin/gcc.exe','-std=c11','-Wall','-Wextra','-O2',
    '-I'+str(root/'tools/sdk'),str(out/'test.c'),'-o',str(out/'test.exe')],[str(out/'test.exe')]]
for index,cmd in enumerate(commands):
    result=subprocess.run(cmd,capture_output=True,text=True)
    (out/(str(index)+'.txt')).write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr);result.check_returncode()
print(out)
