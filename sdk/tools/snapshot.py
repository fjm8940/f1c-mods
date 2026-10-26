"""
snapshot.py — Press SPACE (while game focused) to capture car state.
Output prints to console AND appends to snapshot_out.txt.
Pause game yourself with P after snapshot fires.
"""
import struct, ctypes, ctypes.wintypes, subprocess, math, time, sys, os

OUTFILE = os.path.join(os.path.dirname(__file__), "..", "snapshot_out.txt")

def find_pid():
    r = subprocess.run(
        ['tasklist', '/FI', 'IMAGENAME eq F1 Challenge Online.exe', '/FO', 'CSV', '/NH'],
        capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip().strip('"'))
    return None

pid = find_pid()
if not pid:
    print("ERROR: game not running"); sys.exit(1)

h = ctypes.windll.kernel32.OpenProcess(0x1F0FFF, False, pid)
if not h:
    print("ERROR: OpenProcess failed (admin?)"); sys.exit(1)

def rd4(addr):
    buf = ctypes.create_string_buffer(4)
    n = ctypes.c_size_t()
    ctypes.windll.kernel32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(n))
    return buf.raw

def rdf(addr): return struct.unpack('<f', rd4(addr))[0]
def rdi(addr): return struct.unpack('<i', rd4(addr))[0]
def rdu(addr): return struct.unpack('<I', rd4(addr))[0]
def rds(addr, sz=64):
    buf = ctypes.create_string_buffer(sz)
    n = ctypes.c_size_t()
    ctypes.windll.kernel32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, sz, ctypes.byref(n))
    return buf.raw.split(b'\x00')[0].decode('latin-1', errors='replace')


def snapshot(num):
    p = rdu(0x007C54E8)
    lines = []
    def pr(s):
        lines.append(s)

    pr(f"========== SNAPSHOT #{num} @ {time.strftime('%H:%M:%S')} ==========")
    pr(f"PlayerVehiclePtr = 0x{p:08x}")
    pr("")

    pr("[SESSION]")
    pr(f"  SessionType  = {rdi(0x007C56A4)}")
    pr(f"  RaceActive   = {rdi(0x007D0308)}")
    pr(f"  VehicleCount = {rdi(0x005C329C)}")
    pr("")

    pr("[IDENTITY]")
    pr(f"  Driver = {rds(p+0x4620)}")
    pr(f"  Car    = {rds(p+0x4678)}")
    pr("")

    pr("[RETIREMENT]")
    pr(f"  FinishState +0x4064 = {rdi(p+0x4064)}")
    pr(f"  ReasonCode  +0x4068 = {rdi(p+0x4068)}")
    pr("")

    pr("[RPM CANDIDATES]")
    for off, name in [(0x48dc,"A"), (0x4fb8,"B"), (0x48e4,"C"), (0x4fc0,"D"), (0x4480,"E"), (0x3984,"F")]:
        v = rdf(p + off)
        rpm = abs(v) * 60.0 / (2 * math.pi)
        pr(f"  +0x{off:04x} {name}: {v:9.1f} rad/s = {rpm:7.0f} RPM")
    pr("")

    pr("[SPEED CANDIDATES]")
    for off in [0xa0, 0xa4, 0xa8, 0x100, 0x104, 0x3984, 0x38bc]:
        v = rdf(p + off)
        kmh = v * 3.6
        if -500.0 < kmh < 500.0:
            pr(f"  +0x{off:04x}: {v:9.3f} m/s = {kmh:7.1f} km/h")
    pr("")

    pr("[GEAR CANDIDATES]")
    for off in range(0x4190, 0x41d0, 4):
        v = rdi(p + off)
        if -1 <= v <= 8:
            pr(f"  +0x{off:04x} = {v}")
    for off in [0x90, 0x94, 0x98, 0x9c, 0xa0]:
        v = rdi(p + off)
        if -1 <= v <= 8:
            pr(f"  +0x{off:04x} = {v}  (low range)")
    pr("")

    pr("[TIRE TEMPS (K / C)]")
    for off, name in [(0x35e8,"cold_A"), (0x36e0,"cold_B"), (0x37d8,"cold_C"), (0x38d0,"cold_D"),
                      (0x39a8,"hot_A"), (0x3a1c,"hot_B"), (0x3a20,"hot_C"), (0x3a38,"hot_D"),
                      (0x3454,"hi_A"), (0x34b8,"hi_B")]:
        v = rdf(p + off)
        pr(f"  +0x{off:04x} {name:7s}: {v:7.1f} K  ({v-273.15:6.1f} C)")
    pr("")

    pr("[POSITION]")
    pr(f"  X = {rdf(p+0x1c):.2f}  Y = {rdf(p+0x20):.2f}  Z = {rdf(p+0x24):.2f}")
    pr("=" * 50)
    pr("")

    text = "\n".join(lines)
    print(text)
    with open(OUTFILE, "a", encoding="utf-8") as f:
        f.write(text + "\n")

# ── main loop ───────────────────────────────────────────────────────────────
print(f"pid={pid}")
print("Press NUMPAD0 anywhere to snapshot.  Ctrl+C = quit.")
print(f"Output also saved to: {os.path.abspath(OUTFILE)}\n")

VK_NUMPAD0 = 0x60
prev_down = False
count = 0

try:
    while True:
        down = bool(ctypes.windll.user32.GetAsyncKeyState(VK_NUMPAD0) & 0x8000)
        if down and not prev_down:
            count += 1
            snapshot(count)
        prev_down = down
        time.sleep(0.03)
except KeyboardInterrupt:
    print("\nDone.")
    ctypes.windll.kernel32.CloseHandle(h)
