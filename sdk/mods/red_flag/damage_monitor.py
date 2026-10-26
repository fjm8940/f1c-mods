"""Live DAMAGE monitor, drivers separated by damage tier. READ-ONLY.

Uses carConditions (veh+0x3D6C, FACT-grade severity, ai_wing_retire trials):
  0 = no physical damage      1 = wing/panel (light)      2 = wheel loss (heavy/terminal)
plus the hit counters (0x33F4/8/C) to catch CONTACT with no lasting damage.

Three tiers, updated live:
  HEAVY   carConditions == 2  (wheel loss / terminal)
  LIGHT   carConditions == 1  (wing / panel)
  CONTACT hit counter ticked recently but carConditions still 0 (clean bump)

Writes damage_board.txt each frame (for the PowerShell viewer) and prints too.

Usage: python -u damage_monitor.py
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, os

PROC_NAME = "F1 Challenge Online.exe"
PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400
VEH_BASE   = 0x5EC6D8
VEH_STRIDE = 0x48C0
NUM_SLOTS  = 22
LIVE_OFF   = 0x32C8

G_PHASE    = 0x7C56A8
OFF_CARCON = 0x3D6C        # float severity 0/1/2 (transient, spikes at impact)
OFF_REASON = 0x4068        # ReasonCode (6=Accident)
OFF_FINISH = 0x4064
OFF_RETIRE = 0x4410
OFF_SPEED  = 0x4440        # float speed
OFF_PITTING  = 0x3450      # pit-in-progress flag
OFF_PITSTATE = 0x4040      # PitState (2 = retired in pit)
OFF_CALLBOX  = 0x454C      # callToBox = 1 (go to pit)
OFF_HITA   = 0x33F4
OFF_HITB   = 0x33F8
OFF_HITC   = 0x33FC

OFF_POSX   = 0x744         # world position X (m)
OFF_POSZ   = 0x74C         # world position Z (m); Y=0x748 is height

RACING_PHASE = 5           # RacePhase when green/racing; anything else = grid/pre-race
STOP_SPEED   = 2.0         # |speed| below this = stopped (m/s)
MOVE_SPEED   = 10.0        # must exceed this once before a stop counts (grid guard)
STOPPED_SECS = 2.5         # must be stopped this long to call a trackside hazard
CONTACT_HOLD = 4.0
CLUSTER_RADIUS = 30.0      # m; cars within this of each other = same incident
PILEUP_MIN     = 3         # slow/stopped cars in a cluster to call a PILEUP
PILEUP_SPEED   = 15.0      # m/s; below this counts toward a pileup cluster (~54 km/h)
REFRESH      = 0.1
FRAME_FILE   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "damage_board.txt")

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

def main():
    g = Game()
    try:
        k32 = ctypes.windll.kernel32; h = k32.GetStdHandle(-11)
        m = wt.DWORD()
        if k32.GetConsoleMode(h, ctypes.byref(m)): k32.SetConsoleMode(h, m.value | 0x0004)
    except Exception: pass

    hit_prev = {}; last_hit_ts = {}; stopped_since = {}
    latched = {}          # slot -> (rank, tier) of worst BAD state seen this race
    peak_sev = {}         # slot -> worst carCon severity seen this race (persists)
    race_underway = False # True once ANY car has launched (grid-start guard)
    prev_racing = False
    for s in range(1, NUM_SLOTS):
        b = VEH_BASE + s * VEH_STRIDE
        hit_prev[s] = (g.i32(b+OFF_HITA), g.i32(b+OFF_HITB), g.i32(b+OFF_HITC))
        last_hit_ts[s] = 0.0; stopped_since[s] = None; latched[s] = None
        peak_sev[s] = 0

    try:
        while True:
            now = time.time()
            phase = g.i32(G_PHASE)
            racing = (phase == RACING_PHASE)
            # clear latches whenever not racing (grid/pre-race/restart) so a new
            # race starts with a clean board
            if not racing:
                race_underway = False
                for s in latched:
                    latched[s] = None
                    peak_sev[s] = 0
            prev_racing = racing
            roster = []
            n_dnf = n_pit = n_dmg = 0
            for s in range(1, NUM_SLOTS):
                b = VEH_BASE + s * VEH_STRIDE
                if g.i32(b+LIVE_OFF) == -1: continue
                car = g.f32(b+OFF_CARCON)
                rsn = g.i32(b+OFF_REASON)
                fin = g.i32(b+OFF_FINISH)
                ret = g.i32(b+OFF_RETIRE)
                spd = g.f32(b+OFF_SPEED)
                pitting  = g.i32(b+OFF_PITTING)
                pitstate = g.i32(b+OFF_PITSTATE)
                callbox  = g.i32(b+OFF_CALLBOX)
                ha, hb, hc = g.i32(b+OFF_HITA), g.i32(b+OFF_HITB), g.i32(b+OFF_HITC)

                if (ha, hb, hc) != hit_prev[s] and any(
                        n is not None and o is not None and n > o
                        for n, o in zip((ha, hb, hc), hit_prev[s])):
                    last_hit_ts[s] = now
                hit_prev[s] = (ha, hb, hc)

                # global grid-start guard: race is underway once anyone launches
                if spd is not None and abs(spd) >= MOVE_SPEED:
                    race_underway = True
                if spd is not None and abs(spd) < STOP_SPEED:
                    if stopped_since[s] is None: stopped_since[s] = now
                else:
                    stopped_since[s] = None
                stopped_dur = (now - stopped_since[s]) if stopped_since[s] is not None else 0.0

                in_pit = bool(pitting) or bool(callbox) or (pitstate not in (0, None))
                sev = 0
                if car is not None:
                    if car >= 1.5: sev = 2
                    elif car >= 0.5: sev = 1
                # latch worst carCon severity (carCon is transient; peak persists)
                if racing and race_underway and sev > peak_sev[s]:
                    peak_sev[s] = sev
                psev = peak_sev[s]
                recent_hit = (now - last_hit_ts[s] <= CONTACT_HOLD)

                stopped = racing and race_underway and stopped_dur >= STOPPED_SECS
                # classify; peak carCon makes wing/wheel damage persist
                if fin == 2 or ret:
                    tier, rank = "RETIRED", 5
                elif stopped and in_pit:
                    tier, rank = "PIT", 2
                elif stopped:
                    tier, rank = "HAZARD", 4       # stopped trackside, racing, not pitting
                elif psev >= 2:
                    tier, rank = "TERM", 3          # lost a wheel this race (heavy)
                elif psev >= 1:
                    tier, rank = "damaged", 1       # wing/panel damage this race
                elif recent_hit:
                    tier, rank = "contact", 1       # bump, no lasting damage (live only)
                else:
                    tier, rank = "ok", 0

                # latch HAZARD/RETIRED (rank>=4) so they stick until race reset
                if rank >= 4:
                    if latched[s] is None or rank > latched[s][0]:
                        latched[s] = (rank, tier)
                lr = latched[s]
                if lr is not None and lr[0] >= rank:
                    disp_rank, disp_tier = lr
                else:
                    disp_rank, disp_tier = rank, tier

                px = g.f32(b+OFF_POSX); pz = g.f32(b+OFF_POSZ)
                roster.append({'rank': disp_rank, 's': s, 'tier': disp_tier, 'spd': spd,
                               'car': car, 'rsn': rsn, 'sd': stopped_dur,
                               'px': px, 'pz': pz})

            # ---- proximity / pileup pass ----
            for r in roster:
                near = 0
                if r['px'] is not None:
                    for o in roster:
                        if o is r or o['px'] is None: continue
                        dx = r['px']-o['px']; dz = r['pz']-o['pz']
                        if dx*dx + dz*dz <= CLUSTER_RADIUS*CLUSTER_RADIUS:
                            near += 1
                r['near'] = near
                # slow neighbours (incl self) for pileup test
                r['slow_cluster'] = 0
                if r['px'] is not None and r['spd'] is not None and abs(r['spd']) < PILEUP_SPEED:
                    cnt = 1
                    for o in roster:
                        if o is r or o['px'] is None or o['spd'] is None: continue
                        if abs(o['spd']) >= PILEUP_SPEED: continue
                        dx = r['px']-o['px']; dz = r['pz']-o['pz']
                        if dx*dx + dz*dz <= CLUSTER_RADIUS*CLUSTER_RADIUS:
                            cnt += 1
                    r['slow_cluster'] = cnt

            # pileup only meaningful once the race is actually underway (not on grid)
            if racing and race_underway:
                pileup = sorted({r['s'] for r in roster if r.get('slow_cluster', 0) >= PILEUP_MIN})
            else:
                pileup = []
            n_haz = sum(1 for r in roster if r['rank'] >= 4)
            n_pit = sum(1 for r in roster if r['rank'] == 2)
            n_dmg = sum(1 for r in roster if r['rank'] in (1, 3))

            roster.sort(key=lambda r: (-r['rank'], r['s']))

            body = []
            body.append(f"=== DAMAGE / HAZARD MONITOR ===  RacePhase={phase}"
                        f"{'  (RACING)' if racing else '  (grid/pre-race)'}  {time.strftime('%H:%M:%S')}")
            body.append(f"  HAZARD/RETIRED:{n_haz}  PIT:{n_pit}  damaged/TERM:{n_dmg}   "
                        f"(hazards latch until race restart)")
            if pileup:
                shown = ", ".join(name_of(s) for s in pileup[:6])
                if len(pileup) > 6: shown += f" +{len(pileup)-6} more"
                pnames = f"{len(pileup)} cars: {shown}"
            else:
                pnames = "-"
            body.append(f"  >>> PILEUP: {pnames}"[:76])
            body.append("")
            body.append(f"  {'DRIVER':14s} {'STATE':8s} {'km/h':>7s} {'carCon':>7s} "
                        f"{'rsn':>4s} {'stop':>6s} {'near':>5s}")
            body.append("  " + "-"*58)
            for r in roster:
                sp = f"{r['spd']*3.6:.0f}" if r['spd'] is not None else "?"
                cc = f"{r['car']:.1f}" if r['car'] is not None else "?"
                mark = ">>" if r['rank'] >= 4 else ("* " if r['rank'] >= 1 else "  ")
                pu = "PILE" if r['s'] in pileup else ""
                body.append(f"{mark}{name_of(r['s']):14s} {r['tier']:8s} {sp:>7s} {cc:>7s} "
                            f"{(str(r['rsn']) if r['rsn'] is not None else '?'):>4s} "
                            f"{r['sd']:5.1f}s {r['near']:>4d} {pu}")
            body.append("")
            body.append("  Ctrl+C to stop.")
            text = "\n".join(body) + "\n"
            sys.stdout.write("\033[2J\033[H" + text); sys.stdout.flush()
            try:
                tmp = FRAME_FILE + ".tmp"
                with open(tmp, "w") as f: f.write(text)
                os.replace(tmp, FRAME_FILE)
            except Exception: pass
            time.sleep(REFRESH)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
