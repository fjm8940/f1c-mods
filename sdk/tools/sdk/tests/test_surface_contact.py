from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3];o=r/'tools/sdk/tests/evidence'/('surface_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r"""
#include "f1c_surface_contact.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char mem[0x10000];
static int rd(uintptr_t p,void*d,size_t n){if(p>sizeof(mem)||n>sizeof(mem)-p)return 0;memcpy(d,mem+p,n);return 1;}
static void put(int p,uint32_t v){memcpy(mem+p,&v,4);}
int main(void){
 for(int i=0;i<4;i++){put(0x1000+0x3674+i*0xf8,0x6000+i*0x100);put(0x6054+i*0x100,0x8000+i*0x100);}
 uint32_t tags[4][3]={{6,21,27},{7,22,27},{11,21,27},{9,23,28}};
 for(int i=0;i<4;i++)memcpy(mem+0x8030+i*0x100,tags[i],12);
 F1CSurfaceContacts a=f1c_surface_contacts(0x1000,rd);assert(a.known==4&&a.drivable==3);
 put(0x1000+0x3674,0xffffffff);a=f1c_surface_contacts(0x1000,rd);assert(a.known==3&&a.drivable==2);
 memset(mem,0,sizeof(mem));a=f1c_surface_contacts(0x1000,rd);assert(!a.known&&!a.drivable);
 assert(!f1c_surface_contacts(UINTPTR_MAX-1,rd).known);
 puts("PASS exact surface tags, mixed edge contacts, unreadable/null/overflow guards; no writes");
}
"""
(o/'surface.c').write_text(s)
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(r/'tools/sdk'),str(o/'surface.c'),'-o',str(o/'surface.exe')],capture_output=True,text=True);log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'surface.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log);print(log);raise SystemExit(a.returncode)
