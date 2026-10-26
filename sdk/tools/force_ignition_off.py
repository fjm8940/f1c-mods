"""Poke ActiveFlag to 1 then try ignition toggle via vkey, watch RPM."""
import ctypes, struct, subprocess, sys, time

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
VEH_BASE = 0x5EC6D8
ACTIVE_FLAG = 0x45F4
RPM = 0x39A0
KEY_COUNT = 0x007CB218
KEY_LIST = 0x007CB21C
DIK_COMMA = 0x33

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

def rf(addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(n))
    return struct.unpack("<f", buf.raw)[0]

def ri(addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, 4, ctypes.byref(n))
    return struct.unpack("<i", buf.raw)[0]

def wr(addr, data):
    k32.WriteProcessMemory(h, ctypes.c_void_p(addr), data, len(data), ctypes.byref(n))

def inject_comma():
    wr(KEY_LIST, struct.pack("<h", DIK_COMMA))
    wr(KEY_COUNT, struct.pack("<i", 1))

print(f"ActiveFlag before: {ri(VEH_BASE + ACTIVE_FLAG)}")
print(f"EngineRPM before: {rf(VEH_BASE + RPM):.1f}")

# Reactivate car
print("Poking ActiveFlag = 1...")
wr(VEH_BASE + ACTIVE_FLAG, struct.pack("<i", 1))
time.sleep(0.2)
print(f"ActiveFlag after: {ri(VEH_BASE + ACTIVE_FLAG)}")

# Try toggling ignition via vkey (shared memory) AND inject_key simultaneously
# vkey first
try:
    import mmap
    h_map = k32.OpenFileMappingA(0xF001F, False, b"F1C_VKEYS")
    if h_map:
        k32.MapViewOfFile.restype = ctypes.c_void_p
        addr = k32.MapViewOfFile(h_map, 0xF001F, 0, 0, 256)
        if addr:
            buf = (ctypes.c_ubyte * 256).from_address(addr)
            # Press comma for 500ms
            buf[DIK_COMMA] = 1
            print("vkey: comma held (0x33=1)")
            time.sleep(0.5)
            buf[DIK_COMMA] = 0
            print("vkey: comma released")
            k32.UnmapViewOfFile(addr)
        k32.CloseHandle(h_map)
except Exception as e:
    print(f"vkey failed: {e}")

# Also inject via key queue
for i in range(10):
    inject_comma()
    time.sleep(0.05)

# Watch RPM
print("\nWatching RPM for 3s:")
for i in range(30):
    rpm = rf(VEH_BASE + RPM)
    print(f"  t+{i*0.1:.1f}s  RPM={rpm:.1f}")
    time.sleep(0.1)

k32.CloseHandle(h)
