"""INTERNAL IMPLEMENTATION. Public callers use: python tools/f1c_mod.py ida-query / ida-export / ida-survey.

Read-only idalib bridge. It is always given a disposable working copy of an IDB;
it never saves the database. Results are written as JSON to --result so that
idalib's own console output cannot corrupt the machine-readable answer.
"""
import idapro  # must be the first import (idalib requirement)

import argparse
import json
import os
import re
import sys
import time

MAX_BYTES = 4096
MAX_DISASM = 400
MAX_XREFS = 500
MAX_FUNCTIONS = 20000
MAX_STRINGS = 20000
MAX_STRING_REFS = 20000
MAX_CALL_EDGES = 60000
STRING_CHUNK = 1 << 20
STRING_OVERLAP = 512


def open_db(path):
    rc = idapro.open_database(path, True)
    if rc != 0:
        raise RuntimeError(f"idalib could not open the working database (rc={rc})")


def parse_address(value):
    match = re.fullmatch(r"(?:0[xX]|sub_)?([0-9a-fA-F]{1,8})", str(value).strip())
    if not match:
        raise ValueError(f"not an address: {value}")
    return int(match.group(1), 16)


def fmt(ea):
    return f"0x{ea:08X}"


def function_record(ea):
    import ida_funcs
    import ida_name
    func = ida_funcs.get_func(ea)
    if func is None:
        return None
    return {"start": fmt(func.start_ea), "end": fmt(func.end_ea),
            "name": ida_name.get_name(func.start_ea)}


def op_info(ea, args):
    import ida_name
    import idc
    return {"address": fmt(ea), "name": ida_name.get_name(ea) or None,
            "function": function_record(ea), "type": idc.get_type(ea),
            "segment": idc.get_segm_name(ea) or None}


def op_decompile(ea, args):
    import ida_hexrays
    if not ida_hexrays.init_hexrays_plugin():
        raise RuntimeError("Hex-Rays decompiler is unavailable")
    func = function_record(ea)
    if func is None:
        raise ValueError(f"no function contains {fmt(ea)}")
    cfunc = ida_hexrays.decompile(int(func["start"], 16))
    if cfunc is None:
        raise RuntimeError(f"decompilation failed at {func['start']}")
    lines = str(cfunc).splitlines()
    shown = lines[:args.max_lines]
    return {"address": fmt(ea), "function": func, "line_count": len(lines),
            "truncated": len(lines) > len(shown), "pseudocode": "\n".join(shown)}


def op_disasm(ea, args):
    import idc
    count = min(args.count, MAX_DISASM)
    rows = []
    cursor = ea
    for _ in range(count):
        text = idc.generate_disasm_line(cursor, 0)
        if not text:
            break
        rows.append({"address": fmt(cursor), "text": text})
        following = idc.next_head(cursor, idc.BADADDR)
        if following == idc.BADADDR:
            break
        cursor = following
    return {"address": fmt(ea), "count": len(rows), "instructions": rows}


def _xref_rows(refs):
    import idautils
    rows = []
    total = 0
    for ref in refs:
        total += 1
        if len(rows) >= MAX_XREFS:
            continue
        source, target = ref["from"], ref["to"]
        rows.append({"from": fmt(source), "to": fmt(target),
                     "type": idautils.XrefTypeName(ref["type"]),
                     "user": ref["user"], "from_function": function_record(source)})
    return rows, total


def op_xrefs_to(ea, args):
    import idautils
    refs = ({"from": x.frm, "to": x.to, "type": x.type, "user": bool(x.user)}
            for x in idautils.XrefsTo(ea, 0))
    rows, total = _xref_rows(refs)
    return {"address": fmt(ea), "total": total, "returned": len(rows),
            "truncated": total > len(rows), "xrefs": rows}


def op_xrefs_from(ea, args):
    import ida_funcs
    import idautils
    func = ida_funcs.get_func(ea)
    heads = list(idautils.FuncItems(func.start_ea)) if func else [ea]
    refs = ({"from": x.frm, "to": x.to, "type": x.type, "user": bool(x.user)}
            for head in heads for x in idautils.XrefsFrom(head, 0)
            if not (func and func.start_ea <= x.to < func.end_ea))
    rows, total = _xref_rows(refs)
    return {"address": fmt(ea), "scope": "function" if func else "instruction",
            "total": total, "returned": len(rows), "truncated": total > len(rows),
            "xrefs": rows}


