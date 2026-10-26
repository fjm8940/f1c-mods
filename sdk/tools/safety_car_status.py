"""Internal read-only backend for f1c_mod.py safety-car-status."""
import ctypes
import os
import struct
import math
import hashlib
import json
from pathlib import Path

def vehicle_snapshot(read, vehicle, slot):
    """Reviewed SC lifecycle/surface fields; unavailable bytes are errors."""
    def raw(address,n):
        if not address or address>0xffffffff-n:raise ValueError('Invalid read range')
        data=read(address,n)
        if data is None or len(data)!=n:raise ValueError('Unreadable vehicle observation')
        return data
    def unpack(address,fmt):return struct.unpack(fmt,raw(address,struct.calcsize(fmt)))[0]
    before=unpack(0x008AFEA0,'<d')
    name=raw(vehicle+0x4620,32)
    result={'slot':slot,'name':name.split(b'\0')[0].decode('latin1'),'read_only':True}
    specs={'role':(0x32c8,'<i'),'finish':(0x4064,'<i'),'retired':(0x4410,'<B'),
           'ai_control':(0x45f4,'<B'),'pit_status':(0x3450,'<i'),
           'speed_ms':(0x4440,'<f'),'lap_distance':(0x4044,'<f'),
           'lap':(0x403c,'<i'),'damage':(0x3d6c,'<i'),'restart_deadline':(0x4170,'<f')}
    for key,(offset,fmt) in specs.items():result[key]=unpack(vehicle+offset,fmt)
    contacts=[]
    for wheel in range(4):
        item={'wheel':wheel,'known':False,'drivable':False}
        try:
            hit=unpack(vehicle+0x3674+wheel*0xf8,'<I')
            if not hit:raise ValueError('no wheel hit')
            material=unpack(hit+0x54,'<I')
            if not material:raise ValueError('no material')
            tags=struct.unpack('<III',raw(material+0x30,12))
            item.update(known=True,tags=tags,drivable=tags in ((6,21,27),(7,22,27),(11,21,27)))
        except ValueError as exc:item['unavailable']=str(exc)
        contacts.append(item)
    after=unpack(0x008AFEA0,'<d')
    if name!=raw(vehicle+0x4620,32):raise ValueError('Vehicle identity changed during read')
    if not math.isfinite(before) or before!=after:raise ValueError('Simulation advanced during read; retry')
    result.update(sim_time=before,contacts=contacts,
                  road_known=sum(c['known'] for c in contacts),road_contacts=sum(c['drivable'] for c in contacts))
    result['restart_pending']=math.isfinite(result['restart_deadline']) and 0<result['restart_deadline']<3.4028234663852886e38 and result['restart_deadline']>=before
    result['limitation']='Current fields only; private detector movement/handled history not exposed. Road contact does not prove racing-line overlap.'
    return result

def observe_vehicle(slot):
    if os.name!='nt' or not 0<=slot<104:raise ValueError('Windows and slot0..103 required')
    from process_memory_snapshot import PROCESS_ACCESS,find_process,main_module,read_memory,SnapshotError
    try:
        pid=find_process('F1 Challenge Online.exe');module=main_module(pid)
        if module['base']!=0x400000 or hashlib.sha256(Path(module['path']).read_bytes()).hexdigest().upper()!='1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003':
            raise ValueError('Unreviewed executable; vehicle read refused')
        layout=json.loads((Path(__file__).parent/'sdk/f1c_sdk.json').read_text(encoding='utf-8'))['vehicle_array']
        vehicle=int(layout['base'],0)+slot*int(layout['stride'],0)
        k=ctypes.WinDLL('kernel32',use_last_error=True)
        k.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];k.OpenProcess.restype=ctypes.c_void_p
        k.CloseHandle.argtypes=[ctypes.c_void_p]
        handle=k.OpenProcess(PROCESS_ACCESS,0,pid)
        if not handle:raise ValueError('Read-only OpenProcess denied: '+str(ctypes.get_last_error()))
        try:return vehicle_snapshot(lambda a,n:read_memory(handle,a,n),vehicle,slot)
        finally:k.CloseHandle(handle)
    except SnapshotError as exc:raise ValueError(str(exc)) from exc

FIELDS = {'ct_state':72,'sc_state':76,'engaged':88,'sc_ready':140,
          'leader_slot':100,'sc_slot':104,'sc_hidden':108,'live_cars':128,
          'heartbeat':144,'race_phase':120,'session_type':124,
          'trig_gates':188,'trig_zones':192,'trig_stopped':196,
          'trig_hold_ms':208,'trig_hold_req':212,'trig_worst_slot':220,
          'trig_worst_speed':224,'trig_field_speed':228,'trig_grace_left':244}
GATES = ['racing','auto_enabled','idle','not_final_lap','outside_grace',
         'field_moving','incident_corroborated','held_long_enough']

def decode(data):
    if len(data)!=444: raise ValueError('Safety Car status size mismatch')
    if struct.unpack_from('<I',data)[0]!=0x50414345: raise ValueError('Safety Car magic mismatch')
    if struct.unpack_from('<II',data,436)!=(1,444): raise ValueError('Safety Car ABI mismatch')
    result={name:struct.unpack_from('<f' if name in ('trig_worst_speed','trig_field_speed') else '<i',data,offset)[0] for name,offset in FIELDS.items()}
    result['gates']={name:bool(result['trig_gates']&(1<<i)) for i,name in enumerate(GATES)}
    result['read_only']=True
    result['limitation']='Published snapshot; compare heartbeat across samples for freshness. No inferred cause for missing incident corroboration.'
    return result

def observe():
    if os.name!='nt': raise ValueError('Requires Windows')
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.OpenFileMappingW.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_wchar_p];k.OpenFileMappingW.restype=ctypes.c_void_p
    k.MapViewOfFile.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_size_t];k.MapViewOfFile.restype=ctypes.c_void_p
    k.UnmapViewOfFile.argtypes=[ctypes.c_void_p];k.CloseHandle.argtypes=[ctypes.c_void_p]
    handle=k.OpenFileMappingW(4,0,'F1C_PACEPROBE_CTL') # FILE_MAP_READ only; never create
    if not handle: raise ValueError('Cannot open existing Safety Car status: '+str(ctypes.get_last_error()))
    view=None
    try:
        view=k.MapViewOfFile(handle,4,0,0,444)
        if not view: raise ValueError('Cannot read Safety Car status: '+str(ctypes.get_last_error()))
        for _ in range(3):
            before=ctypes.string_at(view,444)
            after=ctypes.string_at(view,444)
            if before==after:return decode(after)
        raise ValueError('Safety Car status changing; retry observation')
    finally:
        if view:k.UnmapViewOfFile(view)
        k.CloseHandle(handle)
