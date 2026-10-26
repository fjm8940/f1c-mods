#!/usr/bin/env python3
"""Build gate: every staged sidecar must be byte-identical to its source and
to its VERIFICATION_MANIFEST.json pin, and every metadata file must pin the
same hashes. Called by build.bat right after the copy step so a line-ending
or stale-copy defect fails the build instead of shipping a pair that does
not match its own manifest (the rc.6 staging defect). 4.3.0 adds the viewer
sidecar (telemetry_viewer.js, manifest key sidecar_viewer); the compiled
expected-hash constants are checked by pin_sidecars.py --check.
Read-only; no output files.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

SIDECARS = [("workbench_ui.js", "sidecar"), ("telemetry_viewer.js", "sidecar_viewer")]


def main():
    if len(sys.argv) != 2:
        print("usage: verify_sidecar.py BUILD_DIR")
        return 2
    root = Path(__file__).resolve().parent
    build = Path(sys.argv[1])
    manifest = json.loads((root / "VERIFICATION_MANIFEST.json").read_text(encoding="utf-8"))
    ok = True
    expected = {}
    for name, key in SIDECARS:
        if not (root / name).is_file() or not (build / name).is_file():
            print("sidecar staging: source or staged %s missing" % name)
            return 2
        want = str((manifest.get(key) or {}).get("sha256", "")).upper()
        expected[name] = want
        source = (root / name).read_bytes()
        staged = (build / name).read_bytes()
        src_hash = hashlib.sha256(source).hexdigest().upper()
        staged_hash = hashlib.sha256(staged).hexdigest().upper()
        good = source == staged and staged_hash == want and src_hash == want
        ok = ok and good
        print("sidecar staging: %s source=%s staged=%s manifest=%s bytes=%d/%d %s" % (
            name, src_hash, staged_hash, want, len(source), len(staged), "OK" if good else "MISMATCH"))
    for meta in ('f1c_mod.json', 'mod.json', 'web_telemetry.dll.mod.json'):
        data = json.loads((root / meta).read_text(encoding='utf-8'))
        pins = {item.get('file'): str(item.get('sha256', '')).upper() for item in data.get('sidecars', [])}
        matched = pins == expected
        ok = ok and matched
        print('sidecar metadata:', meta, 'OK' if matched else 'MISMATCH')
    compiled = subprocess.run([sys.executable, str(root / 'pin_sidecars.py'), '--check'],
                              capture_output=True, text=True)
    print('sidecar compiled pins:', 'OK' if compiled.returncode == 0 else 'STALE')
    ok = ok and compiled.returncode == 0
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