def op_bytes(ea, args):
    import ida_bytes
    size = args.size
    if not 1 <= size <= MAX_BYTES:
        raise ValueError(f"--size must be 1..{MAX_BYTES}")
    data = ida_bytes.get_bytes(ea, size)
    if data is None:
        raise ValueError(f"bytes unavailable at {fmt(ea)}")
    return {"address": fmt(ea), "size": size, "hex": data.hex().upper()}


OPERATIONS = {"info": op_info, "decompile": op_decompile, "disasm": op_disasm,
              "xrefs-to": op_xrefs_to, "xrefs-from": op_xrefs_from, "bytes": op_bytes}


def safe_file_name(name, ea):
    stem = re.sub(r"[^A-Za-z0-9_]", "_", name or f"sub_{ea:X}")[:120]
    return f"{stem}_{ea:08x}.c"


def export_all(args):
    import ida_hexrays
    import ida_name
    import idautils
    if not ida_hexrays.init_hexrays_plugin():
        raise RuntimeError("Hex-Rays decompiler is unavailable")
    output = args.output
    written, failures = 0, []
    started = time.time()
    for ea in idautils.Functions():
        name = ida_name.get_name(ea)
        try:
            cfunc = ida_hexrays.decompile(ea)
        except ida_hexrays.DecompilationFailure as error:
            failures.append({"address": fmt(ea), "name": name, "error": str(error)})
            continue
        if cfunc is None:
            failures.append({"address": fmt(ea), "name": name, "error": "no result"})
            continue
        path = os.path.join(output, safe_file_name(name, ea))
        with open(path, "x", encoding="utf-8", newline="\n") as handle:
            handle.write(str(cfunc) + "\n")
        written += 1
    return {"written": written, "failures": failures,
            "seconds": round(time.time() - started, 1)}


def _segment_blocks(start, size):
    """Yield (address, bytes) blocks of a segment with a small overlap."""
    import ida_bytes
    offset = 0
    while offset < size:
        want = min(STRING_CHUNK, size - offset)
        data = ida_bytes.get_bytes(start + offset, want)
        if data:
            yield start + offset, data
        offset += max(want - STRING_OVERLAP, 1)


def _printable(byte):
    return 0x20 <= byte <= 0x7E or byte == 9


def _scan_segment_strings(start, size, min_len, found):
    for base, data in _segment_blocks(start, size):
        index = 0
        length = len(data)
        while index < length:
            byte = data[index]
            if _printable(byte):
                end = index
                while end < length and _printable(data[end]):
                    end += 1
                run = end - index
                if run >= min_len:
                    address = base + index
                    key = fmt(address)
                    if key not in found:
                        found[key] = {"address": key, "encoding": "ascii",
                                      "text": data[index:end].decode("ascii")}
                index = end
            else:
                index += 1
        index = 0
        while index + 1 < length:
            if _printable(data[index]) and data[index + 1] == 0:
                end = index
                while (end + 1 < length and _printable(data[end])
                       and data[end + 1] == 0):
                    end += 2
                run = (end - index) // 2
                if run >= min_len:
                    address = base + index
                    key = fmt(address)
                    if key not in found:
                        text = data[index:end:2].decode("ascii")
                        found[key] = {"address": key, "encoding": "utf16le",
                                      "text": text}
                index = end + 1
            else:
                index += 1


