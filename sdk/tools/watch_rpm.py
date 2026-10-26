"""Watch EngineRPM for N seconds."""
import ctypes, struct, subprocess, sys, time

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
RPM_OFF = 0x39A0
VEH_BASE = 0x5EC6D8

r = subprocess.run(
    ["tasklist", "/FI", f"IMAGENAME eq {PROC}", "/FO", "CSV", "/NH"],
    capture_output=True, text=True,
)
lines = [l for l in r.stdout.splitlines() if "F1 Challenge" in l]
if not lines:
    sys.exit("Game not running")
pid = int(lines[0].split(",")[1].strip().strip('"'))
h = k32.OpenProcess(0x1F0FFF, False, pid)
n = ctypes.c_size_t()
buf = ctypes.create_string_buffer(4)

dur = float(sys.argv[1]) if len(sys.argv) > 1 else 5.0
hz = float(sys.argv[2]) if len(sys.argv) > 2 else 10

period = 1.0 / hz
t0 = time.time()
while time.time() - t0 < dur:
    k32.ReadProcessMemory(h, ctypes.c_void_p(VEH_BASE + RPM_OFF), buf, 4, ctypes.byref(n))
    rpm = struct.unpack("<f", buf.raw)[0]
    print(f"t+{time.time()-t0:.1f}s  RPM={rpm:.1f}")
    time.sleep(period)

k32.CloseHandle(h)
