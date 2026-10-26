"""Accident-detection POC / finetuning tool. READ-ONLY.

Watches a set of candidate crash/damage fields on every AI slot and prints
each change as it happens, timestamped, so we can see empirically which
field(s) mark 'terminally damaged' during a real crash. Also prints global
RacePhase / GameState transitions (green-flag racing = RacePhase 2).

Workflow: run this, get the race green, deliberately crash 2-3 cars, then
read the log to pick the signal + threshold for the real detector.

Usage: python -u red_flag_diag.py [seconds]   (default 600)
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time

PROC_NAME = "F1 Challenge Online.exe"
PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400
VEH_BASE   = 0x5EC6D8
VEH_STRIDE = 0x48C0
NUM_SLOTS  = 22
LIVE_OFF   = 0x32C8

G_SESSION = 0x7C56A4
G_STATE   = 0x7C569C
G_PHASE   = 0x7C56A8
G_COUNT   = 0x5C329C

# candidate fields: offset -> (label, type)  type 'i'=int32, 'f'=float
FIELDS = {
    0x4064: ('FinishState',   'i'),
    0x4410: ('RetiredFlag',   'i'),
    0x4068: ('ReasonCode',    'i'),
    0x45F4: ('ActiveFlag',    'f'),
    0x3D6C: ('carConditions', 'f'),
    0x457C: ('AIPace',        'f'),
    0x453C: ('SpeedCap',      'f'),
    0x33F4: ('hitA',          'i'),
    0x33F8: ('hitB',          'i'),
    0x33FC: ('hitC',          'i'),
    0x456C: ('unk_456c',      'i'),
    0x4544: ('unk_4544',      'i'),
}

# print a change only if it moves at least this much (float noise gate)
FLOAT_EPS = 0.05

NAMES = [
    'Matias', 'M.Schumacher', 'Hakkinen', 'Coulthard', 'Barrichello',
    'R.Schumacher', 'TesterA', 'Raikkonen', 'Trulli', 'Frentzen',
    'Hill', 'Irvine', 'Montoya', 'Fisichella', 'Manning',
    'Heidfeld', 'Salo', 'Sarrazin', 'Alesi', 'De La Rosa', 'Webber', 'Alonso'
]

class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD),
                ('th32ProcessID', wt.DWORD),
                ('th32DefaultHeapID', ctypes.POINTER(wt.ULONG)),
                ('th32ModuleID', wt.DWORD), ('cntThreads', wt.DWORD),
                ('th32ParentProcessID', wt.DWORD), ('pcPriClassBase', wt.LONG),
                ('dwFlags', wt.DWORD), ('szExeFile', ctypes.c_char * 260)]

def find_pid(name=PROC_NAME):
    k32 = ctypes.windll.kernel32
    snap = k32.CreateToolhelp32Snapshot(0x2, 0)
    e = PROCESSENTRY32(); e.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pid = None
    if k32.Process32First(snap, ctypes.byref(e)):
        while True:
            if e.szExeFile.decode(errors='replace').lower() == name.lower():
                pid = e.th32ProcessID; break
            if not k32.Process32Next(snap, ctypes.byref(e)): break
    k32.CloseHandle(snap)
    return pid

class Game:
    def __init__(self):
        self.k32 = ctypes.windll.kernel32
        self.k32.OpenProcess.restype = wt.HANDLE
        self.k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
        self.k32.ReadProcessMemory.argtypes = [
            wt.HANDLE, wt.LPCVOID, wt.LPVOID, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t)]
        pid = find_pid()
        if not pid: sys.exit(f"'{PROC_NAME}' is not running")
        self.h = self.k32.OpenProcess(PROCESS_ALL, False, pid)
        if not self.h: sys.exit("OpenProcess failed (run PowerShell as admin?)")
    def raw(self, addr, n=4):
        buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
        if not self.k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            return None
        return buf.raw
    def i32(self, addr):
        r = self.raw(addr); return None if r is None else struct.unpack('<i', r)[0]
    def f32(self, addr):
        r = self.raw(addr); return None if r is None else struct.unpack('<f', r)[0]

def name_of(s): return NAMES[s] if s < len(NAMES) else f"slot{s}"

def read_field(g, base, off, typ):
    return g.f32(base + off) if typ == 'f' else g.i32(base + off)

def changed(typ, old, new):
    if old is None or new is None: return old != new
    if typ == 'f': return abs(new - old) >= FLOAT_EPS
    return new != old

def fmt(typ, v):
    if v is None: return 'None'
    return f'{v:.3f}' if typ == 'f' else str(v)

def main():
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 600.0
    g = Game()
    prev = {}
    for s in range(1, NUM_SLOTS):
        base = VEH_BASE + s * VEH_STRIDE
        for off, (lbl, typ) in FIELDS.items():
            prev[(s, off)] = read_field(g, base, off, typ)
    gp = (g.i32(G_SESSION), g.i32(G_STATE), g.i32(G_PHASE))
    print(f"diag: {NUM_SLOTS-1} AI slots x {len(FIELDS)} fields. "
          f"Session={gp[0]} State={gp[1]} Phase={gp[2]} (green race = Phase 2). Ctrl+C stop.")
    t0 = time.time()
    try:
        while time.time() - t0 < duration:
            ng = (g.i32(G_SESSION), g.i32(G_STATE), g.i32(G_PHASE))
            if ng != gp:
                ts = time.strftime('%H:%M:%S')
                print(f"{ts}  GLOBAL  Session={ng[0]} State={ng[1]} RacePhase={ng[2]}")
                gp = ng
            for s in range(1, NUM_SLOTS):
                base = VEH_BASE + s * VEH_STRIDE
                for off, (lbl, typ) in FIELDS.items():
                    v = read_field(g, base, off, typ)
                    o = prev[(s, off)]
                    if changed(typ, o, v):
                        ts = time.strftime('%H:%M:%S')
                        print(f"{ts}  {name_of(s):13s} +0x{off:04x} {lbl:14s} {fmt(typ,o)} -> {fmt(typ,v)}")
                        prev[(s, off)] = v
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
