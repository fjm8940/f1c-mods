"""
inject_key.py -- push a scancode into the game's OWN pending-key queue.

The game collects each frame's pressed keys into word_7CB21C[] with the count in
dword_7CB218, then several handlers (KeyboardInputActionMapper_0040be70,
sub_42C440, sub_4D8450, ...) switch on those scancodes. Writing the queue is the
game's own command path -- no OS input simulation.

Usage:
    python tools/inject_key.py <scancode> [--repeat N] [--watch-veh 0x39a0]

Scancodes are DirectInput / set-1 (',' = 51 / 0x33).
"""

import ctypes
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32
COUNT = 0x007CB218
LIST = 0x007CB21C
VEH_BASE = 0x005EC6D8
PROC = "F1 Challenge Online.exe"


def find_pid():
    r = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {PROC}', '/FO', 'CSV', '/NH'],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip().strip('"'))
    return None


def main():
    sc = int(sys.argv[1], 0)
    rep = 30
    if "--repeat" in sys.argv:
        rep = int(sys.argv[sys.argv.index("--repeat") + 1])
    woff = 0x39A0
    if "--watch-veh" in sys.argv:
        woff = int(sys.argv[sys.argv.index("--watch-veh") + 1], 0)

    pid = find_pid()
    h = k32.OpenProcess(0x1F0FFF, False, pid)
    n = ctypes.c_size_t()

    def wr(addr, data):
        k32.WriteProcessMemory(h, ctypes.c_void_p(addr), data, len(data), ctypes.byref(n))

    def rdf(addr):
        b = ctypes.create_string_buffer(4)
        k32.ReadProcessMemory(h, ctypes.c_void_p(addr), b, 4, ctypes.byref(n))
        return struct.unpack('<f', b.raw)[0]

    print(f"pid {pid}  injecting scancode {sc} x{rep}, watching veh+0x{woff:04X}")
    print(f"  t=0.00  {rdf(VEH_BASE + woff):.3f}")
    t0 = time.time()
    for i in range(rep):
        wr(LIST, struct.pack('<h', sc))
        wr(COUNT, struct.pack('<i', 1))
        time.sleep(0.05)
        if i % 4 == 0:
            print(f"  t={time.time()-t0:.2f}  {rdf(VEH_BASE + woff):.3f}")
    time.sleep(1.0)
    print(f"  final   {rdf(VEH_BASE + woff):.3f}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    main()
