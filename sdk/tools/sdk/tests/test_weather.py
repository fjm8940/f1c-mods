
from pathlib import Path
import subprocess,datetime
r=Path(__file__).resolve().parents[3];o=r/'tools/sdk/tests/evidence'/('weather_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));o.mkdir(parents=True)
s=r"""
#include "f1c_weather.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static float data[2];static int readable=1;
static int rd(uintptr_t p,void*d,size_t n){assert(p==0x008AFEE8u&&n==8);if(!readable)return 0;memcpy(d,data,n);return 1;}
int main(void){
 data[0]=.97f;data[1]=1;F1CWeather w=f1c_weather_read(rd);assert(w.valid&&fabs(w.on_path-.97)<0.000001&&w.off_path==1);
 data[0]=NAN;assert(!f1c_weather_read(rd).valid);data[0]=INFINITY;assert(!f1c_weather_read(rd).valid);
 data[0]=-1;assert(!f1c_weather_read(rd).valid);data[0]=0;data[1]=1.1f;assert(!f1c_weather_read(rd).valid);
 data[1]=0;assert(f1c_weather_read(rd).valid);readable=0;assert(!f1c_weather_read(rd).valid);assert(!f1c_weather_read(0).valid);
 puts("PASS native wetness reader: exact bounded address, valid zero/full wetness, NaN/infinity/range/null/unreadable rejection; no writes");
}
"""
(o/'weather.c').write_text(s,encoding='utf-8')
a=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-I'+str(r/'tools/sdk'),str(o/'weather.c'),'-o',str(o/'weather.exe')],capture_output=True,text=True);log=a.stdout+a.stderr
if not a.returncode:
 a=subprocess.run([str(o/'weather.exe')],capture_output=True,text=True);log+=a.stdout+a.stderr
(o/'RESULTS.txt').write_text(log,encoding='utf-8');print(log);raise SystemExit(a.returncode)
