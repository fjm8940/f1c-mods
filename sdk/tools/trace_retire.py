"""trace_retire.py — live sequence tracer for retirement/pit events.

Polls every live car's key state fields and prints every change with a timestamp,
so a wing-loss -> drive-to-pit -> retire sequence is captured in exact order,
and you see which car and which field fires first.

Usage: python tools/trace_retire.py [seconds]   (default 90)
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time

PROC="F1 Challenge Online.exe"; VEH=0x5EC6D8; STR=0x48C0; NSLOT=104
FIELDS=[(0x4068,"ReasonCode",'i'),(0x454C,"callToBox",'i'),(0x3450,"pitting",'i'),
        (0x4064,"FinishState",'i'),(0x4410,"RetiredFlag",'b'),(0x45F4,"ActiveFlag",'b'),
        (0x4040,"PitState",'i'),(0x4058,"QualPos",'i'),(0x4060,"RacePos",'i')]
class PE(ctypes.Structure):
    _fields_=[('dwSize',wt.DWORD),('cntUsage',wt.DWORD),('th32ProcessID',wt.DWORD),
    ('th32DefaultHeapID',ctypes.POINTER(wt.ULONG)),('th32ModuleID',wt.DWORD),
    ('cntThreads',wt.DWORD),('th32ParentProcessID',wt.DWORD),('pcPriClassBase',wt.LONG),
    ('dwFlags',wt.DWORD),('szExeFile',ctypes.c_char*260)]
k=ctypes.windll.kernel32
snap=k.CreateToolhelp32Snapshot(2,0); e=PE(); e.dwSize=ctypes.sizeof(PE); pid=None
if k.Process32First(snap,ctypes.byref(e)):
    while True:
        if e.szExeFile.decode(errors='replace').lower()==PROC.lower(): pid=e.th32ProcessID; break
        if not k.Process32Next(snap,ctypes.byref(e)): break
k.CloseHandle(snap)
if not pid: sys.exit("not running")
h=k.OpenProcess(0x38|0x400,False,pid)
def rd(a,n):
    b=ctypes.create_string_buffer(n); g=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,ctypes.c_void_p(a),b,n,ctypes.byref(g)): return None
    return b.raw
def rv(a,t):
    d=rd(a,4)
    if d is None: return None
    if t=='b': return d[0]
    return struct.unpack('<i',d)[0]
def name(slot):
    d=rd(VEH+slot*STR+0x46F8,16)
    return d.split(b'\x00')[0].decode('latin-1','replace') if d else "?"

live=[s for s in range(NSLOT) if (v:=rv(VEH+s*STR+0x32C8,'i')) is not None and v!=-1]
print(f"tracing {len(live)} cars, fields: {[f[1] for f in FIELDS]}")
prev={(s,o):rv(VEH+s*STR+o,t) for s in live for o,_,t in FIELDS}
secs=float(sys.argv[1]) if len(sys.argv)>1 else 90
t0=time.time()
while time.time()-t0<secs:
    for s in live:
        base=VEH+s*STR
        for o,nm,t in FIELDS:
            v=rv(base+o,t)
            if v is not None and v!=prev[(s,o)]:
                print(f"[{time.time()-t0:6.2f}s] slot {s:>2} {name(s):12s} {nm:12s} {prev[(s,o)]} -> {v}")
                prev[(s,o)]=v
    time.sleep(0.05)
print("done")
