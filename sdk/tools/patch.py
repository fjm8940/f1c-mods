"""
patch.py — byte-patch manager for F1 Challenge Online.exe.

Replaces the "full exe copy per mod" pattern with a manifest: every patch
records its VA, ORIGINAL bytes, and new bytes, so patches verify before
touching anything, revert cleanly, stack, and collide loudly instead of
silently.

MANIFEST resolution order (first hit wins; `status` prints which one it used):
  1. $F1C_PATCH_MANIFEST                        (explicit override)
  2. mods/extended_vanilla_features/patches/manifest.json
  3. mods/patches/manifest.json                 (corpus-wide default)
A new manifest is created at #2 if none exists, because that is where the only
current patch set (`blister-tolerance`) lives. A mod that wants its own
isolated manifest should set $F1C_PATCH_MANIFEST rather than assume #3.

USAGE
-----
  # define a patch (reads + records original bytes from the exe at that VA)
  python tools/patch.py add cut-track-dq 0x004bc4e0 "e8 fb fb ff ff" --desc "redirect penalty->DQ"
  # one mod = many byte sites: just `add` again with the same name

  python tools/patch.py status                  # every patch: applied / clean / FOREIGN
  python tools/patch.py apply cut-track-dq      # verify originals, write new bytes
  python tools/patch.py revert cut-track-dq     # verify patched, restore originals
  python tools/patch.py apply cut-track-dq --out "F1C (dq).exe"   # patched COPY, original untouched
  python tools/patch.py show cut-track-dq       # sites + current exe state

Safety:
  - apply refuses if bytes at VA are neither original nor already-new (FOREIGN
    = another mod or wrong exe version touched them).
  - overlapping VAs across patches are reported by status.
  - in-place apply creates <exe>.prepatch.bak once (first ever apply).

VA -> file offset via PE section headers (no external deps).
"""

import json
import os
import re
import shutil
import struct
import sys

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PROJ_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_EXE = os.path.join(PROJ_ROOT, "..", "F1 Challenge V10",
                           "F1 Challenge Online.exe")

_EVF_MANIFEST = os.path.join(
    PROJ_ROOT, "mods", "extended_vanilla_features", "patches", "manifest.json")
_ROOT_MANIFEST = os.path.join(PROJ_ROOT, "mods", "patches", "manifest.json")


def resolve_manifest():
    """First existing of: $F1C_PATCH_MANIFEST, the EVF manifest, the corpus one.

    Falls back to the EVF path for creation, since that is where the only
    current patch set lives. Kept explicit so a stale hardcoded path can never
    again silently send one mod's patches into another mod's folder.
    """
    env = os.environ.get("F1C_PATCH_MANIFEST")
    if env:
        return os.path.abspath(env)
    for cand in (_EVF_MANIFEST, _ROOT_MANIFEST):
        if os.path.exists(cand):
            return cand
    return _EVF_MANIFEST


MANIFEST = resolve_manifest()


# ---------------------------------------------------------------------------
# PE mapping
# ---------------------------------------------------------------------------
def pe_sections(data):
    """[(va_start, va_end, raw_off)] from PE headers."""
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    nsec = struct.unpack_from('<H', data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from('<H', data, e_lfanew + 20)[0]
    image_base = struct.unpack_from('<I', data, e_lfanew + 24 + 28)[0]
    sec0 = e_lfanew + 24 + opt_size
    secs = []
    for i in range(nsec):
        off = sec0 + i * 40
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, off + 8)
        secs.append((image_base + va, image_base + va + max(vsize, rsize), raw))
    return secs


def va_to_file(data, va):
    for start, end, raw in pe_sections(data):
        if start <= va < end:
            return raw + (va - start)
    raise ValueError(f"VA 0x{va:08x} not inside any PE section")