def survey_all(args):
    import ida_funcs
    import ida_name
    import ida_nalt
    import ida_segment
    import idautils
    import idc

    min_string = max(2, args.min_string)
    max_strings = max(1, min(args.max_strings, MAX_STRINGS))
    max_functions = max(1, min(args.max_functions, MAX_FUNCTIONS))

    functions = []
    starts = set()
    functions_truncated = False
    for ea in idautils.Functions():
        if len(functions) >= max_functions:
            functions_truncated = True
            break
        func = ida_funcs.get_func(ea)
        if func is None:
            continue
        flags = func.flags
        functions.append({
            "start": fmt(func.start_ea), "end": fmt(func.end_ea),
            "size": func.end_ea - func.start_ea,
            "name": ida_name.get_name(func.start_ea),
            "segment": idc.get_segm_name(func.start_ea) or None,
            "library": bool(flags & ida_funcs.FUNC_LIB),
            "thunk": bool(flags & ida_funcs.FUNC_THUNK),
        })
        starts.add(func.start_ea)

    edges = set()
    edges_truncated = False
    for item in functions:
        origin = int(item["start"], 16)
        for head in idautils.FuncItems(origin):
            for ref in idautils.XrefsFrom(head, 0):
                target = ref.to
                if target in starts and target != origin:
                    edges.add((origin, target))
        if len(edges) >= MAX_CALL_EDGES:
            edges_truncated = True
            break

    imports = []
    for module_index in range(ida_nalt.get_import_module_qty()):
        module = ida_nalt.get_import_module_name(module_index)

        def collect(ea, name, ordinal):
            imports.append({"address": fmt(ea), "name": name,
                            "ordinal": ordinal, "module": module})
            return True

        ida_nalt.enum_import_names(module_index, collect)

    exports = [{"address": fmt(ea), "name": name, "ordinal": ordinal}
               for index, ordinal, ea, name in idautils.Entries()]

    found = {}
    strings_truncated = False
    for segment_ea in idautils.Segments():
        if len(found) >= max_strings:
            strings_truncated = True
            break
        segment = ida_segment.getseg(segment_ea)
        if segment is None:
            continue
        size = segment.end_ea - segment.start_ea
        if size <= 0:
            continue
        _scan_segment_strings(segment.start_ea, size, min_string, found)

    strings = sorted(found.values(), key=lambda row: row["address"])[:max_strings]

    string_refs = []
    refs_truncated = False
    for row in strings:
        ea = int(row["address"], 16)
        for ref in idautils.XrefsTo(ea, 0):
            owner = function_record(ref.frm)
            string_refs.append({"string": row["address"], "from": fmt(ref.frm),
                                "function": owner["start"] if owner else None,
                                "type": idautils.XrefTypeName(ref.type)})
        if len(string_refs) >= MAX_STRING_REFS:
            refs_truncated = True
            break

    return {
        "functions": functions,
        "functions_truncated": functions_truncated,
        "imports": imports,
        "exports": exports,
        "strings": strings,
        "strings_truncated": strings_truncated,
        "string_refs": string_refs,
        "string_refs_truncated": refs_truncated,
        "call_edges": [{"from": fmt(origin), "to": fmt(target)}
                       for origin, target in sorted(edges)],
        "call_edges_truncated": edges_truncated,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", required=True)
    parser.add_argument("--result", required=True)
    sub = parser.add_subparsers(dest="mode", required=True)
    query = sub.add_parser("query")
    query.add_argument("operation", choices=sorted(OPERATIONS))
    query.add_argument("address")
    query.add_argument("--max-lines", type=int, default=200)
    query.add_argument("--count", type=int, default=40)
    query.add_argument("--size", type=int, default=64)
    export = sub.add_parser("export")
    export.add_argument("--output", required=True)
    survey = sub.add_parser("survey")
    survey.add_argument("--min-string", type=int, default=4)
    survey.add_argument("--max-strings", type=int, default=MAX_STRINGS)
    survey.add_argument("--max-functions", type=int, default=MAX_FUNCTIONS)
    args = parser.parse_args()
    result = {"ok": False}
    try:
        open_db(args.db)
        try:
            if args.mode == "query":
                ea = parse_address(args.address)
                result = {"ok": True, "operation": args.operation,
                          **OPERATIONS[args.operation](ea, args)}
            elif args.mode == "survey":
                result = {"ok": True, **survey_all(args)}
            else:
                result = {"ok": True, **export_all(args)}
        finally:
            idapro.close_database(False)
    except Exception as error:  # reported to the caller as structured failure
        result = {"ok": False, "error": f"{type(error).__name__}: {error}"}
    with open(args.result, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=2)
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
