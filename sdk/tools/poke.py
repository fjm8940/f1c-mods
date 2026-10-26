"""
poke.py — named runtime memory console for F1 Challenge Online.exe.

Resolves NAMES from mods/sdk/f1c_sdk.json (run tools/gen_sdk.py first) so you
never hand-type raw addresses. Exe is non-ASLR: absolute addresses are live.

USAGE
-----
  python tools/poke.py list tire                 # search known globals
  python tools/poke.py read SessionType          # read by name
  python tools/poke.py read 0x007c56a4:int32     # read by addr:type
  python tools/poke.py read 0x05ecd2f8:str       # NUL-terminated ASCII (driver name)
  python tools/poke.py write TireWearMultiplier 3.0
  python tools/poke.py watch RacePhase SessionType --hz 4
  python tools/poke.py veh TireWear              # player-vehicle struct field
  python tools/poke.py veh 0x8cc:float           # player-vehicle raw offset
  python tools/poke.py snapshot before.json      # capture ALL known globals
  python tools/poke.py diff before.json          # what changed since capture
  python tools/poke.py diff before.json --thresh 0.001

The observe-verify loop for mods:
  snapshot -> do the thing in-game -> diff  => the globals that moved.
  watch    => live confirmation a patch/hook fires.

Writes go straight into the running game — save replays/setups first.
"""

import ctypes
import ctypes.wintypes as wt
import json
import os
import re
import struct
import sys
import time

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PROJ_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SDK_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sdk", "f1c_sdk.json")
PROC_NAME = "F1 Challenge Online.exe"

PROCESS_ALL = 0x0010 | 0x0020 | 0x0008 | 0x0400  # VM_READ|VM_WRITE|VM_OPERATION|QUERY

TYPES = {
    'float':  ('<f', 4), 'double': ('<d', 8),
    'int32':  ('<i', 4), 'uint32': ('<I', 4), 'int': ('<i', 4),
    'int16':  ('<h', 2), 'uint16': ('<H', 2),
    'int8':   ('<b', 1), 'uint8':  ('<B', 1), 'byte': ('<B', 1),
    'ptr':    ('<I', 4),
    'str':    (None, 64), 'cstr': (None, 64),   # NUL-terminated ASCII, read-only
}


def sdk_type_to_poke(t):
    t = (t or 'int32').replace('_t', '').rstrip('*')
    if t == 'void':
        return 'ptr'
    return t if t in TYPES else 'int32'


# ---------------------------------------------------------------------------
# process attach
# ---------------------------------------------------------------------------
k32 = ctypes.windll.kernel32


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD),
                ('th32ProcessID', wt.DWORD),
                ('th32DefaultHeapID', ctypes.POINTER(wt.ULONG)),
                ('th32ModuleID', wt.DWORD), ('cntThreads', wt.DWORD),
                ('th32ParentProcessID', wt.DWORD), ('pcPriClassBase', wt.LONG),
                ('dwFlags', wt.DWORD), ('szExeFile', ctypes.c_char * 260)]


def find_pid(name=PROC_NAME):
    snap = k32.CreateToolhelp32Snapshot(0x2, 0)  # TH32CS_SNAPPROCESS
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pid = None
    if k32.Process32First(snap, ctypes.byref(entry)):
        while True:
            if entry.szExeFile.decode(errors='replace').lower() == name.lower():
                pid = entry.th32ProcessID
                break
            if not k32.Process32Next(snap, ctypes.byref(entry)):
                break
    k32.CloseHandle(snap)
    return pid


def find_pids(name=PROC_NAME):
    """All running game processes (multi-instance tools pick one by pid)."""
    snap = k32.CreateToolhelp32Snapshot(0x2, 0)
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pids = []
    if k32.Process32First(snap, ctypes.byref(entry)):
        while True:
            if entry.szExeFile.decode(errors='replace').lower() == name.lower():
                pids.append(entry.th32ProcessID)
            if not k32.Process32Next(snap, ctypes.byref(entry)):
                break
    k32.CloseHandle(snap)
    return pids


