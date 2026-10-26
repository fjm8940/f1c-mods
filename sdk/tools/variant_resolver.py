#!/usr/bin/env python3
"""Variant-aware PE profiles and fail-closed symbol recipes for F1C."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess


OBJDUMP = Path(r"C:\msys64\mingw32\bin\objdump.exe")


class ResolverError(RuntimeError):
    pass


class PEImage:
    def __init__(self, path):
        self.path = Path(path).resolve()
        self.data = self.path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ResolverError("not a PE image")
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise ResolverError("invalid PE signature")
        self.machine, self.section_count, self.timestamp = struct.unpack_from(
            "<HHI", self.data, pe + 4)
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        optional = pe + 24
        if struct.unpack_from("<H", self.data, optional)[0] != 0x10B:
            raise ResolverError("only PE32 images are supported")
        self.entry_rva = struct.unpack_from("<I", self.data, optional + 16)[0]
        self.image_base = struct.unpack_from("<I", self.data, optional + 28)[0]
        self.image_size = struct.unpack_from("<I", self.data, optional + 56)[0]
        self.checksum = struct.unpack_from("<I", self.data, optional + 64)[0]
        self.dll_characteristics = struct.unpack_from("<H", self.data, optional + 70)[0]
        table = optional + optional_size
        self.sections = []
        for index in range(self.section_count):
            offset = table + index * 40
            name = self.data[offset:offset + 8].split(b"\0", 1)[0].decode("ascii", "replace")
            virtual_size, rva, raw_size, raw_pointer = struct.unpack_from(
                "<IIII", self.data, offset + 8)
            characteristics = struct.unpack_from("<I", self.data, offset + 36)[0]
            self.sections.append({
                "name": name, "rva": rva, "virtual_size": virtual_size,
                "raw_size": raw_size, "raw_pointer": raw_pointer,
                "characteristics": characteristics,
            })

    def rva_to_offset(self, rva):
        for section in self.sections:
            extent = max(section["virtual_size"], section["raw_size"])
            if section["rva"] <= rva < section["rva"] + extent:
                delta = rva - section["rva"]
                if delta >= section["raw_size"]:
                    return None
                return section["raw_pointer"] + delta
        return rva if rva < len(self.data) else None

    def read_va(self, va, size):
        offset = self.rva_to_offset(va - self.image_base)
        if offset is None or offset + size > len(self.data):
            raise ResolverError(f"VA is not file backed: 0x{va:08X}")
        return self.data[offset:offset + size]

    def executable_ranges(self):
        for section in self.sections:
            if section["characteristics"] & 0x20000000 and section["raw_size"]:
                start = section["raw_pointer"]
                yield section, self.data[start:start + section["raw_size"]]

    def profile(self):
        return {
            "path": str(self.path), "sha256": hashlib.sha256(self.data).hexdigest().upper(),
            "file_size": len(self.data), "machine": f"0x{self.machine:04X}",
            "timestamp": f"0x{self.timestamp:08X}", "image_base": f"0x{self.image_base:08X}",
            "entry_rva": f"0x{self.entry_rva:08X}", "image_size": f"0x{self.image_size:08X}",
            "checksum": f"0x{self.checksum:08X}",
            "dynamic_base": bool(self.dll_characteristics & 0x40),
            "sections": [{**section,
                          "sha256": hashlib.sha256(
                              self.data[section["raw_pointer"]:
                                        section["raw_pointer"] + section["raw_size"]]).hexdigest().upper()}
                         for section in self.sections],
        }


def _mask_unstable(data, image_low, image_high):
    mask = bytearray(b"\xFF" * len(data))
    for offset in range(max(0, len(data) - 3)):
        value = struct.unpack_from("<I", data, offset)[0]
        if image_low <= value < image_high:
            mask[offset:offset + 4] = b"\0\0\0\0"
    # Mask direct relative branches. False opcode detections only reduce
    # specificity; canonical uniqueness and quorum checks remain mandatory.
    index = 0
    while index < len(data):
        opcode = data[index]
        if opcode in (0xE8, 0xE9) and index + 5 <= len(data):
            mask[index + 1:index + 5] = b"\0" * 4
            index += 5
        elif opcode == 0x0F and index + 6 <= len(data) and 0x80 <= data[index + 1] <= 0x8F:
            mask[index + 2:index + 6] = b"\0" * 4
            index += 6
        elif (0x70 <= opcode <= 0x7F or opcode == 0xEB) and index + 2 <= len(data):
            mask[index + 1] = 0
            index += 2
        else:
            index += 1
    return bytes(mask)


def _pattern_regex(pattern, mask):
    parts = []
    run = bytearray()
    for byte, keep in zip(pattern, mask):
        if keep:
            run.append(byte)
        else:
            if run:
                parts.append(re.escape(bytes(run))); run.clear()
            parts.append(b".")
    if run:
        parts.append(re.escape(bytes(run)))
    return re.compile(b"".join(parts), re.DOTALL)


def scan_pattern(content, pattern, mask):
    return [match.start() for match in _pattern_regex(pattern, mask).finditer(content)]


def normalized_instructions(exe, start_va, size):
    if not OBJDUMP.is_file():
        return []
    command = [str(OBJDUMP), "-D", "-b", "pei-i386", "-m", "i386", "-M", "intel",
               f"--start-address=0x{start_va:X}", f"--stop-address=0x{start_va + size:X}", str(exe)]
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        return []
    tokens = []
    instruction = re.compile(r"^\s*[0-9a-fA-F]+:\s+(?:[0-9a-fA-F]{2}\s+)+\s*([a-zA-Z].*)$")
    for line in result.stdout.splitlines():
        match = instruction.match(line)
        if not match:
            continue
        text = match.group(1).strip().lower()
        text = re.sub(r"\b(?:0x)?[0-9a-f]{3,}\b", "#", text)
        text = re.sub(r"\s+", " ", text)
        tokens.append(text)
    return tokens


def instruction_hash(exe, start_va, size):
    tokens = normalized_instructions(exe, start_va, size)
    return hashlib.sha256("\n".join(tokens).encode()).hexdigest() if tokens else None


def make_function_recipe(pe, name, address, size, anchor_size=48, volatile_values=None):
    body = pe.read_va(address, size)
    offsets = sorted({0, max(0, size // 2 - anchor_size // 2), max(0, size - anchor_size)})
    anchors = []
    for offset in offsets:
        pattern = body[offset:offset + min(anchor_size, size - offset)]
        mask = bytearray(_mask_unstable(pattern, pe.image_base, pe.image_base + pe.image_size))
        for value in volatile_values or []:
            needle = struct.pack("<I", value)
            for match in re.finditer(re.escape(needle), pattern):
                mask[match.start():match.start() + 4] = b"\0" * 4
        mask = bytes(mask)
        canonical_hits = 0
        for section, content in pe.executable_ranges():
            canonical_hits += len(scan_pattern(content, pattern, mask))
        anchors.append({
            "offset": offset, "pattern": pattern.hex(), "mask": mask.hex(),
            "canonical_hits": canonical_hits,
        })
    return {
        "name": name, "canonical_va": f"0x{address:08X}", "size": size,
        "anchors": anchors, "minimum_anchors": min(2, len(anchors)),
        "normalized_instruction_sha256": instruction_hash(pe.path, address, size),
    }


def make_operand_recipe(pe, function, name, canonical_value, kind, occurrence=None,
                        volatile_values=None):
    address = int(function["canonical_va"], 16)
    body = pe.read_va(address, function["size"])
    needle = struct.pack("<I", canonical_value)
    positions = [match.start() for match in re.finditer(re.escape(needle), body)]
    if occurrence is not None:
        positions = positions[occurrence:occurrence + 1]
    if not positions:
        raise ResolverError(f"operand 0x{canonical_value:X} absent from {function['name']}")
    contexts = []
    for position in positions:
        start = max(0, position - 10)
        end = min(len(body), position + 14)
        pattern = body[start:end]
        mask = bytearray(_mask_unstable(pattern, pe.image_base, pe.image_base + pe.image_size))
        for value in volatile_values or []:
            volatile = struct.pack("<I", value)
            for match in re.finditer(re.escape(volatile), body):
                operand_start, operand_end = match.start(), match.start() + 4
                overlap_start, overlap_end = max(start, operand_start), min(end, operand_end)
                if overlap_start < overlap_end:
                    mask[overlap_start - start:overlap_end - start] = b"\0" * (overlap_end - overlap_start)
        relative = position - start
        mask[relative:relative + 4] = b"\0" * 4
        contexts.append({
            "pattern": pattern.hex(), "mask": bytes(mask).hex(),
            "operand_offset": relative, "canonical_function_offset": position,
        })
    return {"name": name, "kind": kind, "width": 4, "contexts": contexts,
            "minimum_contexts": len(contexts)}


def bootstrap_f1c_recipes(exe, functions_csv):
    pe = PEImage(exe)
    rows = {}
    import csv
    with Path(functions_csv).open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rows[int(row["addr"], 16)] = row
    specs = {
        "BuildActiveVehiclePointerArray": (0x0042BC80, [0x005EC6D8, 0x48C0, 0x32C8]),
        "PitStopDecisionTireCompound": (0x0042A3D0, [0xCD4, 0xD14]),
    }
    functions = {}
    for name, (address, volatile_values) in specs.items():
        row = rows.get(address)
        if not row:
            raise ResolverError(f"function metadata missing: 0x{address:08X}")
        functions[name] = make_function_recipe(pe, name, address, int(row["size"]),
                                                volatile_values=volatile_values)
    symbols = {
        "VehicleArray": {"function": "BuildActiveVehiclePointerArray",
                         **make_operand_recipe(pe, functions["BuildActiveVehiclePointerArray"],
                                               "VehicleArray", 0x005EC6D8, "absolute_va",
                                               volatile_values=specs["BuildActiveVehiclePointerArray"][1])},
        "VehicleStride": {"function": "BuildActiveVehiclePointerArray",
                          **make_operand_recipe(pe, functions["BuildActiveVehiclePointerArray"],
                                                "VehicleStride", 0x48C0, "constant",
                                                volatile_values=specs["BuildActiveVehiclePointerArray"][1])},
        "VehicleLivenessOffset": {"function": "BuildActiveVehiclePointerArray",
                                  **make_operand_recipe(pe, functions["BuildActiveVehiclePointerArray"],
                                                        "VehicleLivenessOffset", 0x32C8, "field_offset",
                                                        volatile_values=specs["BuildActiveVehiclePointerArray"][1])},
        "FrontWingRecordPtr": {"function": "PitStopDecisionTireCompound",
                               **make_operand_recipe(pe, functions["PitStopDecisionTireCompound"],
                                                     "FrontWingRecordPtr", 0xCD4, "field_offset",
                                                     volatile_values=specs["PitStopDecisionTireCompound"][1])},
        "FrontWingIntactFlag": {"function": "PitStopDecisionTireCompound",
                                **make_operand_recipe(pe, functions["PitStopDecisionTireCompound"],
                                                      "FrontWingIntactFlag", 0xD14, "field_offset",
                                                      volatile_values=specs["PitStopDecisionTireCompound"][1])},
    }
    return {
        "schema_version": 1,
        "canonical_target": pe.profile(),
        "policy": "Resolution requires anchor quorum, operand agreement and invariant validation.",
        "functions": functions, "symbols": symbols,
        "invariants": [
            {"kind": "image_range", "symbol": "VehicleArray"},
            {"kind": "range", "symbol": "VehicleStride", "minimum": 0x1000, "maximum": 0x10000},
            {"kind": "range", "symbol": "VehicleLivenessOffset", "minimum": 0, "maximum_symbol": "VehicleStride"},
            {"kind": "range", "symbol": "FrontWingRecordPtr", "minimum": 0, "maximum_symbol": "VehicleStride"},
            {"kind": "range", "symbol": "FrontWingIntactFlag", "minimum": 0, "maximum_symbol": "VehicleStride"},
            {"kind": "delta", "left": "FrontWingIntactFlag", "right": "FrontWingRecordPtr",
             "expected": 0x40},
        ],
    }


def resolve_function(pe, recipe):
    votes = {}
    details = []
    for anchor in recipe["anchors"]:
        pattern = bytes.fromhex(anchor["pattern"]); mask = bytes.fromhex(anchor["mask"])
        hits = []
        for section, content in pe.executable_ranges():
            for hit in scan_pattern(content, pattern, mask):
                hit_rva = section["rva"] + hit
                candidate_rva = hit_rva - int(anchor["offset"])
                hits.append(candidate_rva)
                votes[candidate_rva] = votes.get(candidate_rva, 0) + 1
        details.append({"offset": anchor["offset"], "hits": [f"0x{value:08X}" for value in hits]})
    if not votes:
        return {"status": "UNRESOLVED", "reason": "no anchors matched", "anchors": details}
    best_votes = max(votes.values())
    best = [rva for rva, count in votes.items() if count == best_votes]
    if best_votes < recipe["minimum_anchors"] or len(best) != 1:
        return {"status": "AMBIGUOUS", "reason": f"best quorum {best_votes}, candidates {len(best)}",
                "anchors": details}
    rva = best[0]
    address = pe.image_base + rva
    normalized = instruction_hash(pe.path, address, recipe["size"])
    expected = recipe.get("normalized_instruction_sha256")
    normalized_match = bool(normalized and expected and normalized == expected)
    confidence = 0.7 * (best_votes / len(recipe["anchors"])) + (0.3 if normalized_match else 0.0)
    return {"status": "RESOLVED", "address": f"0x{address:08X}", "rva": f"0x{rva:08X}",
            "anchor_quorum": best_votes, "anchor_total": len(recipe["anchors"]),
            "normalized_match": normalized_match, "confidence": round(confidence, 4),
            "normalized_instruction_sha256": normalized, "anchors": details}


def resolve_operand(pe, function_result, function_recipe, symbol_recipe):
    if function_result.get("status") != "RESOLVED":
        return {"status": "BLOCKED", "reason": "owner function unresolved"}
    function_address = int(function_result["address"], 16)
    body = pe.read_va(function_address, function_recipe["size"])
    values = []
    contexts = []
    for context in symbol_recipe["contexts"]:
        pattern = bytes.fromhex(context["pattern"]); mask = bytes.fromhex(context["mask"])
        hits = scan_pattern(body, pattern, mask)
        context_values = []
        for hit in hits:
            operand = hit + int(context["operand_offset"])
            if operand + 4 <= len(body):
                context_values.append(struct.unpack_from("<I", body, operand)[0])
        contexts.append({"hits": hits, "values": [f"0x{value:08X}" for value in context_values]})
        if len(context_values) == 1:
            values.append(context_values[0])
    if len(values) < symbol_recipe.get("minimum_contexts", 1):
        return {"status": "UNRESOLVED", "reason": "operand context quorum failed", "contexts": contexts}
    if len(set(values)) != 1:
        return {"status": "AMBIGUOUS", "reason": "operand contexts disagree", "contexts": contexts}
    value = values[0]
    return {"status": "RESOLVED", "value": f"0x{value:08X}", "numeric_value": value,
            "kind": symbol_recipe["kind"], "context_quorum": len(values),
            "context_total": len(symbol_recipe["contexts"]),
            "confidence": round(len(values) / len(symbol_recipe["contexts"]), 4),
            "contexts": contexts}


def validate_invariants(pe, symbols, invariants):
    results = []
    for invariant in invariants:
        names = [invariant.get("symbol"), invariant.get("left"), invariant.get("right"),
                 invariant.get("maximum_symbol")]
        names = [name for name in names if name]
        if any(symbols.get(name, {}).get("status") != "RESOLVED" for name in names):
            results.append({**invariant, "passed": False, "reason": "dependent symbol unresolved"})
            continue
        if invariant["kind"] == "image_range":
            value = symbols[invariant["symbol"]]["numeric_value"]
            passed = pe.image_base <= value < pe.image_base + pe.image_size
        elif invariant["kind"] == "range":
            value = symbols[invariant["symbol"]]["numeric_value"]
            maximum = (symbols[invariant["maximum_symbol"]]["numeric_value"]
                       if invariant.get("maximum_symbol") else invariant["maximum"])
            passed = invariant["minimum"] <= value < maximum
        elif invariant["kind"] == "delta":
            left = symbols[invariant["left"]]["numeric_value"]
            right = symbols[invariant["right"]]["numeric_value"]
            passed = left - right == invariant["expected"]
        else:
            passed = False
        results.append({**invariant, "passed": passed})
    return results


def resolve_recipes(exe, recipes):
    pe = PEImage(exe)
    functions = {name: resolve_function(pe, recipe)
                 for name, recipe in recipes["functions"].items()}
    symbols = {}
    for name, recipe in recipes["symbols"].items():
        owner = recipe["function"]
        symbols[name] = resolve_operand(pe, functions[owner], recipes["functions"][owner], recipe)
    invariants = validate_invariants(pe, symbols, recipes.get("invariants", []))
    all_functions = all(item["status"] == "RESOLVED" for item in functions.values())
    all_symbols = all(item["status"] == "RESOLVED" for item in symbols.values())
    all_invariants = all(item["passed"] for item in invariants)
    status = "RESOLVED" if all_functions and all_symbols and all_invariants else "REFUSED"
    confidence_values = ([item.get("confidence", 0) for item in functions.values()] +
                         [item.get("confidence", 0) for item in symbols.values()])
    confidence = min(confidence_values) if status == "RESOLVED" and confidence_values else 0.0
    return {"schema_version": 1, "status": status, "target": pe.profile(),
            "confidence": round(confidence, 4),
            "functions": functions, "symbols": symbols, "invariants": invariants,
            "policy": "REFUSED profiles must never supply addresses to a mod."}
