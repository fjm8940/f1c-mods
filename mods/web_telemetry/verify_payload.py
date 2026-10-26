#!/usr/bin/env python3
"""Build gate (BACKEND 2 review fix 4): candidate payload staging lives in the
build directory (%TEMP%), never in the workspace. The payload dir must contain
exactly the installable set — web_telemetry.dll, workbench_ui.js,
telemetry_viewer.js (4.3.0), web_telemetry.ini and web_telemetry.dll.mod.json — with no test artifacts, and
each file must be byte-identical to its build/source origin. The install route
and the manifest hashes point at this directory. The workspace payload
directory must not exist. Read-only; fails closed.
"""
import hashlib
import os
from pathlib import Path
import sys

EXPECTED = ["web_telemetry.dll", "web_telemetry.ini", "web_telemetry.dll.mod.json",
            "workbench_ui.js", "telemetry_viewer.js"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    if len(sys.argv) not in (2,3) or (len(sys.argv)==3 and sys.argv[2]!='--preflight'):
        print("usage: verify_payload.py BUILD_DIR [--preflight]")
        return 2
    root = Path(__file__).resolve().parent
    build = Path(sys.argv[1]).resolve()
    temp = Path(os.environ['TEMP']).resolve()
    if build == temp or temp not in build.parents:
        print('payload: build output must be a dedicated directory inside TEMP:',build)
        return 1
    if build.exists() and not build.is_dir():
        print('payload: build output must be a directory')
        return 1
    payload = build / 'payload'
    if payload.resolve() != payload or (payload.exists() and not payload.is_dir()):
        print('payload: staging directory must not redirect to another location')
        return 1
    outputs = [build / name for name in ('web_telemetry.dll','workbench_ui.js','telemetry_viewer.js','runtime_test.exe')]
    outputs += [payload / name for name in EXPECTED]
    if any(path.resolve() != path or (path.exists() and not path.is_file()) for path in outputs):
        print('payload: output files must not redirect or name directories')
        return 1
    if len(sys.argv)==3:
        return 0
    if (root / "payload").exists():
        print("payload: workspace payload dir must not exist (staging lives in %TEMP%):",
              root / "payload")
        return 1
    if not payload.is_dir():
        print("payload: staging directory missing:", payload)
        return 2
    present = sorted(p.name for p in payload.iterdir())
    if present != sorted(EXPECTED):
        print("payload: file set mismatch; found", present)
        return 1
    if any(not p.is_file() or p.is_symlink() for p in payload.iterdir()):
        print('payload: every install entry must be a regular file')
        return 1
    checks = [
        ("web_telemetry.dll", build / "web_telemetry.dll"),
        ("workbench_ui.js", root / "workbench_ui.js"),
        ("telemetry_viewer.js", root / "telemetry_viewer.js"),
        ("web_telemetry.ini", root / "web_telemetry.ini"),
        ("web_telemetry.dll.mod.json", root / "web_telemetry.dll.mod.json"),
    ]
    ok = True
    for name, source in checks:
        same = (payload / name).read_bytes() == source.read_bytes()
        ok = ok and same
        print("payload: %-28s %s %s" % (name, sha(payload / name),
                                        "OK" if same else "MISMATCH vs " + str(source)))
    print("payload gate:", "OK" if ok else "FAILED", "(%d files, staging %s)" % (len(present), payload))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
