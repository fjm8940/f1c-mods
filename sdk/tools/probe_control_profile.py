"""Read the ignition control struct from the game's control profile."""
import ctypes, struct, subprocess, sys

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
ACTIVE_PROFILE = 0x5EA518
IGN_OFFSET = 0x3A30
STARTER_OFFSET = 0x3A4C
CTRL_STRIDE = 28

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

# Read ActiveControlProfilePtr
buf = ctypes.create_string_buffer(4)
k32.ReadProcessMemory(h, ctypes.c_void_p(ACTIVE_PROFILE), buf, 4, ctypes.byref(n))
profile_base = struct.unpack("<I", buf.raw)[0]
print(f"ActiveControlProfilePtr = 0x{profile_base:08X}")

# Read ignition control struct
for name, off in [("Ignition", IGN_OFFSET), ("Starter", STARTER_OFFSET)]:
    ctrl_buf = ctypes.create_string_buffer(CTRL_STRIDE)
    k32.ReadProcessMemory(h, ctypes.c_void_p(profile_base + off), ctrl_buf, CTRL_STRIDE, ctypes.byref(n))
    vals = struct.unpack("<7I", ctrl_buf.raw)
    print(f"\n{name} control at profile+0x{off:04X}:")
    print(f"  [0] +0x00 string_ptr  = 0x{vals[0]:08X}")
    print(f"  [1] +0x04 device       = {vals[1]}")
    print(f"  [2] +0x08 action_type? = {vals[2]}")
    print(f"  [3] +0x0C default_val  = {vals[3]}")
    print(f"  [4] +0x10 default_key  = {vals[4]}")
    print(f"  [5] +0x14 current_val  = {vals[5]}")
    print(f"  [6] +0x18 current_key  = {vals[6]}")

    # Try to read the string name
    if vals[0] and 0x400000 <= vals[0] <= 0xFFFFFF:
        try:
            strbuf = ctypes.create_string_buffer(32)
            k32.ReadProcessMemory(h, ctypes.c_void_p(vals[0]), strbuf, 32, ctypes.byref(n))
            s = strbuf.raw.split(b"\x00")[0].decode("latin-1", errors="replace")
            print(f"  name string: '{s}'")
        except:
            pass

k32.CloseHandle(h)