class Game:
    def __init__(self, pid=None):
        pid = pid or find_pid()
        if not pid:
            sys.exit(f"'{PROC_NAME}' is not running — start the game first")
        self.h = k32.OpenProcess(PROCESS_ALL, False, pid)
        if not self.h:
            sys.exit(f"OpenProcess failed (run as admin?) — pid {pid}")
        self.pid = pid

    def read(self, addr, size):
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf,
                                     size, ctypes.byref(got)):
            return None
        return buf.raw[:got.value]

    def write(self, addr, data):
        n = ctypes.c_size_t()
        ok = k32.WriteProcessMemory(self.h, ctypes.c_void_p(addr), data,
                                    len(data), ctypes.byref(n))
        return bool(ok) and n.value == len(data)

    def read_val(self, addr, ptype):
        # `str` / `cstr`: NUL-terminated ASCII. Needed for the vehicle struct's
        # driver name at +0x4620, which is what makes a live slot
        # self-identifying (see CLAUDE.md).
        if ptype in ('str', 'cstr'):
            raw = self.read(addr, 64)
            if raw is None:
                return None
            return raw.split(b'\0', 1)[0].decode('ascii', 'replace')
        fmt, size = TYPES[ptype]
        raw = self.read(addr, size)
        return None if raw is None or len(raw) < size else struct.unpack(fmt, raw)[0]

    def write_val(self, addr, ptype, value):
        # Decide int-vs-float from the TYPE NAME, not from the struct format
        # string: 'double' packs as '<d', which contains no 'f', so the old
        # `'f' not in fmt` test sent every double down the integer path and
        # `write <addr>:double 1.5` died on int('1.5'). That silently looked
        # like a poke that did nothing (2026-07-31, findings.jsonl).
        fmt, _ = TYPES[ptype]
        is_float = ptype in ('float', 'double')
        if isinstance(value, str):
            v = float(value) if is_float else int(value, 0)
        else:
            v = float(value) if is_float else int(value)
        return self.write(addr, struct.pack(fmt, v))


# ---------------------------------------------------------------------------
# name resolution
# ---------------------------------------------------------------------------
def load_sdk():
    try:
        with open(SDK_JSON, encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        sys.exit("mods/sdk/f1c_sdk.json missing — run: python tools/gen_sdk.py")


def resolve(sdk, token):
    """'SessionType' | 'DAT_007c56a4' | '0x7c56a4[:type]' -> (addr, ptype, label)"""
    tok, _, forced = token.partition(':')
    m = re.match(r'(?:dat_|0x)?([0-9a-fA-F]{6,8})$', tok, re.I)
    if m and (tok.lower().startswith(('0x', 'dat_')) or not tok.isalpha()):
        addr = int(m.group(1), 16)
        # reverse lookup for label/type
        for n, g in sdk['globals'].items():
            if g['addr'] == addr:
                return addr, forced or sdk_type_to_poke(g['type']), n
        return addr, forced or 'int32', f'0x{addr:08x}'
    gl = sdk['globals']
    if tok in gl:
        g = gl[tok]
        return g['addr'], forced or sdk_type_to_poke(g['type']), tok
    # case-insensitive exact, then substring
    low = tok.lower()
    exact = [n for n in gl if n.lower() == low]
    subs = [n for n in gl if low in n.lower()]
    if exact:
        g = gl[exact[0]]
        return g['addr'], forced or sdk_type_to_poke(g['type']), exact[0]
    if len(subs) == 1:
        g = gl[subs[0]]
        return g['addr'], forced or sdk_type_to_poke(g['type']), subs[0]
    if subs:
        print(f"ambiguous '{tok}' — matches:")
        for n in subs[:15]:
            print(f"  {n}  (0x{gl[n]['addr']:08x})")
        sys.exit(1)
    sys.exit(f"unknown global '{tok}' — try: poke.py list {tok}")


# ---------------------------------------------------------------------------
# commands
# ---------------------------------------------------------------------------
def cmd_list(sdk, term):
    t = term.lower()
    hits = [(n, g) for n, g in sdk['globals'].items()
            if t in n.lower() or t in (g.get('desc') or '').lower()]
    for n, g in sorted(hits)[:40]:
        print(f"  {n:44s} 0x{g['addr']:08x} {sdk_type_to_poke(g['type']):7s} {g.get('desc','')[:40]}")
    if len(hits) > 40:
        print(f"  ... {len(hits)-40} more")
    if not hits:
        print("  no matches")


def fmt_val(v, ptype):
    if v is None:
        return "<unreadable>"
    if ptype in ('str', 'cstr'):
        return repr(v)
    if ptype in ('float', 'double'):
        return f"{v:.6g}"
    return f"{v} (0x{v & 0xFFFFFFFF:x})"


def cmd_read(sdk, game, tokens):
    for token in tokens:
        addr, ptype, label = resolve(sdk, token)
        v = game.read_val(addr, ptype)
        print(f"  {label:44s} 0x{addr:08x} {ptype:7s} = {fmt_val(v, ptype)}")


def cmd_write(sdk, game, token, value):
    addr, ptype, label = resolve(sdk, token)
    before = game.read_val(addr, ptype)
    if not game.write_val(addr, ptype, value):
        sys.exit(f"write failed at 0x{addr:08x}")
    after = game.read_val(addr, ptype)
    print(f"  {label} 0x{addr:08x}: {fmt_val(before, ptype)} -> {fmt_val(after, ptype)}")


def cmd_watch(sdk, game, tokens, hz):
    targets = [resolve(sdk, t) for t in tokens]
    last = {}
    print(f"watching {len(targets)} globals at {hz} Hz — Ctrl+C to stop")
    try:
        while True:
            ts = time.strftime('%H:%M:%S')
            for addr, ptype, label in targets:
                v = game.read_val(addr, ptype)
                if last.get(addr, object()) != v:
                    print(f"  {ts}  {label:40s} = {fmt_val(v, ptype)}")
                    last[addr] = v
            time.sleep(1.0 / hz)
    except KeyboardInterrupt:
        pass


def cmd_veh(sdk, game, token):
    """Read a field of the PLAYER vehicle struct: by field-name search or raw offset."""
    pv = sdk['globals'].get('PlayerVehiclePtr')
    if not pv:
        sys.exit("PlayerVehiclePtr not in SDK globals")
    base = game.read_val(pv['addr'], 'ptr')
    if not base:
        sys.exit("player vehicle pointer is NULL (not in session?)")
    print(f"  player vehicle @ 0x{base:08x}")
    tok, _, forced = token.partition(':')
    veh = sdk['structs'].get('VehicleStruct', {})
    if re.match(r'(?:0x)?[0-9a-fA-F]+$', tok):
        off = int(tok, 16)
        ptype = forced or 'float'
        v = game.read_val(base + off, ptype)
        print(f"  +0x{off:04x} {ptype:7s} = {fmt_val(v, ptype)}")
        return
    t = tok.lower()
    hits = [(off, c) for off, cands in veh.items() for c in cands
            if t in c['name'].lower()]
    if not hits:
        sys.exit(f"no VehicleStruct field matching '{tok}'")
    for off, c in hits[:20]:
        ptype = forced or sdk_type_to_poke(c['type'])
        v = game.read_val(base + int(off, 16), ptype)
        print(f"  {off} {c['name']:36s} {ptype:7s} = {fmt_val(v, ptype)}")


def _snapshot_all(sdk, game):
    out = {}
    for n, g in sdk['globals'].items():
        ptype = sdk_type_to_poke(g['type'])
        v = game.read_val(g['addr'], ptype)
        if v is not None:
            out[n] = [g['addr'], ptype, v]
    return out


def cmd_snapshot(sdk, game, path):
    t0 = time.time()
    snap = _snapshot_all(sdk, game)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'time': time.strftime('%Y-%m-%d %H:%M:%S'), 'values': snap}, f)
    print(f"  captured {len(snap)} globals in {time.time()-t0:.1f}s -> {path}")


