"""Canonical read-only Safety Car status decoder tests."""
from pathlib import Path
import sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from safety_car_status import decode
b=bytearray(444)
struct.pack_into('<I',b,0,0x50414345)
struct.pack_into('<II',b,436,1,444)
struct.pack_into('<i',b,188,255)
struct.pack_into('<f',b,224,1.25)
struct.pack_into('<i',b,220,20)
for offset,value in ((100,7),(104,24),(108,1),(128,23)):
 struct.pack_into('<i',b,offset,value)
r=decode(bytes(b));assert all(r['gates'].values()) and r['trig_worst_speed']==1.25 and r['trig_worst_slot']==20
assert (r['leader_slot'],r['sc_slot'],r['sc_hidden'],r['live_cars'])==(7,24,1,23)
struct.pack_into('<i',b,188,43)
r=decode(bytes(b));assert r['gates']['auto_enabled'] and not r['gates']['incident_corroborated']
for bad in (bytes(b[:-1]),bytes(444)):
 try:decode(bad)
 except ValueError:pass
 else:raise AssertionError('accepted invalid block')
struct.pack_into('<II',b,436,2,444)
try:decode(bytes(b))
except ValueError:pass
else:raise AssertionError('accepted unsupported ABI')
print('PASS Safety Car diagnostic decode: gates/float/slot/size/magic/ABI; no writes')

from safety_car_status import vehicle_snapshot
mem={}
def put(a,fmt,*v):
 for i,x in enumerate(struct.pack(fmt,*v)):mem[a+i]=x
base=0x100000
put(0x008AFEA0,'<d',127.0)
for i,x in enumerate(b'Jirko Malcharek\0'.ljust(32,b'\0')):mem[base+0x4620+i]=x
for off,val in ((0x32c8,1),(0x4064,0),(0x3450,0),(0x403c,0),(0x3d6c,0)):
 put(base+off,'<i',val)
for off,val in ((0x4410,0),(0x45f4,1)):put(base+off,'<B',val)
for off,val in ((0x4440,0),(0x4044,0),(0x4170,3.4028234663852886e38)):put(base+off,'<f',val)
for i in range(4):
 hit=0x200000+i*256;mat=0x300000+i*256
 put(base+0x3674+i*0xf8,'<I',hit);put(hit+0x54,'<I',mat);put(mat+0x30,'<III',7,22,27)
def rd(a,n):return bytes(mem[a+i] for i in range(n)) if all(a+i in mem for i in range(n)) else None
v=vehicle_snapshot(rd,base,20)
assert v['road_known']==4 and v['road_contacts']==4 and not v['restart_pending'] and v['lap_distance']==0
put(0x300000+0x30,'<III',1,2,3)
assert vehicle_snapshot(rd,base,20)['road_contacts']==3
put(base+0x3674,'<I',0xffffffff)
assert vehicle_snapshot(rd,base,20)['road_known']==3
put(base+0x4170,'<f',140)
assert vehicle_snapshot(rd,base,20)['restart_pending']
calls=0
def advancing(a,n):
 global calls
 if a==0x008AFEA0:
  calls+=1;return struct.pack('<d',float(calls))
 return rd(a,n)
try:vehicle_snapshot(advancing,base,20)
except ValueError:pass
else:raise AssertionError('accepted cross-tick observation')
print('PASS per-car diagnostics: known road/mixed tags/overflow/pending deadline/cross-tick rejection')
