"""Check game session state."""
import ctypes, struct, subprocess, sys

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
VEH_BASE = 0x5EC6D8
SESSION_TYPE = 0x7C56A4
RACE_PHASE = 0x7C56A8
PAUSE_FLAG = 0x7CE25F
RPM = 0x39A0
SPEED = 0x34D4

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

def ri(addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(n))
    return struct.unpack("<i", buf.raw)[0]

def rf(addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(n))
    return struct.unpack("<f", buf.raw)[0]

def rb(addr):
    buf = ctypes.create_string_buffer(1)
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 1, ctypes.byref(n))
    return buf.raw[0]

st = ri(SESSION_TYPE)
print(f"SessionType = {st}  (5=qual, 6=warmup, 7=race)")

rp = ri(RACE_PHASE)
phases = {0:"garage/monitor", 1:"recon", 2:"formation", 3:"countdown", 4:"green", 5:"checkered", 6:"ended"}
print(f"RacePhase = {rp}  ({phases.get(rp, 'unknown')})")

pf = rb(PAUSE_FLAG)
print(f"PauseFlag = {pf}  ({'PAUSED' if pf else 'running'})")

rpm = rf(VEH_BASE + RPM)
spd = rf(VEH_BASE + SPEED)
print(f"EngineRPM = {rpm:.1f}")
print(f"Speed = {spd:.1f}")

# Also check driverStatus
ds = ri(VEH_BASE + 0x4510)
print(f"driverStatus = {ds}  (0=follow, 5=garaged)")

# Check ActiveFlag
af = ri(VEH_BASE + 0x45F4)
print(f"ActiveFlag = {af}")

k32.CloseHandle(h)
