"""Read-only red-flag detector — mirrors gr30.dll's check_auto_redflag().

Detects TERMINAL DAMAGE, not DNF. A car is terminally damaged the moment
its ActiveFlag (veh+0x45F4, float, FACT-tier) drops nonzero -> 0.0 = the
game's visual kill switch (car wrecked/stopped on track, ~4-5s after the
crash). DNF/FinishState comes much later (pit classification) and is the
wrong signal. Counts kill edges in an 8s rolling window; >=3 -> prints a big
ACTIVATE RED FLAG banner.

No writes. Safe to run alongside the game or the DLL.

Usage: python red_flag_watch.py [seconds]
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time

PROC_NAME = "F1 Challenge Online.exe"
PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400
VEH_BASE   = 0x5EC6D8
VEH_STRIDE = 0x48C0
NUM_SLOTS  = 22           # this grid (slot 0 = player)
LIVE_OFF   = 0x32C8       # int32 != -1  => slot live
OFF_ACTIVE = 0x45F4       # float ActiveFlag: ~1.0 running, 0.0 = killed/terminal
OFF_FINISH = 0x4064       # 0=run 1=fin 2=DNF 3=DSQ (late, informational only)

# detection tunables — match gr30.c
THRESHOLD  = 3
WINDOW_S   = 8.0
COOLDOWN_S = 120.0

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
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pid = None
    if k32.Process32First(snap, ctypes.byref(entry)):
        while True:
            if entry.szExeFile.decode(errors='replace').lower() == name.lower():
                pid = entry.th32ProcessID
                break
            if not k32.Process32Next(snap, ctypes.byref(entry)):
                break
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
        if not pid:
            sys.exit(f"'{PROC_NAME}' is not running")
        self.h = self.k32.OpenProcess(PROCESS_ALL, False, pid)
        if not self.h:
            sys.exit("OpenProcess failed (run PowerShell as admin?)")
    def read_raw(self, addr):
        buf = ctypes.create_string_buffer(4)
        got = ctypes.c_size_t()
        if not self.k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(got)):
            return None
        return buf.raw
    def read_i32(self, addr):
        r = self.read_raw(addr)
        return None if r is None else struct.unpack('<i', r)[0]
    def read_f32(self, addr):
        r = self.read_raw(addr)
        return None if r is None else struct.unpack('<f', r)[0]

def name_of(slot):
    return NAMES[slot] if slot < len(NAMES) else f"slot{slot}"

# terminal = ActiveFlag has dropped to ~0.0 (killed on track)
def is_terminal(g, base):
    act = g.read_f32(base + OFF_ACTIVE)
    if act is None:
        return False, act
    return (abs(act) < 0.5), act

def main():
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 300.0
    g = Game()

    # seed baseline (don't count cars already killed / not spawned)
    was_term = {}
    for slot in range(1, NUM_SLOTS):
        base = VEH_BASE + slot * VEH_STRIDE
        d, _ = is_terminal(g, base)
        was_term[slot] = d

    edges = []            # list of (timestamp, slot)
    cooldown_until = 0.0
    print(f"red-flag watch: {NUM_SLOTS-1} AI slots, need >={THRESHOLD} terminal-damage within {WINDOW_S:.0f}s. Ctrl+C to stop.")
    t0 = time.time()
    try:
        while time.time() - t0 < duration:
            now = time.time()
            for slot in range(1, NUM_SLOTS):
                base = VEH_BASE + slot * VEH_STRIDE
                d, act = is_terminal(g, base)
                if d and not was_term[slot]:
                    edges.append((now, slot))
                    fin = g.read_i32(base + OFF_FINISH)
                    ts = time.strftime('%H:%M:%S')
                    print(f"{ts}  TERMINAL  {name_of(slot):14s} (ActiveFlag->{act:.3f}, FinishState={fin})")
                was_term[slot] = d

            # prune + count within window
            edges = [(t, s) for (t, s) in edges if now - t <= WINDOW_S]
            recent = len(edges)

            if recent >= THRESHOLD and now >= cooldown_until:
                who = ", ".join(name_of(s) for (_, s) in edges)
                print("\n" + "=" * 60)
                print(f"  >>> ACTIVATE RED FLAG <<<  {recent} cars terminally damaged in {WINDOW_S:.0f}s")
                print(f"      involved: {who}")
                print("=" * 60 + "\n")
                cooldown_until = now + COOLDOWN_S
                edges = []      # reset like the DLL reseeds
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
