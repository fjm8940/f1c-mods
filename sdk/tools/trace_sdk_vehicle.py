#!/usr/bin/env python3
"""Read-only, SDK-driven live vehicle tracer with optional pit-cycle capture."""
from pathlib import Path
import argparse
import ctypes
import ctypes.wintypes as wt
import json
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
catalogue = json.loads((ROOT / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
VEH_BASE, VEH_STRIDE, VEH_SLOTS = 0x5EC6D8, 0x48C0, 104
SESSION_TIME, RACE_PHASE = 0x8AFEA0, 0x7C56A8
EXPECTED_PE = {"machine": 0x14C, "timestamp": 0x3E9C24F6,
               "entry": 0x4D8000, "image_size": 0x5FA000,
               "checksum": 0x306F7B}

parser = argparse.ArgumentParser()
parser.add_argument("--car", required=True, help="driver-name prefix")
parser.add_argument("--secs", type=float, default=180.0)
parser.add_argument("--hz", type=float, default=20.0)
parser.add_argument("--wait-phase", type=int)
parser.add_argument("--pit-cycle", action="store_true",
                    help="stop after observing 0 -> 2 -> 3 -> 4 -> 0")
parser.add_argument("--semantic", action="append",
                    help="only trace selected semantics; repeatable")
parser.add_argument("--output", type=Path)
args = parser.parse_args()

class ProcessMemory:
    def __init__(self):
        listing = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq F1 Challenge Online.exe"],
            text=True, errors="replace")
        pid = next((int(token) for line in listing.splitlines()
                    if "f1 challenge online.exe" in line.lower()
                    for token in line.split() if token.isdigit()), None)
        if pid is None:
            raise SystemExit("F1 Challenge Online.exe is not running")
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
        self.kernel.OpenProcess.restype = wt.HANDLE
        self.kernel.ReadProcessMemory.argtypes = (
            wt.HANDLE, wt.LPCVOID, wt.LPVOID, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t))
        self.kernel.ReadProcessMemory.restype = wt.BOOL
        self.kernel.CloseHandle.argtypes = (wt.HANDLE,)
        self.kernel.CloseHandle.restype = wt.BOOL
        self.handle = self.kernel.OpenProcess(0x0410, False, pid)
        if not self.handle:
            raise SystemExit(f"OpenProcess read-only failed: {ctypes.get_last_error()}")

    def read(self, address, size):
        buffer = (ctypes.c_ubyte * size)()
        received = ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(
                wt.HANDLE(self.handle), ctypes.c_void_p(address), buffer, size,
                ctypes.byref(received)) or received.value != size:
            return None
        return bytes(buffer)

    def close(self):
        self.kernel.CloseHandle(wt.HANDLE(self.handle))

def unpack(data, ctype):
    if ctype == "uint8_t": return data[0]
    if ctype == "int32_t": return struct.unpack("<i", data)[0]
    if ctype == "float": return struct.unpack("<f", data)[0]
    if ctype == "void*": return f"0x{struct.unpack('<I', data)[0]:08X}"
    return None

def validate_target(memory):
    header = memory.read(0x400000, 0x400)
    if not header or header[:2] != b"MZ":
        raise SystemExit("target validation failed: no DOS header at 0x400000")
    pe = struct.unpack_from("<I", header, 0x3C)[0]
    actual = {
        "machine": struct.unpack_from("<H", header, pe + 4)[0],
        "timestamp": struct.unpack_from("<I", header, pe + 8)[0],
        "entry": struct.unpack_from("<I", header, pe + 40)[0],
        "image_size": struct.unpack_from("<I", header, pe + 80)[0],
        "checksum": struct.unpack_from("<I", header, pe + 88)[0],
    }
    if actual != EXPECTED_PE:
        raise SystemExit(f"target validation failed: {actual!r}")

fields = []
sizes = {"uint8_t": 1, "int32_t": 4, "float": 4, "void*": 4}
for name, field in catalogue["vehicle_fields"].items():
    if field.get("trust") not in ("FACT", "CONFIRMED"):
        continue
    ctype = field.get("ctype")
    if ctype not in sizes:
        continue
    semantic = field.get("semantic", "state")
    if args.semantic and semantic not in args.semantic:
        continue
    fields.append((int(field["offset"], 16), name, ctype, sizes[ctype],
                   field["trust"], semantic))
fields.sort()

memory = ProcessMemory()
try:
    validate_target(memory)
    deadline = time.monotonic() + args.secs
    slot = None
    while time.monotonic() < deadline and slot is None:
        for candidate in range(VEH_SLOTS):
            base = VEH_BASE + candidate * VEH_STRIDE
            live = memory.read(base + 0x32C8, 4)
            name = memory.read(base + 0x4620, 64)
            if live and name and struct.unpack("<i", live)[0] != -1:
                driver = name.split(b"\0", 1)[0].decode("latin1", "replace")
                if driver.lower().startswith(args.car.lower()):
                    slot = candidate
                    break
        if slot is None:
            time.sleep(0.25)
    if slot is None:
        raise SystemExit(f"driver not found before timeout: {args.car!r}")
    base = VEH_BASE + slot * VEH_STRIDE

    output = args.output or ROOT / "analysis" / "live_traces" / (
        time.strftime("%Y%m%d_%H%M%S_") + args.car.replace(" ", "_") + ".jsonl")
    output.parent.mkdir(parents=True, exist_ok=True)
    previous = {}
    pit_progress = 0
    pit_sequence = [0, 2, 3, 4, 0]
    with output.open("w", encoding="utf-8") as stream:
        header = {"event": "trace_start", "driver": args.car, "slot": slot,
                  "fields": len(fields), "target": EXPECTED_PE}
        stream.write(json.dumps(header) + "\n")
        print(f"Tracing {args.car} in slot {slot}: {len(fields)} fields -> {output}")
        while time.monotonic() < deadline:
            phase_raw = memory.read(RACE_PHASE, 4)
            session_raw = memory.read(SESSION_TIME, 8)
            if not phase_raw or not session_raw:
                break
            phase = struct.unpack("<i", phase_raw)[0]
            session_time = struct.unpack("<d", session_raw)[0]
            if args.wait_phase is not None and phase != args.wait_phase:
                time.sleep(1.0 / max(args.hz, 1.0))
                continue
            for offset, name, ctype, size, trust, semantic in fields:
                raw = memory.read(base + offset, size)
                if raw is None:
                    continue
                value = unpack(raw, ctype)
                if name in previous and previous[name] != value:
                    event = {"event": "field_change", "wall": time.time(),
                             "session_time": session_time, "race_phase": phase,
                             "slot": slot, "offset": f"0x{offset:04X}",
                             "name": name, "type": ctype, "trust": trust,
                             "semantic": semantic, "before": previous[name],
                             "after": value}
                    stream.write(json.dumps(event) + "\n")
                    stream.flush()
                    print(f"s={session_time:8.3f} {name}: {previous[name]} -> {value}")
                    if args.pit_cycle and name == "PitStatus":
                        if value == pit_sequence[pit_progress]:
                            pit_progress += 1
                        elif value == 0:
                            pit_progress = 1
                        if pit_progress == len(pit_sequence):
                            stream.write(json.dumps({"event": "scenario_complete",
                                                     "scenario": "pit_cycle"}) + "\n")
                            print("Pit-cycle scenario complete")
                            raise StopIteration
                previous[name] = value
                if args.pit_cycle and name == "PitStatus" and pit_progress == 0 and value == 0:
                    pit_progress = 1
            time.sleep(1.0 / max(args.hz, 1.0))
except StopIteration:
    pass
finally:
    memory.close()
