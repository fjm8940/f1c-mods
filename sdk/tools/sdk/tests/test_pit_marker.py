"""Canonical SDK gate; synthetic memory for reviewed cached marker chain."""
from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3];o=r/'tools/sdk/tests/evidence'/('pit_marker_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r'''
#include "f1c_pit_marker.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char m[0x4000];
static int read_at(uintptr_t a,void *out,size_t n){if(a<0x1000||a+n>sizeof(m))return 0;memcpy(out,m+a,n);return 1;}
static void put(uintptr_t a,uint32_t n){memcpy(m+a,&n,4);}
int main(void){float marker[3]={20,3,40},out[3]={9,9,9},car[3]={120,3,40};
 put(0x1194,0x2000);put(0x2204,0x3000);put(0x3014,0x20000000);memcpy(m+0x30e8,marker,12);
 assert(f1c_pit_entry_marker(0x1000,read_at,out));assert(!memcmp(out,marker,12));
 assert(f1c_pit_within_100(car,out));car[0]=120.01f;assert(!f1c_pit_within_100(car,out));
 car[0]=20;car[1]=24;assert(!f1c_pit_within_100(car,out));car[1]=3;assert(f1c_pit_within_100(car,out));
 put(0x3014,0);assert(!f1c_pit_entry_marker(0x1000,read_at,out));assert(!memcmp(out,marker,12));
 /* Reproduce live failure: uncached world zeros, valid local center/identity. */
 float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 float live[3]={-20.6694717f,-7.1777077f,-215.739746f};
 memcpy(m+0x21a0,identity,64);memcpy(m+0x30dc,live,12);
 put(0x3014,0);assert(f1c_pit_entry_marker(0x1000,read_at,out));assert(!memcmp(out,live,12));
 float sc[3]={-40.3773727f,-12.064287f,-263.994965f};assert(f1c_pit_within_100(sc,out));
 /* Non-identity translation/rotation, same layout as native0055EEAF. */
 float transform[16]={0,0,-1,0,0,1,0,0,1,0,0,0,10,20,30,1};float local[3]={2,3,4};
 memcpy(m+0x21a0,transform,64);memcpy(m+0x30dc,local,12);
 assert(f1c_pit_entry_marker(0x1000,read_at,out));assert(out[0]==14&&out[1]==23&&out[2]==28);
 uint32_t flags_after;memcpy(&flags_after,m+0x3014,4);assert(flags_after==0); /* never populate game cache */
 put(0x21a0,0x7fc00000);assert(!f1c_pit_entry_marker(0x1000,read_at,out));
 put(0x3014,0x20000000);put(0x30e8,0x7fc00000);assert(!f1c_pit_entry_marker(0x1000,read_at,out));
 put(0x2204,0xfffffff0);assert(!f1c_pit_entry_marker(0x1000,read_at,out));
 assert(!f1c_pit_entry_marker(0,read_at,out));
 puts("PASS native marker chain, cached/uncached transform, live52m reproduction, finite/readability guards, unchanged failure output,100m boundary and height guard");}
'''
(o/'test.c').write_text(s);x=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(r/'tools/sdk'),str(o/'test.c'),'-o',str(o/'test.exe')],capture_output=True,text=True);log=x.stdout+x.stderr
if not x.returncode:x=subprocess.run([str(o/'test.exe')],capture_output=True,text=True);log+=x.stdout+x.stderr
(o/'RESULTS.txt').write_text(log);print(log);assert x.returncode==0
