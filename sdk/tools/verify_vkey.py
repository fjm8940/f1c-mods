"""Watch spectated vehicle slot before/after vkey injection."""
import ctypes, struct, subprocess, sys, time

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
SPEC_SLOT = 0x7C54DC

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

slot = ri(SPEC_SLOT)
print(f"SpectatedVehicleSlot = {slot}  (0=player)")

# Read scancode buffer state from vkeys shared memory
try:
    import mmap
    h_map = k32.OpenFileMappingA(0xF001F, False, b"F1C_VKEYS")
    if h_map:
        k32.MapViewOfFile.restype = ctypes.c_void_p
        addr = k32.MapViewOfFile(h_map, 0xF001F, 0, 0, 256)
        if addr:
            buf = (ctypes.c_ubyte * 256).from_address(addr)
            comma_state = buf[0x33]
            print(f"vkey comma (0x33) state = {comma_state}")
            k32.UnmapViewOfFile(addr)
        k32.CloseHandle(h_map)
except Exception as e:
    print(f"vkey check failed: {e}")

k32.CloseHandle(h)