# ---------------------------------------------------------------------------
# manifest
# ---------------------------------------------------------------------------
def load_manifest():
    try:
        with open(MANIFEST, encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        return {'exe': DEFAULT_EXE, 'patches': {}}


def save_manifest(m):
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    with open(MANIFEST, 'w', encoding='utf-8') as f:
        json.dump(m, f, indent=1)


def parse_hex(s):
    h = re.sub(r'[\s,]+', '', s).lower().replace('0x', '')
    if len(h) % 2 or not re.match(r'^[0-9a-f]*$', h):
        sys.exit(f"bad hex byte string: {s!r}")
    return bytes.fromhex(h)


def site_state(data, site):
    off = va_to_file(data, site['va'])
    cur = data[off:off + len(bytes.fromhex(site['new']))]
    if cur == bytes.fromhex(site['new']):
        return 'applied'
    if cur == bytes.fromhex(site['orig']):
        return 'clean'
    return 'FOREIGN'


# ---------------------------------------------------------------------------
# commands
# ---------------------------------------------------------------------------
def cmd_add(m, name, va_s, new_s, desc, exe):
    va = int(va_s, 16)
    new = parse_hex(new_s)
    with open(exe, 'rb') as f:
        data = f.read()
    off = va_to_file(data, va)
    orig = data[off:off + len(new)]
    if orig == new:
        print(f"  warning: bytes at 0x{va:08x} already equal the new bytes "
              f"(recording anyway; original == new)")
    p = m['patches'].setdefault(name, {'desc': desc or '', 'sites': []})
    if desc and not p['desc']:
        p['desc'] = desc
    for s in p['sites']:
        if s['va'] == va:
            sys.exit(f"  site 0x{va:08x} already in patch '{name}' — remove first")
    p['sites'].append({'va': va, 'orig': orig.hex(), 'new': new.hex()})
    save_manifest(m)
    print(f"  {name}: site 0x{va:08x}  {orig.hex()} -> {new.hex()}  ({len(new)} bytes)")


def _overlaps(m):
    spans = []
    for name, p in m['patches'].items():
        for s in p['sites']:
            spans.append((s['va'], s['va'] + len(s['new']) // 2, name))
    spans.sort()
    out = []
    for i in range(1, len(spans)):
        a, b = spans[i - 1], spans[i]
        if b[0] < a[1] and a[2] != b[2]:
            out.append((a[2], b[2], b[0]))
    return out


def cmd_status(m, exe):
    with open(exe, 'rb') as f:
        data = f.read()
    print(f"  exe:      {exe}")
    print(f"  manifest: {MANIFEST}")
    for name, p in sorted(m['patches'].items()):
        states = {site_state(data, s) for s in p['sites']}
        state = states.pop() if len(states) == 1 else 'MIXED(' + ','.join(sorted(states)) + ')'
        print(f"  {name:28s} {len(p['sites'])} site(s)  [{state}]  {p['desc'][:40]}")
    if not m['patches']:
        print("  (no patches defined — use `add`)")
    for a, b, va in _overlaps(m):
        print(f"  !! COLLISION: '{a}' and '{b}' overlap at 0x{va:08x}")


def cmd_show(m, name, exe):
    p = m['patches'].get(name) or sys.exit(f"no patch '{name}'")
    with open(exe, 'rb') as f:
        data = f.read()
    print(f"  {name} — {p['desc']}")
    for s in p['sites']:
        print(f"    0x{s['va']:08x}  {s['orig']} -> {s['new']}  [{site_state(data, s)}]")


def _write_sites(exe, sites, use_new, out=None):
    with open(exe, 'rb') as f:
        data = bytearray(f.read())
    want_from = 'orig' if use_new else 'new'
    for s in sites:
        st = site_state(bytes(data), s)
        target = 'applied' if not use_new else 'clean'
        if st == 'FOREIGN':
            sys.exit(f"  ABORT: bytes at 0x{s['va']:08x} match neither original nor "
                     f"patch (other mod / wrong exe version). Nothing written.")
        if (use_new and st == 'applied') or (not use_new and st == 'clean'):
            print(f"    0x{s['va']:08x} already in target state — skipped")
            continue
        off = va_to_file(bytes(data), s['va'])
        blob = bytes.fromhex(s['new'] if use_new else s['orig'])
        data[off:off + len(blob)] = blob
        print(f"    0x{s['va']:08x} {'patched' if use_new else 'reverted'} ({len(blob)} bytes)")
    dest = out or exe
    if out is None and use_new:
        bak = exe + '.prepatch.bak'
        if not os.path.exists(bak):
            shutil.copy2(exe, bak)
            print(f"    backup: {bak}")
    with open(dest, 'wb') as f:
        f.write(data)
    print(f"  wrote {dest}")


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    exe = DEFAULT_EXE
    out = None
    if '--exe' in args:
        i = args.index('--exe'); exe = args[i + 1]; args = args[:i] + args[i + 2:]
    if '--out' in args:
        i = args.index('--out'); out = args[i + 1]; args = args[:i] + args[i + 2:]
    desc = ''
    if '--desc' in args:
        i = args.index('--desc'); desc = args[i + 1]; args = args[:i] + args[i + 2:]
    if out and not os.path.isabs(out):
        out = os.path.join(os.path.dirname(exe), out)

    m = load_manifest()
    cmd = args[0]
    if cmd == 'add':
        cmd_add(m, args[1], args[2], args[3], desc, exe)
    elif cmd == 'status':
        cmd_status(m, exe)
    elif cmd == 'show':
        cmd_show(m, args[1], exe)
    elif cmd == 'apply':
        p = m['patches'].get(args[1]) or sys.exit(f"no patch '{args[1]}'")
        if out:
            shutil.copy2(exe, out)
            _write_sites(out, p['sites'], use_new=True, out=out)
        else:
            _write_sites(exe, p['sites'], use_new=True)
    elif cmd == 'revert':
        p = m['patches'].get(args[1]) or sys.exit(f"no patch '{args[1]}'")
        _write_sites(exe, p['sites'], use_new=False)
    elif cmd == 'remove':
        if m['patches'].pop(args[1], None) is None:
            sys.exit(f"no patch '{args[1]}'")
        save_manifest(m)
        print(f"  removed '{args[1]}' from manifest (exe untouched)")
    else:
        print(f"unknown command '{cmd}'")
        print(__doc__)


if __name__ == '__main__':
    main()
