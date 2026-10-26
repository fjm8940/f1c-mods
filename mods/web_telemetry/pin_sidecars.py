#!/usr/bin/env python3
"""Sidecar pin maintenance (4.3.0). Recomputes the SHA256 of each sidecar
and rewrites every pin that must match it:

  - the expected-hash constant compiled into web_telemetry.c
  - VERIFICATION_MANIFEST.json (sidecar / sidecar_viewer)
  - the sidecars[] list in f1c_mod.json, mod.json, web_telemetry.dll.mod.json

Run deliberately after editing a sidecar, then rebuild; verify_sidecar.py
fails the build when any pin disagrees. Use --check to only report.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SIDECARS = [
    # file, C constant, manifest key, served at
    ("workbench_ui.js", "WORKBENCH_UI_EXPECTED_SHA256", "sidecar", "/workbench.js"),
    ("telemetry_viewer.js", "VIEWER_UI_EXPECTED_SHA256", "sidecar_viewer", "/viewer.js"),
]
METADATA = ("f1c_mod.json", "mod.json", "web_telemetry.dll.mod.json")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def write_json(path, data):
    raw = path.read_text(encoding="utf-8")
    text = json.dumps(data, indent=2, ensure_ascii=False)
    path.write_text(text + ("\n" if raw.endswith("\n") else ""), encoding="utf-8")


def pin_payload(build_dir):
    """Record the staged payload (build_dir/payload) in the manifest. The DLL
    embeds the sidecar pins, so run this after the sidecar pins are current
    and a build has staged the payload; builds are reproducible."""
    payload = Path(build_dir) / "payload"
    if not payload.is_dir():
        print("payload dir missing:", payload)
        return 2
    manifest_path = ROOT / "VERIFICATION_MANIFEST.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    files = {p.name: sha(p) for p in sorted(payload.iterdir()) if p.is_file()}
    entry = manifest.get("payload") or {}
    entry["dir"] = "%TEMP%\\" + Path(build_dir).name + "\\payload"
    entry["files"] = files
    manifest["payload"] = entry
    write_json(manifest_path, manifest)
    for name, h in files.items():
        print("payload %-28s %s" % (name, h))
    return 0


def main():
    if "--payload" in sys.argv:
        i = sys.argv.index("--payload")
        if i + 1 >= len(sys.argv):
            print("usage: pin_sidecars.py --payload BUILD_DIR")
            return 2
        return pin_payload(sys.argv[i + 1])
    check = "--check" in sys.argv
    hashes = {name: sha(ROOT / name) for name, _, _, _ in SIDECARS}
    ok = True
    c_path = ROOT / "web_telemetry.c"
    c_src = c_path.read_text(encoding="utf-8")
    for name, const, _, _ in SIDECARS:
        pat = re.compile(r'(#define %s \\\n    ")([0-9A-F]{64})(")' % const)
        m = pat.search(c_src)
        if not m:
            print("missing C constant", const)
            return 2
        if m.group(2) != hashes[name]:
            ok = False
            c_src = pat.sub(lambda mm: mm.group(1) + hashes[name] + mm.group(3), c_src)
    manifest_path = ROOT / "VERIFICATION_MANIFEST.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for name, _, key, served in SIDECARS:
        entry = manifest.get(key) or {}
        if entry.get("sha256", "").upper() != hashes[name] or entry.get("file") != name:
            ok = False
            entry["file"] = name
            entry["sha256"] = hashes[name]
            entry.setdefault("note", "Must sit beside the installed DLL (served at %s); pinned by pin_sidecars.py and gated by verify_sidecar.py." % served)
            manifest[key] = entry
    meta = {}
    for name in METADATA:
        path = ROOT / name
        data = json.loads(path.read_text(encoding="utf-8"))
        want = [{"file": f, "sha256": hashes[f]} for f, _, _, _ in SIDECARS]
        have = [{"file": i.get("file"), "sha256": str(i.get("sha256", "")).upper()} for i in data.get("sidecars", [])]
        if have != want:
            ok = False
            data["sidecars"] = want
        meta[path] = data
    for name, h in hashes.items():
        print("%-22s %s" % (name, h))
    if check:
        print("pins:", "OK" if ok else "STALE")
        return 0 if ok else 1
    if not ok:
        c_path.write_text(c_src, encoding="utf-8")
        write_json(manifest_path, manifest)
        for path, data in meta.items():
            write_json(path, data)
        print("pins: updated (rebuild required)")
    else:
        print("pins: already current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
