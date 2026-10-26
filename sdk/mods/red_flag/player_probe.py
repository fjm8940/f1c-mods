"""Player-car TRACK-POSITION probe. READ-ONLY. Run in a foreground console.

Uses the track-relative coordinate fields (CONFIRMED in the SDK):
  veh+0x744  trk_x  = LATERAL offset across the track (0 = centerline)
  veh+0x74C  trk_y  = LONGITUDINAL distance along the lap
so |trk_x| is your distance from the track centerline, and comparing it to
the track half-width tells you ON TRACK vs OFF TRACK.

The half-width isn't known until you calibrate it once per track:
  drive onto the white line / track edge and press E — that records the
  current |trk_x| as the edge. After that the probe shows ON/OFF and how far
  outside the track you are.

Keys (this window must have focus):
  E = set track edge at current lateral   R = reset calibration & min/max
  B = snapshot a scan baseline (do this ON track), then drive OFF track and
      watch the "TOP MOVERS" list — fields that change most between on/off
      track are the ON/OFF-track / surface candidates we're hunting.
  Q = quit

Usage: python -u player_probe.py
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, os
try:
    import msvcrt
except ImportError:
    msvcrt = None

PROC_NAME = "F1 Challenge Online.exe"
PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400
VEH_BASE   = 0x5EC6D8       # slot 0 = player
OFF_TRK_X  = 0x744          # lateral (across track)
OFF_TRK_Y  = 0x74C          # longitudinal (along lap)
OFF_SPEED  = 0x4440
OFF_CARCON = 0x3D6C
OFF_NODE   = 0x48A4
G_PHASE    = 0x7C56A8

DEFAULT_HALFWIDTH = 12.0    # metres, rough guess until you press E
REFRESH = 0.08

# regions of the veh struct to scan for a surface / on-off-track signal
SCAN_RANGES = [(0x0700, 0x0800), (0x3800, 0x3E00), (0x4400, 0x4600)]
SCAN_TOPN = 12

# candidate on/off-track fields to watch live (from A/B discovery)
WATCHLIST = [0x07fc, 0x38c0, 0x38d0, 0x4480, 0x4428, 0x3d6c]

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
        # 64-bit-safe handle types (default int restype truncates HANDLE)
        self.k32.OpenProcess.restype = wt.HANDLE
        self.k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
        self.k32.ReadProcessMemory.argtypes = [
            wt.HANDLE, wt.LPCVOID, wt.LPVOID, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t)]
        pid = find_pid()
        if not pid: sys.exit(f"'{PROC_NAME}' is not running")
        self.h = self.k32.OpenProcess(PROCESS_ALL, False, pid)
        if not self.h:
            sys.exit("OpenProcess failed (run PowerShell as admin?)")
    def raw(self, addr, n=4):
        buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
        if not self.k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            return None
        return buf.raw
    def i32(self, addr):
        r = self.raw(addr); return None if r is None else struct.unpack('<i', r)[0]
    def f32(self, addr):
        r = self.raw(addr); return None if r is None else struct.unpack('<f', r)[0]

def bar(val, lo, hi, width=41):
    # centered bar: 0 in the middle, val mapped across [lo,hi]
    if hi <= lo: return " " * width
    frac = (val - lo) / (hi - lo)
    frac = max(0.0, min(1.0, frac))
    pos = int(frac * (width - 1))
    cells = [' '] * width
    mid = width // 2
    cells[mid] = '|'          # centerline
    cells[pos] = 'O'          # car
    return "".join(cells)

def enable_vt():
    """Turn on ENABLE_VIRTUAL_TERMINAL_PROCESSING so \\033[..] clears work."""
    try:
        k32 = ctypes.windll.kernel32
        h = k32.GetStdHandle(-11)          # STD_OUTPUT_HANDLE
        mode = wt.DWORD()
        if k32.GetConsoleMode(h, ctypes.byref(mode)):
            k32.SetConsoleMode(h, mode.value | 0x0004)
    except Exception:
        pass

def main():
    g = Game()
    enable_vt()
    os.system("")
    halfwidth = DEFAULT_HALFWIDTH
    calibrated = False
    xmin = 1e9; xmax = -1e9
    scan_offs = [off for (a, b) in SCAN_RANGES for off in range(a, b, 4)]
    baseline = None
    frozen = None
    try:
        while True:
            if msvcrt and msvcrt.kbhit():
                k = msvcrt.getch().lower()
                if k == b'q': break
                elif k == b'e':
                    x = g.f32(VEH_BASE + OFF_TRK_X)
                    if x is not None and abs(x) > 0.1:
                        halfwidth = abs(x); calibrated = True
                elif k == b'r':
                    calibrated = False; halfwidth = DEFAULT_HALFWIDTH
                    xmin = 1e9; xmax = -1e9; baseline = None
                elif k == b'b':
                    baseline = {o: g.f32(VEH_BASE + o) for o in scan_offs}
                    frozen = None
                elif k == b'c':
                    if baseline is not None:
                        sample = {o: g.f32(VEH_BASE + o) for o in scan_offs}
                        movers = []
                        for o in scan_offs:
                            cur = sample.get(o); base = baseline.get(o)
                            if cur is None or base is None: continue
                            if o in (OFF_TRK_X, OFF_TRK_Y, 0x748): continue
                            d = abs(cur - base)
                            if d != d or d > 1e6 or d <= 1e-4: continue
                            movers.append((d, o, base, cur))
                        movers.sort(reverse=True)
                        frozen = movers

            x   = g.f32(VEH_BASE + OFF_TRK_X)
            y   = g.f32(VEH_BASE + OFF_TRK_Y)
            spd = g.f32(VEH_BASE + OFF_SPEED)
            cc  = g.f32(VEH_BASE + OFF_CARCON)
            nd  = g.i32(VEH_BASE + OFF_NODE)
            ph  = g.i32(G_PHASE)

            if x is not None:
                xmin = min(xmin, x); xmax = max(xmax, x)

            on_track = (x is not None and abs(x) <= halfwidth)
            outside  = 0.0 if x is None else max(0.0, abs(x) - halfwidth)
            status   = "ON TRACK " if on_track else "OFF TRACK"

            span = max(halfwidth * 1.6, 15.0)
            out = ["\033[2J\033[H"]
            out.append("=== PLAYER TRACK-POSITION PROBE ===   RacePhase=%s (green=2)" % ph)
            out.append("")
            def ff(v, w=8): return f"{v:{w}.2f}" if v is not None else f"{'?':>{w}}"
            out.append(f"  world X  trk_x = {ff(x)} m")
            out.append(f"  world Z  trk_y = {ff(y)} m")
            out.append(f"  speed          = {ff(spd)}")
            out.append(f"  carConditions  = {ff(cc)}     nodeIdx = {nd if nd is not None else '?'}")
            out.append("")
            out.append(f"  L{'-'*19}+{'-'*19}R")
            out.append("  " + (bar(x, -span, span) if x is not None else " "*41))
            out.append("")
            out.append("  WATCH (weave on/off track, see which tracks the surface):")
            wparts = []
            for wo in WATCHLIST:
                wv = g.f32(VEH_BASE + wo)
                wparts.append(f"0x{wo:04x}={ff(wv,9)}")
            out.append("    " + "  ".join(wparts[:3]))
            out.append("    " + "  ".join(wparts[3:]))
            out.append("")
            out.append(f"  track half-width = {halfwidth:.2f} m   "
                       + ("(CALIBRATED)" if calibrated else "(default guess - press E on the edge)"))
            out.append(f"  |trk_x| range seen: {xmin:.2f} .. {xmax:.2f}" if xmax > -1e8 else "")
            out.append("")
            out.append(f"  >>> {status} <<<   distance outside track = {outside:5.2f} m")
            out.append("")

            # discovery: FROZEN A/B capture (stationary method)
            if frozen is not None:
                out.append("  FROZEN A/B (baseline=B spot, sample=C spot):")
                if frozen:
                    for d, o, base, cur in frozen[:SCAN_TOPN]:
                        out.append(f"    +0x{o:04x}  {base:12.4f} -> {cur:12.4f}   (d={d:.4f})")
                else:
                    out.append("    (no change captured)")
                out.append("")
            # live top movers vs baseline
            if baseline is None:
                out.append("  SCAN: stop ON track, press B; roll onto grass, stop, press C.")
            else:
                movers = []
                for o in scan_offs:
                    cur = g.f32(VEH_BASE + o); base = baseline.get(o)
                    if cur is None or base is None: continue
                    # ignore world-position fields & wild/NaN values
                    if o in (OFF_TRK_X, OFF_TRK_Y, 0x748): continue
                    d = abs(cur - base)
                    if d != d or d > 1e6: continue
                    if d > 1e-4:
                        movers.append((d, o, base, cur))
                movers.sort(reverse=True)
                out.append("  TOP MOVERS vs baseline (surface/on-off candidates):")
                for d, o, base, cur in movers[:SCAN_TOPN]:
                    out.append(f"    +0x{o:04x}  {base:12.4f} -> {cur:12.4f}   (d={d:.4f})")
                if not movers:
                    out.append("    (nothing changed yet)")
            out.append("")
            out.append("  [B] baseline(on track)  [C] capture(off track)  [E] edge  [R] reset  [Q] quit")
            sys.stdout.write("\n".join(out) + "\n")
            sys.stdout.flush()
            time.sleep(REFRESH)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