def cmd_diff(sdk, game, path, thresh):
    with open(path, encoding='utf-8') as f:
        old = json.load(f)['values']
    live = _snapshot_all(sdk, game)
    changed = []
    for n, (addr, ptype, v0) in old.items():
        cur = live.get(n)
        if cur is None:
            continue
        v1 = cur[2]
        if ptype in ('float', 'double'):
            if v0 != v1 and (abs(v1 - v0) > thresh * max(1.0, abs(v0))):
                changed.append((n, addr, ptype, v0, v1))
        elif v0 != v1:
            changed.append((n, addr, ptype, v0, v1))
    changed.sort(key=lambda r: r[0])
    print(f"  {len(changed)} of {len(old)} globals changed since {path}:")
    for n, addr, ptype, v0, v1 in changed[:80]:
        print(f"  {n:44s} 0x{addr:08x} {fmt_val(v0, ptype)} -> {fmt_val(v1, ptype)}")
    if len(changed) > 80:
        print(f"  ... {len(changed)-80} more")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd, args = sys.argv[1], sys.argv[2:]
    sdk = load_sdk()

    if cmd == 'list':
        cmd_list(sdk, ' '.join(args) if args else '')
        return

    game = Game()
    if cmd == 'read':
        cmd_read(sdk, game, args)
    elif cmd == 'write':
        cmd_write(sdk, game, args[0], args[1])
    elif cmd == 'watch':
        hz = 2.0
        toks = []
        it = iter(args)
        for a in it:
            if a == '--hz':
                hz = float(next(it))
            else:
                toks.append(a)
        cmd_watch(sdk, game, toks, hz)
    elif cmd == 'veh':
        cmd_veh(sdk, game, args[0])
    elif cmd == 'snapshot':
        cmd_snapshot(sdk, game, args[0] if args else 'snapshot.json')
    elif cmd == 'diff':
        thresh = 0.0
        if '--thresh' in args:
            i = args.index('--thresh')
            thresh = float(args[i + 1])
            args = args[:i] + args[i + 2:]
        cmd_diff(sdk, game, args[0], thresh)
    else:
        print(f"unknown command '{cmd}'")
        print(__doc__)


if __name__ == '__main__':
    main()
