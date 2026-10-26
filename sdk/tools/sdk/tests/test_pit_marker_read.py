"""Canonical gate for bounded XPITIN diagnostic reader."""
from pathlib import Path
import sys,struct
r=Path(__file__).resolve().parents[3];sys.path.insert(0,str(r/'tools'))
from runtime_validator import pit_marker_chain
memory={}
def put(a,fmt,*values):
 for i,b in enumerate(struct.pack(fmt,*values)):memory[a+i]=b
def read(a,n):
 try:return bytes(memory[a+i] for i in range(n))
 except KeyError:return None
assert pit_marker_chain(read)['status']=='missing_node'
put(0x8acee0+0x194,'<I',0x2000);put(0x2204,'<I',0x3000);put(0x3014,'<I',0)
put(0x30dc,'<3f',2,3,4);put(0x30e8,'<3f',0,0,0)
put(0x21a0,'<16f',0,0,-1,0,0,1,0,0,1,0,0,0,10,20,30,1)
backup=dict(memory);result=pit_marker_chain(read);assert result['status']=='uncached' and result['resolved_world']==[14,23,28];assert memory==backup
put(0x3014,'<I',0x20000000);put(0x30e8,'<3f',5,6,7);assert pit_marker_chain(read)['resolved_world']==[5,6,7]
print('PASS bounded XPITIN reader missing/uncached/cached diagnosis, rotation/translation, no writes')
