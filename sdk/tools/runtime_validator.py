#!/usr/bin/env python3
"""Read-only behavioral validation and discovery of F1C vehicle fields."""
from pathlib import Path
import ctypes
import json
import math
import os
import struct
import time

from process_memory_snapshot import (PROCESS_ACCESS, SnapshotError, find_process,
                                     main_module, read_memory)


class ValidationError(RuntimeError):
    pass


FORMATS = {"int32": "<i", "uint32": "<I", "float32": "<f",
           "uint16": "<H", "uint8": "<B"}
TEXT_ENCODINGS = {"ascii64": 0x40}
LIVE_ENCODINGS = set(FORMATS) | set(TEXT_ENCODINGS)


def number(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def load_recipes(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("recipes"), dict):
        raise ValidationError(f"invalid runtime recipe file: {path}")
    return data


def load_recipe(path, name):
    recipes = load_recipes(path)["recipes"]
    if name not in recipes:
        raise ValidationError(f"unknown runtime recipe {name!r}; available: {', '.join(sorted(recipes))}")
    return recipes[name]


def _unpack(raw, encoding):
    if encoding in TEXT_ENCODINGS:
        return raw[:TEXT_ENCODINGS[encoding]].split(b"\0", 1)[0].decode("cp1252", errors="replace").strip()
    return struct.unpack(FORMATS[encoding], raw[:struct.calcsize(FORMATS[encoding])])[0]


def field_width(encoding):
    if encoding in TEXT_ENCODINGS:
        return TEXT_ENCODINGS[encoding]
    return struct.calcsize(FORMATS[encoding])


def read_controller_event_records(handle, base, *, start=0x864, count=20):
    """Read the commentary controller's bounded 8-DWORD event queue only."""
    raw = read_memory(handle, base + start, count * 0x20)
    if raw is None:
        return None
    records = []
    for index in range(count):
        words = struct.unpack_from("<8I", raw, index * 0x20)
        records.append({"index": index, "event_code": words[0],
                        "score": words[1], "time_raw": words[2],
                        "time_adjusted_raw": words[3],
                        "driver_id_1": words[4], "driver_id_2": words[5],
                        "context_1": words[6], "context_2": words[7]})
    return records


class CandidateStats:
    def __init__(self, offset):
        self.offset = offset
        self.values = 0
        self.in_range = 0
        self.comparisons = 0
        self.unchanged = 0
        self.changes = 0
        self.allowed_changes = 0
        self.monotonic_changes = 0
        self.slots = set()
        self.distinct = set()
        self.previous = {}
        self.transitions = []

    def add(self, slot, value, minimum, maximum, allowed, monotonic, timestamp):
        if isinstance(value, float) and not math.isfinite(value):
            return
        self.values += 1
        self.slots.add(slot)
        if minimum <= value <= maximum:
            self.in_range += 1
        if len(self.distinct) < 128:
            self.distinct.add(value)
        if slot in self.previous:
            old = self.previous[slot]
            self.comparisons += 1
            if value == old:
                self.unchanged += 1
            else:
                delta = value - old
                self.changes += 1
                if delta in allowed:
                    self.allowed_changes += 1
                if ((monotonic == "increasing" and delta > 0) or
                        (monotonic == "decreasing" and delta < 0) or not monotonic):
                    self.monotonic_changes += 1
                if len(self.transitions) < 100:
                    self.transitions.append({"t": round(timestamp, 4), "slot": slot,
                                             "before": old, "after": value, "delta": delta})
        self.previous[slot] = value

    def raw(self):
        return {
            "offset": f"0x{self.offset:04X}", "values": self.values,
            "in_range": self.in_range, "comparisons": self.comparisons,
            "unchanged": self.unchanged, "changes": self.changes,
            "allowed_changes": self.allowed_changes,
            "monotonic_changes": self.monotonic_changes,
            "slots": len(self.slots), "distinct_values": len(self.distinct),
            "sample_values": sorted(self.distinct, key=lambda item: (str(type(item)), item))[:20],
            "transitions": self.transitions,
        }


def score_candidate(raw, recipe):
    behavior = recipe["behavior"]
    values = raw["values"]
    changes = raw["changes"]
    comparisons = raw["comparisons"]
    minimum_changes = behavior.get("minimum_changes", 1)
    minimum_slots = behavior.get("minimum_slots", 1)
    range_score = raw["in_range"] / values if values else 0.0
    allowed_score = raw["allowed_changes"] / changes if changes else 0.0
    monotonic_score = raw["monotonic_changes"] / changes if changes else 0.0
    stability = raw["unchanged"] / comparisons if comparisons else 0.0
    expected_stability = behavior.get("expected_stability", 0.0)
    stability_score = min(1.0, stability / expected_stability) if expected_stability else 1.0
    change_evidence = min(1.0, changes / minimum_changes)
    slot_evidence = min(1.0, raw["slots"] / minimum_slots)
    score = (0.25 * range_score + 0.25 * allowed_score + 0.15 * monotonic_score +
             0.10 * stability_score + 0.15 * change_evidence + 0.10 * slot_evidence)
    if values and not changes:
        verdict = "INCONCLUSIVE_NO_NATURAL_TRANSITION"
    elif changes < minimum_changes:
        verdict = "INCONCLUSIVE_TOO_FEW_TRANSITIONS"
    elif score >= 0.85:
        verdict = "STRONGLY_SUPPORTED"
    elif score >= 0.70:
        verdict = "SUPPORTED"
    else:
        verdict = "NOT_SUPPORTED"
    return {**raw, "score": round(score, 4), "verdict": verdict,
            "checks": {"range_ratio": round(range_score, 4),
                       "allowed_delta_ratio": round(allowed_score, 4),
                       "monotonic_change_ratio": round(monotonic_score, 4),
                       "stability_ratio": round(stability, 4),
                       "change_evidence": round(change_evidence, 4),
                       "slot_evidence": round(slot_evidence, 4)}}


def analyze_stats(recipe_name, recipe, stats, metadata=None, limit=25):
    ranked = sorted((score_candidate(item.raw() if isinstance(item, CandidateStats) else item, recipe)
                     for item in stats), key=lambda item: (-item["score"], -item["changes"], item["offset"]))
    known = recipe.get("known_offset")
    known_result = next((item for item in ranked if number(item["offset"]) == number(known)), None) if known else None
    return {
        "schema_version": 1, "recipe": recipe_name, "mode": (metadata or {}).get("mode"),
        "read_only": True, "metadata": metadata or {},
        "known_offset": known, "known_offset_result": known_result,
        "ranked_candidates": ranked[:limit],
        "static_evidence": recipe.get("static_evidence", []),
        "limitations": recipe.get("limitations", []),
        "policy": "Behavioral support identifies state. It does not authorize or validate writes.",
    }


def validate_observation(recipe_name, recipe, observation, limit=25):
    if observation.get("schema_version") != 1 or observation.get("recipe") != recipe_name:
        raise ValidationError("observation schema or recipe does not match")
    rows = observation.get("candidate_stats")
    if not isinstance(rows, list):
        raise ValidationError("observation contains no candidate_stats")
    return analyze_stats(recipe_name, recipe, rows, observation.get("metadata"), limit)


def observe_live(root, recipe_name, recipe, *, seconds, hz, process_name,
                 mode="known", vehicle_base=None, limit=25, progress=None):
    if os.name != "nt":
        raise ValidationError("live runtime observation requires Windows")
    sdk = json.loads((Path(root) / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
    layout = sdk["vehicle_array"]
    base = number(vehicle_base) if vehicle_base is not None else number(layout["base"])
    stride = number(layout["stride"]); slots = int(layout["slots"])
    active_offset = number(layout["slot_active_field"])
    encoding = recipe["encoding"]
    width = struct.calcsize(FORMATS[encoding])
    if mode == "known":
        offsets = [number(recipe["known_offset"])]
    elif mode == "discover":
        scan = recipe["scan"]
        offsets = list(range(number(scan["start"]), number(scan["end"]) - width + 1,
                             int(scan.get("alignment", width))))
    else:
        raise ValidationError(f"unknown observation mode: {mode}")
    stats = {offset: CandidateStats(offset) for offset in offsets}
    pid = find_process(process_name)
    kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(PROCESS_ACCESS, False, pid)
    if not handle:
        error = kernel.GetLastError()
        if error == 5:
            raise SnapshotError("OpenProcess access denied; run Codex/terminal at the game's integrity level")
        raise SnapshotError(f"OpenProcess failed with Windows error {error}")
    module = main_module(pid)
    globals_now = {}
    started = time.monotonic(); samples = 0; active_seen = set(); unreadable = 0
    interval = 1.0 / hz
    try:
        while time.monotonic() - started < seconds:
            tick = time.monotonic(); elapsed = tick - started
            for name, spec in recipe.get("globals", {}).items():
                raw = read_memory(handle, number(spec["address"]), struct.calcsize(FORMATS[spec["encoding"]]))
                if raw is not None:
                    globals_now[name] = _unpack(raw, spec["encoding"])
            bounds = recipe["value_range"]
            minimum = bounds["minimum"]; maximum = bounds["maximum"]
            global_name = bounds.get("maximum_global")
            if global_name in globals_now and globals_now[global_name] > 0:
                maximum = min(maximum, globals_now[global_name] + bounds.get("maximum_slack", 0))
            for slot in range(slots):
                slot_base = base + slot * stride
                active_raw = read_memory(handle, slot_base + active_offset, 4)
                if active_raw is None or struct.unpack("<i", active_raw)[0] == -1:
                    continue
                active_seen.add(slot)
                if mode == "discover":
                    raw_block = read_memory(handle, slot_base + number(recipe["scan"]["start"]),
                                            number(recipe["scan"]["end"]) - number(recipe["scan"]["start"]))
                    if raw_block is None:
                        unreadable += 1; continue
                    scan_start = number(recipe["scan"]["start"])
                    for offset in offsets:
                        value = _unpack(raw_block[offset - scan_start:offset - scan_start + width], encoding)
                        stats[offset].add(slot, value, minimum, maximum,
                                          recipe["behavior"]["allowed_change_deltas"],
                                          recipe["behavior"].get("monotonic"), elapsed)
                else:
                    for offset in offsets:
                        raw = read_memory(handle, slot_base + offset, width)
                        if raw is None:
                            unreadable += 1; continue
                        stats[offset].add(slot, _unpack(raw, encoding), minimum, maximum,
                                          recipe["behavior"]["allowed_change_deltas"],
                                          recipe["behavior"].get("monotonic"), elapsed)
            samples += 1
            if progress and samples % max(1, round(hz * 5)) == 0:
                progress(elapsed, len(active_seen))
            delay = interval - (time.monotonic() - tick)
            if delay > 0:
                time.sleep(delay)
    finally:
        kernel.CloseHandle(handle)
    metadata = {"mode": mode, "process": process_name, "pid": pid,
                "module": module["name"], "module_base": f"0x{module['base']:08X}",
                "vehicle_base": f"0x{base:08X}", "stride": f"0x{stride:X}",
                "seconds": round(time.monotonic() - started, 3), "hz_requested": hz,
                "samples": samples, "active_slots_seen": sorted(active_seen),
                "globals": globals_now, "unreadable_reads": unreadable}
    observation = {"schema_version": 1, "recipe": recipe_name, "metadata": metadata,
                   "candidate_stats": [item.raw() for item in stats.values()]}
    return observation, analyze_stats(recipe_name, recipe, stats.values(), metadata, limit)


def correlate_live(root, trigger_name, trigger_recipe, context_recipes, *, additional_triggers=(), seconds, hz,
                   process_name, vehicle_base=None, max_events=200, progress=None):
    """Capture bounded, read-only per-slot context when known fields change."""
    if os.name != "nt":
        raise ValidationError("live runtime correlation requires Windows")
    trigger_recipes = {trigger_name: trigger_recipe, **additional_triggers}
    all_recipes = {**trigger_recipes, **context_recipes}
    if any(recipe.get("object") not in ("vehicle", "controller_event_queue")
           for recipe in trigger_recipes.values()):
        raise ValidationError("correlation triggers must be vehicle fields or controller event queues")
    for name, recipe in all_recipes.items():
        if recipe.get("object") == "vehicle" and "known_offset" in recipe:
            pass
        elif recipe.get("object") in ("global", "indirect_global") and all(
                key in recipe for key in ("module_offset", "known_offset")):
            pass
        elif recipe.get("object") == "controller_event_queue" and "module_offset" in recipe:
            continue
        else:
            raise ValidationError(
                f"correlation recipe {name!r} must be a known vehicle field or global")
        if recipe.get("object") != "controller_event_queue" and recipe.get("encoding") not in LIVE_ENCODINGS:
            raise ValidationError(f"correlation recipe {name!r} has unsupported encoding")
    sdk = json.loads((Path(root) / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
    layout = sdk["vehicle_array"]
    base = number(vehicle_base) if vehicle_base is not None else number(layout["base"])
    stride = number(layout["stride"]); slots = int(layout["slots"])
    active_offset = number(layout["slot_active_field"])
    vehicle_fields = {name: (number(recipe["known_offset"]), recipe["encoding"])
                      for name, recipe in all_recipes.items()
                      if recipe.get("object") == "vehicle"}
    global_fields = {name: (number(recipe["module_offset"]), number(recipe["known_offset"]), recipe["encoding"],
                            recipe.get("object") == "indirect_global")
                     for name, recipe in all_recipes.items()
                     if recipe.get("object") in ("global", "indirect_global")}
    event_queues = {name: (number(recipe["module_offset"]), number(recipe.get("table_start", 0x864)),
                           int(recipe.get("record_count", 20)), set(recipe.get("event_codes", (225, 226))))
                    for name, recipe in all_recipes.items()
                    if recipe.get("object") == "controller_event_queue"}
    vehicle_triggers = {name: recipe for name, recipe in trigger_recipes.items()
                        if recipe.get("object") == "vehicle"}
    queue_triggers = {name: recipe for name, recipe in trigger_recipes.items()
                      if recipe.get("object") == "controller_event_queue"}
    # Queue transitions apply to pairs, not one vehicle slot.  Preserve the
    # same-tick vehicle/global state so a later analysis can resolve both
    # members without interpolating between independent samples.
    snapshot_field_names = tuple(vehicle_fields) + tuple(global_fields)
    pid = find_process(process_name)
    kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(PROCESS_ACCESS, False, pid)
    if not handle:
        error = kernel.GetLastError()
        if error == 5:
            raise SnapshotError("OpenProcess access denied; run Codex/terminal at the game's integrity level")
        raise SnapshotError(f"OpenProcess failed with Windows error {error}")
    module = main_module(pid)
    started = time.monotonic(); samples = 0; unreadable = 0; active_seen = set()
    previous = {}; previous_queues = {}; last_values = {}; events = []; interval = 1.0 / hz
    try:
        while time.monotonic() - started < seconds:
            tick = time.monotonic(); elapsed = tick - started
            global_values = {}
            for name, (module_offset, offset, encoding, indirect) in global_fields.items():
                target = module["base"] + module_offset
                if indirect:
                    pointer_raw = read_memory(handle, target, 4)
                    if pointer_raw is None:
                        unreadable += 1; continue
                    target = struct.unpack("<I", pointer_raw)[0]
                raw = read_memory(handle, target + offset, field_width(encoding)) if target else None
                if raw is None:
                    unreadable += 1; continue
                value = _unpack(raw, encoding)
                if isinstance(value, float) and not math.isfinite(value):
                    continue
                global_values[name] = value
            queue_values = {}
            for name, (module_offset, table_start, record_count, event_codes) in event_queues.items():
                records = read_controller_event_records(handle, module["base"] + module_offset,
                                                       start=table_start, count=record_count)
                if records is None:
                    unreadable += 1; continue
                queue_values[name] = [record for record in records if record["event_code"] in event_codes]
            for slot in range(slots):
                slot_base = base + slot * stride
                active_raw = read_memory(handle, slot_base + active_offset, 4)
                if active_raw is None or struct.unpack("<i", active_raw)[0] == -1:
                    continue
                active_seen.add(slot)
                values = {}
                complete = True
                for name, (offset, encoding) in vehicle_fields.items():
                    raw = read_memory(handle, slot_base + offset, field_width(encoding))
                    if raw is None:
                        unreadable += 1; complete = False; break
                    value = _unpack(raw, encoding)
                    if isinstance(value, float) and not math.isfinite(value):
                        complete = False; break
                    values[name] = value
                values.update(global_values)
                values.update(queue_values)
                if not complete:
                    continue
                previous_slot = previous.setdefault(slot, {})
                for name in vehicle_triggers:
                    trigger = values[name]
                    if name in previous_slot and trigger != previous_slot[name] and len(events) < max_events:
                        events.append({"t": round(elapsed, 4), "slot": slot,
                                       "trigger": name, "before": previous_slot[name],
                                       "after": trigger, "context": values})
                    previous_slot[name] = trigger
                last_values[str(slot)] = values
            for name in queue_triggers:
                current = queue_values.get(name, [])
                if name in previous_queues and current != previous_queues[name] and len(events) < max_events:
                    vehicle_snapshot = {
                        slot: {field: row[field] for field in snapshot_field_names if field in row}
                        for slot, row in last_values.items()
                    }
                    driver_map = {slot: row.get("driverDataId") for slot, row in vehicle_snapshot.items()
                                  if row.get("driverDataId") is not None}
                    events.append({"t": round(elapsed, 4), "slot": -1, "trigger": name,
                                   "before": previous_queues[name], "after": current,
                                   "context": {"vehicle_driver_ids": driver_map,
                                               "vehicle_snapshot_by_slot": vehicle_snapshot,
                                               "controller_records": current}})
                previous_queues[name] = current
            samples += 1
            if progress and samples % max(1, round(hz * 5)) == 0:
                progress(elapsed, len(active_seen))
            delay = interval - (time.monotonic() - tick)
            if delay > 0:
                time.sleep(delay)
    finally:
        kernel.CloseHandle(handle)
    metadata = {"process": process_name, "pid": pid, "module": module["name"],
                "module_base": f"0x{module['base']:08X}", "vehicle_base": f"0x{base:08X}",
                "stride": f"0x{stride:X}", "seconds": round(time.monotonic() - started, 3),
                "hz_requested": hz, "samples": samples,
                "active_slots_seen": sorted(active_seen), "unreadable_reads": unreadable}
    field_locations = {name: f"0x{offset:04X}" for name, (offset, _) in vehicle_fields.items()}
    field_locations.update({name: ((f"*[module+0x{module_offset:X}]+0x{offset:X}") if indirect
                                  else f"module+0x{module_offset:X}+0x{offset:X}")
                            for name, (module_offset, offset, _, indirect) in global_fields.items()})
    return {"schema_version": 1, "kind": "runtime_vehicle_correlation", "read_only": True,
            "trigger": trigger_name, "triggers": list(trigger_recipes),
            "fields": field_locations,
            "metadata": metadata, "events": events, "last_values_by_slot": last_values,
            "policy": "Correlation captures read-only state only. It does not establish causation or authorize writes."}


def read_live_fields(root, recipes, *, process_name, vehicle_base=None):
    """Read one bounded, named-field snapshot for the active vehicle slots."""
    if os.name != "nt":
        raise ValidationError("live runtime read requires Windows")
    for name, recipe in recipes.items():
        if recipe.get("object") != "vehicle" or "known_offset" not in recipe:
            raise ValidationError(f"runtime read recipe {name!r} must be a known vehicle field")
        if recipe.get("encoding") not in LIVE_ENCODINGS:
            raise ValidationError(f"runtime read recipe {name!r} has unsupported encoding")
    sdk = json.loads((Path(root) / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
    layout = sdk["vehicle_array"]
    base = number(vehicle_base) if vehicle_base is not None else number(layout["base"])
    stride = number(layout["stride"]); slots = int(layout["slots"])
    active_offset = number(layout["slot_active_field"])
    fields = {name: (number(recipe["known_offset"]), recipe["encoding"])
              for name, recipe in recipes.items()}
    pid = find_process(process_name); kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(PROCESS_ACCESS, False, pid)
    if not handle:
        error = kernel.GetLastError()
        if error == 5:
            raise SnapshotError("OpenProcess access denied; run Codex/terminal at the game's integrity level")
        raise SnapshotError(f"OpenProcess failed with Windows error {error}")
    module = main_module(pid); values = {}; unreadable = 0
    try:
        for slot in range(slots):
            slot_base = base + slot * stride
            active_raw = read_memory(handle, slot_base + active_offset, 4)
            if active_raw is None or struct.unpack("<i", active_raw)[0] == -1:
                continue
            row = {}
            for name, (offset, encoding) in fields.items():
                raw = read_memory(handle, slot_base + offset, field_width(encoding))
                if raw is None:
                    unreadable += 1; continue
                value = _unpack(raw, encoding)
                row[name] = None if isinstance(value, float) and not math.isfinite(value) else value
            values[str(slot)] = row
    finally:
        kernel.CloseHandle(handle)
    return {"schema_version": 1, "kind": "runtime_vehicle_snapshot", "read_only": True,
            "fields": {name: f"0x{offset:04X}" for name, (offset, _) in fields.items()},
            "metadata": {"process": process_name, "pid": pid, "module": module["name"],
                         "module_base": f"0x{module['base']:08X}", "vehicle_base": f"0x{base:08X}",
                         "stride": f"0x{stride:X}", "unreadable_reads": unreadable},
            "values_by_slot": values,
            "policy": "Snapshot is read-only state. It does not establish causation or authorize writes."}


def render_report(report):
    lines = [f"Runtime validation: {report['recipe']}",
             f"Mode: {report.get('mode') or 'recorded'} (read-only)"]
    known = report.get("known_offset_result")
    if known:
        lines += [f"Known offset {known['offset']}: {known['verdict']} "
                  f"(score {known['score']:.1%}, {known['changes']} natural changes)"]
    lines.append("Top candidates:")
    for item in report["ranked_candidates"][:10]:
        lines.append(f"  {item['offset']:>8}  {item['score']:.1%}  {item['verdict']}  "
                     f"changes={item['changes']} allowed={item['allowed_changes']} slots={item['slots']}")
    if known and known["verdict"].startswith("INCONCLUSIVE"):
        lines.append("Result is intentionally inconclusive: observe long enough for natural lap crossings.")
    lines.append("No memory was written. This validates field identity, not write safety.")
    return "\n".join(lines) + "\n"


def pit_marker_chain(read):
    """Bounded read-only exact-target diagnostic of native XPITIN cache."""
    result = {"steward": "0x008ACEE0", "status": "missing_node"}
    def u32(address):
        raw = read(address, 4)
        return struct.unpack("<I", raw)[0] if raw is not None and len(raw)==4 else None
    node=u32(0x008ACEE0+0x194); result["node"]=node
    if not node:return result
    render=u32(node+0x204);result["render"]=render
    if not render:result["status"]="missing_render";return result
    flags=u32(render+0x14);result["flags"]=flags
    for label,offset in [("local",0xdc),("world",0xe8)]:
        raw=read(render+offset,12)
        if raw is not None and len(raw)==12:
            result[label]=[v if math.isfinite(v) else None for v in struct.unpack("<3f",raw)]
    raw=read(node+0x1a0,64)
    if raw is not None and len(raw)==64:
        result["transform"]=[v if math.isfinite(v) else None for v in struct.unpack("<16f",raw)]
    mapper=u32(0x008B8F24); vt=u32(mapper) if mapper else None
    result["point_transform_function"]=u32(vt+4) if vt else None
    result["status"]="cached" if flags is not None and flags&0x20000000 else "uncached"
    local=result.get("local");matrix=result.get("transform")
    if result["status"]=="cached":result["resolved_world"]=result.get("world")
    elif local and matrix and all(v is not None for v in local+matrix) and all(abs(matrix[k])<=0.001 for k in [3,7,11]) and abs(matrix[15]-1)<=0.001:
        result["resolved_world"]=[sum(local[i]*matrix[4*i+k] for i in range(3))+matrix[12+k] for k in range(3)]
    sc=read(0x005EC6D8+24*0x48C0+0x34E0,12)
    if sc is not None and len(sc)==12:
        position=list(struct.unpack("<3f",sc))
        if all(math.isfinite(v) for v in position):
            result["slot24_position"]=position
            world=result.get("resolved_world")
            if world and all(v is not None and math.isfinite(v) for v in world):result["slot24_horizontal_distance"]=math.hypot(position[0]-world[0],position[2]-world[2])

    return result


def read_live_pit_marker(process_name):
    import hashlib
    if os.name!="nt":raise ValidationError("Windows required")
    pid=find_process(process_name);module=main_module(pid)
    if module["base"]!=0x400000 or hashlib.sha256(Path(module["path"]).read_bytes()).hexdigest().upper()!="1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003":
        raise ValidationError("canonical target required for pit-marker read")
    kernel=ctypes.windll.kernel32;handle=kernel.OpenProcess(PROCESS_ACCESS,False,pid)
    if not handle:raise SnapshotError("OpenProcess denied for pit marker")
    try:result=pit_marker_chain(lambda a,n:read_memory(handle,a,n) if 0<a<0x100000000-n else None)
    finally:kernel.CloseHandle(handle)
    return {"read_only":True,"pid":pid,"module":module,"pit_marker":result}
