import ctypes
k32 = ctypes.windll.kernel32

# Find game PID
import subprocess, re
out = subprocess.check_output(['tasklist', '/FI', 'IMAGENAME eq F1 Challenge Online.exe'], text=True, errors='replace')
pid = None
for line in out.splitlines():
    if 'F1 Challenge' in line:
        m = re.search(r'\b(\d+)\b', line)
        if m:
            pid = int(m.group(1))
            break
if not pid:
    print("Game not running")
    exit(1)

h = k32.OpenProcess(0x0010, 0, pid)
b = (ctypes.c_int * 1)()

k32.ReadProcessMemory(h, ctypes.c_void_p(0x007D0308), b, 4, None)
print(f"RaceActiveFlag={b[0]}")

k32.ReadProcessMemory(h, ctypes.c_void_p(0x007D05A0), b, 4, None)
print(f"LapCount={b[0]}")

k32.ReadProcessMemory(h, ctypes.c_void_p(0x005C329C), b, 4, None)
count = b[0]
print(f"VehicleCount={count}")
print()

names = [
    'Matias', 'M.Schumacher', 'Hakkinen', 'Coulthard', 'Barrichello',
    'R.Schumacher', 'TesterA', 'Raikkonen', 'Trulli', 'Frentzen',
    'Hill', 'Irvine', 'Montoya', 'Fisichella', 'Manning',
    'Heidfeld', 'Salo', 'Sarrazin', 'Alesi', 'De La Rosa', 'Webber', 'Alonso'
]

positions = []
for i in range(min(count, 22)):
    vp = (ctypes.c_uint * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(0x007C54F0 + i*4), vp, 4, None)
    veh = vp[0]
    if veh == 0:
        continue

    nb = (ctypes.c_char * 40)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(veh + 0x4620), nb, 40, None)
    name = nb.value.decode('ascii', errors='replace').split('\0')[0].strip()

    p4060 = (ctypes.c_int * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(veh + 0x4060), p4060, 4, None)

    p06b4 = (ctypes.c_int * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(0x005F0DEC + i*0x48C0 + 0x06B4), p06b4, 4, None)

    fs = (ctypes.c_int * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(veh + 0x4064), fs, 4, None)

    lr = (ctypes.c_float * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(veh + 0x453C), lr, 4, None)

    # also try other potential position fields
    p4034 = (ctypes.c_int * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(veh + 0x4034), p4034, 4, None)

    # driver+0x06BC = plannedPositionAtStart
    p06bc = (ctypes.c_int * 1)()
    k32.ReadProcessMemory(h, ctypes.c_void_p(0x005F0DEC + i*0x48C0 + 0x06BC), p06bc, 4, None)

    positions.append((p4060[0], p06b4[0], p06bc[0], p4034[0], i, name, fs[0], lr[0]))

positions.sort(key=lambda x: x[0] if x[0] > 0 else 999)

fs_map = {0: 'RUN', 1: 'FIN', 2: 'DNF', 3: 'DSQ'}
print(f"{'LIVE':>3s} {'slot':>4s} {'4060':>5s} {'06B4':>5s} {'06BC':>5s} {'4034':>5s} {'FS':>4s} {'spd':>5s}  Driver")
print("-" * 80)
for rank, (p4060, p06b4, p06bc, p4034, slot, name, fs, lr) in enumerate(positions, 1):
    fs_s = fs_map.get(fs, str(fs))
    print(f"P{rank:2d}  [{slot:2d}]  {p4060:5d}  {p06b4:5d}  {p06bc:5d}  {p4034:5d}  {fs_s:4s}  {lr:4.1f}  {name}")

k32.CloseHandle(h)
