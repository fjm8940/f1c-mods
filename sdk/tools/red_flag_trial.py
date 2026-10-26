#!/usr/bin/env python3
"""Build or load the canonical development Red Flag trial.

Internal specialist backend. Public callers must use:
  python tools/f1c_mod.py tool-run --acknowledge-risk red-flag-trial ...
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys


SDK_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = SDK_ROOT.parent
PROJECT = WORKSPACE / "development" / "mods" / "red_flag"
SOURCE = PROJECT / "src" / "gr44.c"
GENERATED = PROJECT / "generated" / "redflag.c"
GENERATOR = PROJECT / "tools" / "make_release.py"
COMPILER = Path(r"C:\msys64\mingw32\bin\gcc.exe")
LOADER = PROJECT / "trials" / "2026-08-17" / "f1c_inject.exe"
UNLOAD_EVENT = b"F1C_EVF_UNLOAD_redflag"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise SystemExit(f"ERROR: {label} missing: {path}")


def build(args: argparse.Namespace) -> None:
    for path, label in ((SOURCE, "source"), (GENERATOR, "generator"),
                        (COMPILER, "32-bit compiler")):
        require_file(path, label)
    output = Path(args.output).resolve()
    if output.exists():
        raise SystemExit(f"ERROR: refusing to overwrite trial artifact: {output}")
    if PROJECT not in output.parents:
        raise SystemExit("ERROR: trial output must remain inside the Red Flag project")
    output.parent.mkdir(parents=True, exist_ok=True)

    subprocess.run([sys.executable, str(GENERATOR)], cwd=PROJECT,
                   check=True)
    compile_source = GENERATED
    if args.hardened:
        import importlib.util
        spec = importlib.util.spec_from_file_location("rf_hardening", PROJECT / "tools" / "harden_release.py")
        hardening = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(hardening)
        compile_source = hardening.transform(GENERATED, output.with_suffix(".hardened.c"))
    command = [
        str(COMPILER), "-shared", "-O2", "-s",
        "-DAUTO_REDFLAG_ENABLED=1",
        "-fvisibility=hidden", "-fno-asynchronous-unwind-tables",
        "-fno-unwind-tables", "-ffunction-sections", "-fdata-sections",
        "-static", "-static-libgcc", "-Wl,--gc-sections",
        "-Wl,--exclude-all-symbols", "-Wl,--no-insert-timestamp",
        "-Wl,--image-base,0x68000000",
    ]
    command.append("-DRF_RELEASE")
    command.append(f"-I{GENERATED.parent}")
    command.extend([
        "-o", str(output), str(compile_source),
        "-lkernel32", "-luser32", "-lwinmm", "-lgdi32",
    ])
    subprocess.run(command, cwd=PROJECT, check=True)
    require_file(output, "compiled DLL")
    data = output.read_bytes()
    if (data[:2] != b"MZ" or b"AUTO_REDFLAG_ENABLED=1" not in data or
            b"RED FLAG MONITOR" in data):
        raise SystemExit("ERROR: compiled DLL failed PE/automatic-mode checks")
    if b"GetAsyncKeyState" in data and b"RF_MANUAL_ACCEPT_HOTKEY=T" not in data:
        raise SystemExit("ERROR: release-profile DLL imports unowned hotkey polling")
    for forbidden in (b"RF_DEBUG_HOTKEYS=", b"NUMPAD1 = DEBUG",
                      b"NUMPAD2 = FULL", b"NUMPAD3 = pit-retire"):
        if forbidden in data:
            raise SystemExit("ERROR: release-profile DLL contains a debug/manual trial hotkey")
    print(json.dumps({
        "status": "built-not-for-distribution",
        "source": str(SOURCE), "source_sha256": digest(SOURCE),
        "generated": str(GENERATED), "generated_sha256": digest(GENERATED),
        "artifact": str(output), "artifact_sha256": digest(output),
        "artifact_bytes": output.stat().st_size,
        "automatic_trigger": True,
        "debug_hotkeys": False,
        "manual_accept_hotkey": "T",
        "hardened": bool(args.hardened),
        "compile_source": str(compile_source),
        "compile_source_sha256": digest(compile_source),
    }, indent=2))


def load(args: argparse.Namespace) -> None:
    dll = Path(args.dll).resolve()
    require_file(dll, "trial DLL")
    require_file(LOADER, "approved 32-bit loader backend")
    actual = digest(dll)
    expected = args.sha256.upper()
    if actual != expected:
        raise SystemExit(f"ERROR: DLL SHA-256 mismatch: expected {expected}, got {actual}")
    if PROJECT not in dll.parents:
        raise SystemExit("ERROR: refusing to load a DLL outside the Red Flag project")
    subprocess.run([str(LOADER), args.process, str(dll)], cwd=PROJECT,
                   check=True)
    print(json.dumps({"status": "load-request-completed", "dll": str(dll),
                      "sha256": actual, "process": args.process}, indent=2))


def status(_args: argparse.Namespace) -> None:
    if sys.platform != "win32":
        raise SystemExit("ERROR: Red Flag load status is Windows-only")
    kernel32 = ctypes.windll.kernel32
    kernel32.OpenEventA.argtypes = [ctypes.c_uint32, ctypes.c_int,
                                    ctypes.c_char_p]
    kernel32.OpenEventA.restype = ctypes.c_void_p
    handle = kernel32.OpenEventA(0x00100000, False, UNLOAD_EVENT)
    if not handle:
        raise SystemExit("ERROR: Red Flag unload event is absent; DLL is not confirmed loaded")
    kernel32.CloseHandle(handle)
    print(json.dumps({"status": "loaded", "unload_event": UNLOAD_EVENT.decode()},
                     indent=2))


def garage_status(args):
    """Read-only exact-layout garage/vehicle comparison; never calls game code."""
    import struct
    from process_memory_snapshot import find_process, main_module
    pid = find_process(args.process)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x1010, False, pid)
    if not handle: raise OSError(ctypes.get_last_error(), "OpenProcess read-only failed")
    def read(addr, size):
        b = ctypes.create_string_buffer(size); n = ctypes.c_size_t()
        if not kernel.ReadProcessMemory(handle, addr, b, size, ctypes.byref(n)) or n.value != size:
            raise OSError(ctypes.get_last_error(), f"read failed at {addr:#x}")
        return b.raw
    def unpack(addr, fmt): return struct.unpack('<'+fmt, read(addr, struct.calcsize('<'+fmt)))
    try:
        if unpack(0x400000, 'H')[0] != 0x5a4d: raise ValueError("unsupported module base")
        count = unpack(0x5c329c, 'i')[0]
        records = unpack(0x5ea2c0+0x158, 'I')[0]
        pits, garages = unpack(0x5ea2c0+0x164, 'ii')
        if not 2 <= count <= 104 or not 1 <= pits <= 104 or not 1 <= garages <= 16: raise ValueError("unsupported live roster/database")
        rows=[]
        for slot in range(count):
            veh=unpack(0x7c54f0+slot*4,'I')[0]
            if not veh: raise ValueError("null vehicle")
            pit, ordinal=unpack(veh+0x48a4,'ii')
            row={'slot':slot,'vehicle':hex(veh),'name':read(veh+0x4620,32).split(b'\0')[0].decode('latin1'),
                 'finish':unpack(veh+0x4064,'i')[0], 'retired':unpack(veh+0x4410,'B')[0], 'active':unpack(veh+0x45f4,'B')[0],
                 'pit':pit,'ordinal':ordinal,'position':unpack(veh+0x34e0,'fff'),'orientation':unpack(veh+0x34ec,'fff')}
            if 0<=pit<pits and 0<=ordinal<garages:
                pose=unpack(records+28*(pit+ordinal*pits),'ffffff')
                row['garage_position']=pose[:3];row['garage_orientation']=pose[3:]
            rows.append(row)
        print(json.dumps({'read_only':True,'pid':pid,'count':count,'pits':pits,'garages':garages,'vehicles':rows},indent=2))
    finally: kernel.CloseHandle(handle)


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    build_parser = sub.add_parser("build")
    build_parser.add_argument("--output", required=True)
    build_parser.add_argument("--hardened", action="store_true", help="apply the pinned source transform")
    build_parser.set_defaults(func=build)
    load_parser = sub.add_parser("load")
    load_parser.add_argument("dll")
    load_parser.add_argument("--sha256", required=True)
    load_parser.add_argument("--process", default="F1 Challenge Online.exe")
    load_parser.set_defaults(func=load)
    status_parser = sub.add_parser("status")
    status_parser.set_defaults(func=status)
    garage_parser = sub.add_parser("garage-status")
    garage_parser.add_argument("--process", default="F1 Challenge Online.exe")
    garage_parser.set_defaults(func=garage_status)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
