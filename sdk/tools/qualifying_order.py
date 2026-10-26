"""Immutable qualifying evidence and conservative transition policy. CLI backend only.

No live writes, grid restoration, function calls, or simulation suppression.
Addresses describe one hash-identified executable and are relocated by module RVA.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import zlib

from process_memory_snapshot import SnapshotError, capture_snapshot, load_manifest

TARGET_SHA256 = "1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003"
REFERENCE_BASE = 0x400000
GLOBALS = {
    "vehicle_count": (0x5C329C, "i"), "vehicle_pointers": (0x7C54F0, None),
    "session_type": (0x7C56A4, "i"), "session_phase": (0x7C56A8, "i"),
    "session_clock": (0x8AFEA0, "d"), "session_start": (0x8ACF90, "f"),
    "session_end": (0x8ACF94, "f"), "lap_limit": (0x8ACF8C, "i"),
    "qualifying_enabled": (0x7CEBC4, "B"), "game_mode": (0x7D0410, "i"),
    "ai_strength": (0x7D01A0, "i"),
}
FIELDS = {
    "driver": (0x4620, "64s"), "controller_kind": (0x32C8, "i"),
    "human_ai_field": (0x32A0, "i"), "position": (0x4060, "i"),
    "stored_grid_position": (0x4058, "i"), "stored_qualifying_time": (0x405C, "f"),
    "lap": (0x403C, "i"), "finish_state": (0x4064, "i"),
    "best_lap": (0x409C, "f"), "line_time": (0x4098, "f"),
    "speed": (0x4440, "f"), "pit_status": (0x3450, "i"),
    "waypoint_selector": (0x4550, "i"), "active_flag": (0x45F4, "i"),
    "ra_norm": (0x44C4, "f"), "qa_norm": (0x44C0, "f"),
    "ra_baseline": (0x44F4, "f"), "live_pace": (0x453C, "f"),
    "wet_factor": (0x4580, "f"),
}


class CapturedMemory:
    def __init__(self, manifest):
        self.manifest = manifest
        self.pages = {int(p["address"], 16): p for p in manifest["pages"]}
        self.cache = {}
        if manifest.get("page_size") != 4096:
            raise SnapshotError("unsupported snapshot page size")

    def read(self, address, size):
        result = bytearray()
        while size:
            base = address & ~4095
            if base not in self.pages:
                raise SnapshotError(f"uncaptured address 0x{address:X}")
            page = self.pages[base]
            if base not in self.cache:
                if page["zero"]:
                    data = bytes(page["size"])
                else:
                    blob = Path(self.manifest["store"]) / (page["sha256"] + ".zlib")
                    try:
                        data = zlib.decompress(blob.read_bytes())
                    except zlib.error as exc:
                        raise SnapshotError(f"invalid compressed page: {blob}") from exc
                if len(data) != page["size"] or hashlib.sha256(data).hexdigest().upper() != page["sha256"].upper():
                    raise SnapshotError(f"snapshot page hash/length mismatch at 0x{base:X}")
                self.cache[base] = data
            offset = address - base
            take = min(size, len(self.cache[base]) - offset)
            if take <= 0:
                raise SnapshotError("read exceeds captured page")
            result.extend(self.cache[base][offset:offset + take])
            address += take
            size -= take
        return bytes(result)

    def value(self, address, fmt):
        value = struct.unpack("<" + fmt, self.read(address, struct.calcsize("<" + fmt)))[0]
        if isinstance(value, bytes):
            return value.split(b"\0", 1)[0].decode("cp1252", errors="replace").strip()
        if isinstance(value, float) and not math.isfinite(value):
            raise SnapshotError(f"nonfinite {fmt} field at 0x{address:X}")
        return value


def completion_policy(state, rows):
    """Evidence classification, deliberately not authority to modify live state."""
    if state["session_type"] != 5:
        return {"state": "NOT_QUALIFYING", "suppress_simulation": False}
    timed_out = state["session_end"] > state["session_start"] and state["session_clock"] >= state["session_end"]
    moving = [r["driver"] for r in rows if abs(r["speed"]) > 0.5]
    # A car stopped on track and a car safely in its garage are not equivalent.
    # Existing fields do not establish a universally reliable off-track predicate.
    terminal = all(r["finish_state"] in (1, 2, 3) for r in rows)
    inactive = bool(rows) and all(r["active_flag"] == 0 for r in rows)
    if timed_out and inactive and not moving:
        label = "COMPLETED_QUALIFYING_PRESERVATION_CANDIDATE"
    elif timed_out and moving:
        label = "CLOCK_EXPIRED_CARS_STILL_MOVING"
    elif timed_out:
        label = "CLOCK_EXPIRED_COMPLETION_UNPROVED"
    else:
        label = "QUALIFYING_TIME_REMAINS"
    return {"state": label, "clock_expired": timed_out,
            "seconds_remaining": max(0.0, state["session_end"] - state["session_clock"]),
            "moving_drivers": moving, "all_finish_states_terminal": terminal,
            "all_active_flags_off": inactive,
            "proposed_action": "PRESERVE_EXISTING_RESULTS" if timed_out and inactive and not moving else "KEEP_NATIVE_SIMULATION",
            "suppress_simulation": False,
            "reason": "Evidence only. Empty track, stationary cars or existing best laps alone do not seal a session."}


def decode(root, snapshot):
    path, manifest = load_manifest(root, snapshot)
    module = manifest["main_module"]
    if module.get("sha256", "").upper() != TARGET_SHA256:
        raise SnapshotError("unsupported executable hash; qualifying layout must be reviewed for this target")
    memory = CapturedMemory(manifest)
    delta = int(module["base"], 16) - REFERENCE_BASE
    state = {name: memory.value(address + delta, fmt)
             for name, (address, fmt) in GLOBALS.items() if fmt}
    count = state["vehicle_count"]
    if not 1 <= count <= 104:
        raise SnapshotError(f"invalid vehicle count: {count}")
    rows, pointers = [], set()
    for slot in range(count):
        pointer = memory.value(GLOBALS["vehicle_pointers"][0] + delta + slot * 4, "I")
        if not pointer or pointer % 4 or pointer in pointers:
            raise SnapshotError("null, unaligned or duplicate active vehicle pointer")
        pointers.add(pointer)
        row = {name: memory.value(pointer + offset, fmt) for name, (offset, fmt) in FIELDS.items()}
        row.update(slot=slot, vehicle=f"0x{pointer:08X}")
        if not row["driver"] or "\ufffd" in row["driver"]:
            raise SnapshotError("unreadable driver identity")
        rows.append(row)
    if len({r["driver"] for r in rows}) != count:
        raise SnapshotError("duplicate driver identity; name-based comparison is ambiguous")
    if sorted(r["position"] for r in rows) != list(range(1, count + 1)):
        raise SnapshotError("classification is not a complete position permutation; retry at a stable session screen")
    rows.sort(key=lambda r: r["position"])
    return {"schema": "f1c.qualifying_order.v1", "read_only": True,
            "captured": manifest["captured"], "pid": manifest["pid"],
            "source_manifest": str(path),
            "source_manifest_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "executable": module, "globals": state, "rows": rows,
            "completion": completion_policy(state, rows),
            "limitations": ["Sequential module capture, not an atomic game-thread snapshot.",
                            "Archive is not a game lock. No simulation is disabled or order restored.",
                            "Stored grid fields are derived from FieldCopy/FieldCopy_c330; compare by driver identity."]}


def compare(before, after):
    if before.get("schema") != "f1c.qualifying_order.v1" or after.get("schema") != "f1c.qualifying_order.v1":
        raise SnapshotError("compare requires qualifying-order JSON exports")
    if before["globals"]["session_type"] != 5:
        raise SnapshotError("before capture must be qualifying")
    if before["executable"].get("sha256") != after["executable"].get("sha256"):
        raise SnapshotError("cannot compare different executable identities")
    old = {r["driver"]: r for r in before["rows"]}
    new = {r["driver"]: r for r in after["rows"]}
    if old.keys() != new.keys():
        raise SnapshotError("lineup differs; refusing to silently compare by slot")
    rows = [{"driver": name, "before": old[name]["position"], "after": new[name]["position"],
             "before_best_lap": old[name]["best_lap"],
             "after_stored_qualifying_time": new[name]["stored_qualifying_time"],
             "after_stored_grid_position": new[name]["stored_grid_position"]}
            for name in old]
    return {"schema": "f1c.qualifying_order_comparison.v1", "read_only": True,
            "before_capture": before["captured"], "after_capture": after["captured"],
            "same_process": before["pid"] == after["pid"],
            "changed_positions": sum(r["before"] != r["after"] for r in rows), "rows": rows}


def write_new(path, report):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write("\n")


def run_command(root, args):
    if Path(args.output).exists():
        raise SnapshotError(f"refusing to overwrite evidence: {args.output}")
    if args.action == "compare":
        report = compare(json.loads(Path(args.before).read_text(encoding="utf-8-sig")),
                         json.loads(Path(args.after).read_text(encoding="utf-8-sig")))
    else:
        if args.action == "capture":
            snapshot, _ = capture_snapshot(root, args.tag, process_name=args.process, scope="module")
        else:
            snapshot = args.snapshot
        report = decode(root, snapshot)
    write_new(args.output, report)
    print(f"Saved: {Path(args.output).resolve()}")
    if "completion" in report:
        print(f"Entries: {len(report['rows'])}; session: {report['globals']['session_type']}")
        print(f"Completion evidence: {report['completion']['state']}")
        for row in report["rows"]:
            lap = row["best_lap"]
            formatted = f"{int(lap // 60)}:{lap % 60:06.3f}" if lap > 0 else "no time"
            print(f"{row['position']:2d}. {row['driver']:<32} {formatted}")
    else:
        print(f"Changed positions: {report['changed_positions']}")
    print("Read-only archive. The game's simulation and grid were not changed.")
