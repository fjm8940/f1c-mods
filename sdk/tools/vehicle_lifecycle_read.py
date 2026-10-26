"""Internal bounded read-only native garage diagnostic; public CLI only."""
import ctypes,hashlib,math,struct,os
from pathlib import Path
from process_memory_snapshot import PROCESS_ACCESS,SnapshotError,find_process,main_module,read_memory

def observe(read,vehicle):
    def get(a,fmt):
        n=struct.calcsize(fmt)
        if a<=0 or a>0xffffffff-n:raise ValueError('address range')
        raw=read(a,n)
        if raw is None or len(raw)!=n:raise ValueError('unreadable')
        return struct.unpack(fmt,raw)
    result={'valid':False,'garage_valid':False,'garage_clear':False}
    try:
        role=get(vehicle+0x32c8,'<i')[0];finish=get(vehicle+0x4064,'<i')[0]
        retired=get(vehicle+0x4410,'<B')[0];ai=get(vehicle+0x45f4,'<B')[0]
        if role not in (-1,0,1,2) or finish not in (0,1,2,3) or retired>1 or ai>1:raise ValueError('invalid state')
        result.update(valid=True,role=role,finish=finish,retired=retired,ai_control=ai)
        if not(retired or finish in (2,3)) or ai:return result
        team=get(vehicle+0x48a4,'<i')[0];bay=get(vehicle+0x48a8,'<i')[0]
        body=get(vehicle+0x34e0,'<3f');manager=0x5ea2c0 # mov ecx, immediate in exact executable
        table=get(manager+0x158,'<I')[0];width=get(manager+0x164,'<I')[0]
        result.update(team=team,bay=bay,manager=manager,table=table,width=width)
        if not manager or not table or not(1<=width<=52 and 0<=team<width and 0<=bay<3):raise ValueError('invalid garage bounds')
        garage=get(table+28*(team+bay*width),'<3f')
        if not all(math.isfinite(v) and abs(v)<=1e7 for v in (*body,*garage)) or not any(garage):raise ValueError('invalid garage pose')
        distance=math.sqrt(sum((a-b)**2 for a,b in zip(body,garage)))
        result.update(garage_valid=True,team=team,bay=bay,body=body,garage=garage,garage_distance=distance,garage_clear=distance<=5)
    except ValueError as e:result['unavailable_reason']=str(e)
    return result

def read_live(process):
    if os.name!='nt':raise SnapshotError('Windows required')
    pid=find_process(process);module=main_module(pid)
    if module['base']!=0x400000 or hashlib.sha256(Path(module['path']).read_bytes()).hexdigest().upper()!='1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003':raise SnapshotError('Canonical target required')
    kernel=ctypes.windll.kernel32;handle=kernel.OpenProcess(PROCESS_ACCESS,False,pid)
    if not handle:raise SnapshotError('OpenProcess denied')
    try:
        rows={str(i):observe(lambda a,n:read_memory(handle,a,n),0x5ec6d8+i*0x48c0) for i in range(128)}
    finally:kernel.CloseHandle(handle)
    return {'read_only':True,'pid':pid,'values_by_slot':rows}
