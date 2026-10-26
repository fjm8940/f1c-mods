#!/usr/bin/env python3
"""ida-query / typed-corpus regression: read-only, exact-target, fail-closed."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
IDA_DIR = Path(r"C:\Program Files\IDA Professional 9.1")
EXE = ROOT.parent / "F1 Challenge V10" / "F1 Challenge Online.exe"


def query(*args, expect_ok=True):
    result = subprocess.run([*CLI, "ida-query", *args, "--json"], cwd=ROOT,
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if expect_ok and result.returncode != 0:
        raise SystemExit(f"ida-query {' '.join(args)} failed: {result.stderr.strip()}")
    if not expect_ok:
        if result.returncode == 0:
            raise SystemExit(f"ida-query {' '.join(args)} should have been refused")
        return None
    return json.loads(result.stdout)


if not IDA_DIR.is_dir():
    print("IDA bridge: SKIP (IDA Professional 9.1 not installed on this machine)")
    raise SystemExit(0)

probe = subprocess.run([sys.executable, "-c", "import idapro"], capture_output=True, text=True)
if probe.returncode != 0:
    raise SystemExit("IDA is installed but the idapro package is not activated "
                     "(see tools/sdk/LLM_OPERATING_CONTRACT.md, IDA section)")

recovery = json.loads((ROOT / "analysis" / "recovery_index.json").read_text(encoding="utf-8"))
address = "0x004C6E80"

decompiled = query("decompile", "0x4C6E80")
if decompiled["function"]["start"] != address or "__thiscall" not in decompiled["pseudocode"]:
    raise SystemExit("decompile returned the wrong function")
source_idb = Path(decompiled["source_idb"])
before = decompiled["source_sha256"]

callers = query("xrefs-to", "sub_4C6E80")
xref_functions = {row["from_function"]["start"] for row in callers["xrefs"]
                  if row["from_function"] and row["type"].startswith("Code")}
if xref_functions != set(recovery["functions"][address]["callers"]):
    raise SystemExit(f"xrefs-to callers {sorted(xref_functions)} disagree with recovery index")

idb_bytes = query("bytes", "004C6E80", "--size", "16")["hex"]
if EXE.is_file():
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    rva = 0x004C6E80 - pe.OPTIONAL_HEADER.ImageBase
    exe_bytes = pe.get_data(rva, 16).hex().upper()
    if exe_bytes != idb_bytes:
        raise SystemExit("IDB bytes differ from the canonical executable")

query("bytes", "0x4C6E80", "--size", "99999", expect_ok=False)
query("decompile", "0xZZ", expect_ok=False)
query("decompile", "0x10", expect_ok=False)

import hashlib
digest = hashlib.sha256(source_idb.read_bytes()).hexdigest().upper()
if digest != before:
    raise SystemExit("source IDB changed during read-only queries")

sys.path.insert(0, str(ROOT / "tools"))
import source_explorer  # noqa: E402
typed = source_explorer.CORPORA.get("typed")
if typed:
    corpus = ROOT / typed[0]
    manifest = json.loads((corpus / "MANIFEST.json").read_text(encoding="utf-8"))
    files = len(list(corpus.glob("*.c")))
    if files != manifest["functions_written"]:
        raise SystemExit("typed corpus file count disagrees with its manifest")
    if not list(corpus.glob(f"*_{address[2:].lower()}.c")):
        raise SystemExit("typed corpus lacks the native-clear function")

print("IDA bridge passed: decompile, xrefs agree with recovery index, IDB bytes match exe, "
      "refusals, source IDB unchanged, typed corpus manifest consistent.")
