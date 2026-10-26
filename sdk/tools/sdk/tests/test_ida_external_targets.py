#!/usr/bin/env python3
"""Front-door tests: isolated external-target IDB creation and one-pass IDB survey."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
IDAT = Path(r"C:\Program Files\IDA Professional 9.1\idat.exe")
GCC = Path(r"C:\msys64\mingw32\bin\gcc.exe")
MARKER = "F1C_SURVEY_MARKER_20261009"


def fail(message):
    raise AssertionError(message)


def run(*args, expect_ok=True):
    completed = subprocess.run([*CLI, *args], cwd=ROOT, capture_output=True, text=True)
    if expect_ok and completed.returncode != 0:
        fail(f"command failed: {' '.join(args)}\n{completed.stdout}\n{completed.stderr}")
    return completed


def main():
    if not IDAT.is_file():
        fail(f"IDA batch executable missing: {IDAT}")
    if not GCC.is_file():
        fail(f"test compiler missing: {GCC}")
    with tempfile.TemporaryDirectory(prefix="f1c_ida_ext_test_") as temporary:
        base = Path(temporary)
        source = base / "probe.c"
        source.write_text(
            "#include <windows.h>\n"
            f'static const char marker[] = "{MARKER}";\n'
            "int main(void) { MessageBoxA(0, marker, marker, 0); return 0; }\n",
            encoding="utf-8")
        exe = base / "probe.exe"
        subprocess.run([str(GCC), "-O0", "-o", str(exe), str(source),
                        "-luser32"], check=True)

        missing = run("ida-new-project", str(base / "proj_missing"),
                      "--exe", str(base / "nope.exe"), expect_ok=False)
        if "missing" not in (missing.stdout + missing.stderr).lower():
            fail("missing target was not refused with a clear error")

        project = base / "probe_project"
        run("ida-new-project", str(project), "--exe", str(exe))
        idb = project / "analysis" / "probe.i64"
        provenance = project / "analysis" / "probe.provenance.json"
        if not idb.is_file() or not provenance.is_file():
            fail("external IDB or provenance was not created")
        record = json.loads(provenance.read_text(encoding="utf-8"))
        if record["schema"] != "f1c.external_idb_provenance.v1":
            fail("provenance schema changed")
        if Path(record["target_exe"]).name != "probe.exe":
            fail("provenance does not name the analyzed target")
        for key in ("target_sha256", "idb_sha256"):
            if len(record[key]) != 64:
                fail(f"provenance {key} is not a sha256 digest")

        second = run("ida-new-project", str(project), "--exe", str(exe), expect_ok=False)
        if "refusing" not in (second.stdout + second.stderr).lower():
            fail("existing analysis was not refused")

        survey_path = base / "survey.json"
        run("ida-survey", "--idb", str(idb), "--output", str(survey_path))
        survey = json.loads(survey_path.read_text(encoding="utf-8"))
        if survey["schema"] != "f1c.ida_survey.v1":
            fail("survey schema changed")
        if survey["source_sha256"] != record["idb_sha256"]:
            fail("survey digest does not match the IDB")
        names = {item["name"] for item in survey["functions"]}
        if not any(name and "main" in name for name in names):
            fail("survey did not find the probe entry function")
        if not any(MARKER in item["text"] for item in survey["strings"]):
            fail("survey did not find the planted marker string")
        if not survey["imports"]:
            fail("survey reported no imports for a PE with user32 imports")
        if not survey["call_edges"]:
            fail("survey reported no call edges for a program with a main call chain")

        rerun = run("ida-survey", "--idb", str(idb), "--output", str(survey_path),
                    expect_ok=False)
        if "refusing" not in (rerun.stdout + rerun.stderr).lower():
            fail("survey output overwrite was not refused")

    print("External-target IDB and survey front door passed.")


if __name__ == "__main__":
    main()
