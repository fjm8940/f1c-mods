"""Live collision / terminal-damage board. READ-ONLY, refreshes in place.

Two live lists:
  IN COLLISION  - car is currently in a collision event (a hit counter just
                  ticked up within COLLISION_HOLD s, or carConditions > 0).
  TERMINAL      - car has a terminal-damage / retirement flag set
                  (FinishState==2, RetiredFlag!=0, or ActiveFlag fell to 0
                  from a previously-running value).

Also prints the raw candidate fields per live car so we can confirm which
signal actually fires during a real crash and finetune from there.

Usage: python -u red_flag_live.py
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, os

PROC_NAME = "F1 Challenge Online.exe"
PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400
VEH_BASE   = 0x5EC6D8
VEH_STRIDE = 0x48C0
NUM_SLOTS  = 22
LIVE_OFF   = 0x32C8       # int32 != -1 => slot live

G_SESSION = 0x7C56A4
G_STATE   = 0x7C569C
G_PHASE   = 0x7C56A8

OFF_FINISH = 0x4064
OFF_RETIRE = 0x4410
OFF_REASON = 0x4068
OFF_ACTIVE = 0x45F4       # float
OFF_CARCON = 0x3D6C       # float carConditions
OFF_HITA   = 0x33F4
OFF_HITB   = 0x33F8
OFF_HITC   = 0x33FC
OFF_SPEED  = 0x4440       # float VEHF_Speed (FACT)

COLLISION_HOLD   = 3.0    # s a car stays flagged "in collision" after a hit
COLLISION_MEMORY = 12.0   # s a stop can still be attributed to a crash
STOP_SPEED       = 3.0    # speed below this = "stopped" (tune from raw column)
STOPPED_SECS     = 2.5    # must be stopped this long to count as terminal
REFRESH          = 0.1

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

def fi(v): return "?" if v is None else str(v)
def ff(v): return "?" if v is None else f"{v:.2f}"

CLEAR = "\033[2J\033[H"
FRAME_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "live_board.txt")

def main():
    g = Game()
    os.system("")   # enable ANSI on Windows terminal
    hit_prev = {}
    last_hit_ts = {}
    active_prev = {}
    stopped_since = {}      # ts car first dropped below STOP_SPEED (None if moving)
    for s in range(1, NUM_SLOTS):
        b = VEH_BASE + s * VEH_STRIDE
        hit_prev[s]   = (g.i32(b+OFF_HITA), g.i32(b+OFF_HITB), g.i32(b+OFF_HITC))
        last_hit_ts[s] = 0.0
        active_prev[s] = g.f32(b+OFF_ACTIVE)
        stopped_since[s] = None

    try:
        while True:
            now = time.time()
            sess, state, phase = g.i32(G_SESSION), g.i32(G_STATE), g.i32(G_PHASE)
            rows = []
            in_coll, terminal = [], []
            for s in range(1, NUM_SLOTS):
                b = VEH_BASE + s * VEH_STRIDE
                if g.i32(b+LIVE_OFF) == -1:
                    continue
                fin = g.i32(b+OFF_FINISH)
                ret = g.i32(b+OFF_RETIRE)
                rsn = g.i32(b+OFF_REASON)
                act = g.f32(b+OFF_ACTIVE)
                car = g.f32(b+OFF_CARCON)
                ha, hb, hc = g.i32(b+OFF_HITA), g.i32(b+OFF_HITB), g.i32(b+OFF_HITC)
                spd = g.f32(b+OFF_SPEED)

                # collision: any hit counter went up
                if (ha, hb, hc) != hit_prev[s] and any(
                        n is not None and o is not None and n > o
                        for n, o in zip((ha, hb, hc), hit_prev[s])):
                    last_hit_ts[s] = now
                hit_prev[s] = (ha, hb, hc)

                # carConditions (0x3D6C, FACT): 0=none 1=wing/panel 2=wheel-loss(terminal)
                sev = 0
                if car is not None:
                    if car >= 1.5: sev = 2
                    elif car >= 0.5: sev = 1

                coll = (now - last_hit_ts[s] <= COLLISION_HOLD) or (sev >= 1)

                # track how long the car has been near-stopped
                if spd is not None and abs(spd) < STOP_SPEED:
                    if stopped_since[s] is None:
                        stopped_since[s] = now
                else:
                    stopped_since[s] = None
                stopped_dur = (now - stopped_since[s]) if stopped_since[s] is not None else 0.0

                # stopped long enough AND a crash happened recently = stranded wreck
                stopped_wreck = (stopped_dur >= STOPPED_SECS and
                                 (now - last_hit_ts[s]) <= COLLISION_MEMORY)

                # TERMINAL = heavy damage (carConditions==2 / wheel loss), the
                # game's own severity signal — plus definitive retire flags / wreck.
                term = (sev >= 2) or (fin == 2) or (ret != 0) or stopped_wreck
                active_prev[s] = act

                if term: terminal.append(name_of(s))
                elif coll: in_coll.append(name_of(s))

                mark = "TERM" if term else ("dmg " if sev >= 1 else ("COLL" if coll else "    "))
                rows.append(f"  [{mark}] {name_of(s):13s} sev={sev} fin={fi(fin)} ret={fi(ret)} "
                            f"rsn={fi(rsn)} carCon={ff(car,5)} spd={ff(spd)} "
                            f"stop={stopped_dur:4.1f}s hit={fi(ha)}/{fi(hb)}/{fi(hc)}")

            body = []
            body.append(f"Session={fi(sess)} State={fi(state)} RacePhase={fi(phase)} "
                        f"(green race = 2)   {time.strftime('%H:%M:%S')}")
            body.append("")
            body.append(f"IN COLLISION ({len(in_coll)}): " + (", ".join(in_coll) if in_coll else "-"))
            body.append(f"TERMINAL     ({len(terminal)}): " + (", ".join(terminal) if terminal else "-"))
            body.append("")
            body.extend(rows)
            body.append("")
            body.append("Ctrl+C to stop.")
            text = "\n".join(body) + "\n"

            # stdout (ANSI in-place) + a frame file the PS viewer tails
            sys.stdout.write(CLEAR + text)
            sys.stdout.flush()
            try:
                tmp = FRAME_FILE + ".tmp"
                with open(tmp, "w") as f:
                    f.write(text)
                os.replace(tmp, FRAME_FILE)
            except Exception:
                pass
            time.sleep(REFRESH)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
