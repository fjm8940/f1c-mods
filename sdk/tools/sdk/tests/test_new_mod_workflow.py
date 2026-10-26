#!/usr/bin/env python3
"""End-to-end test: instantiate, build, and exercise guarded deployment."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CREATE = ROOT / "tools" / "create_f1c_mod.py"
DEPLOY = ROOT / "tools" / "deploy_f1c_mod.py"
SDK = ROOT / "tools" / "sdk"

with tempfile.TemporaryDirectory(prefix="f1c_sdk_workflow_") as temporary:
    base = Path(temporary)
    mod = base / "workflow_probe"
    command = [sys.executable, str(CREATE), "workflow_probe",
               "--title", "Workflow Probe", "--output", str(mod)]
    subprocess.run(command, cwd=ROOT, check=True)
    for generated in mod.iterdir():
        if generated.is_file() and "{{" in generated.read_text(encoding="utf-8"):
            raise SystemExit(f"unexpanded template token in {generated.name}")
    refused = subprocess.run(command, cwd=ROOT, capture_output=True)
    if refused.returncode == 0:
        raise SystemExit("generator overwrote an existing destination")

    environment = os.environ.copy()
    environment["F1C_SDK_DIR"] = str(SDK)
    subprocess.run(["cmd", "/c", str(mod / "build.bat")],
                   cwd=ROOT, env=environment, check=True)
    dll = mod / "build" / "workflow_probe.dll"
    data = dll.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if struct.unpack_from("<H", data, pe + 4)[0] != 0x14C:
        raise SystemExit("starter DLL is not x86")

    destination = base / "deployed"
    destination.mkdir()
    deploy = [sys.executable, str(DEPLOY), str(dll), str(destination)]
    subprocess.run(deploy, cwd=ROOT, check=True)
    if subprocess.run(deploy, cwd=ROOT, capture_output=True).returncode == 0:
        raise SystemExit("deployment overwrote a target without authorization")
    subprocess.run(deploy + ["--replace-sdk-owned"], cwd=ROOT, check=True)

    protected = base / "pace_probe.dll"
    shutil.copy2(dll, protected)
    protected_result = subprocess.run(
        [sys.executable, str(DEPLOY), str(protected), str(destination)],
        cwd=ROOT, capture_output=True)
    if protected_result.returncode == 0:
        raise SystemExit("deployment accepted the protected production filename")

print("New-mod workflow passed: create/refuse-overwrite/build-x86/deploy-guard/update-owned.")
