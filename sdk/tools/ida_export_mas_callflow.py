"""Export focused MAS call-flow evidence from an existing IDA database.

Run under IDA in autonomous mode. The script only reads database state, writes
one JSON report selected by F1C_IDA_EXPORT_PATH, and exits without saving.
"""
import json
import os
from pathlib import Path

import ida_bytes
import ida_funcs
import ida_hexrays
import ida_lines
import ida_name
import idaapi
import idautils
import idc


TARGETS = (0x0053CBC0, 0x0055FC40, 0x0055FBE0, 0x0055FE30, 0x0053CE40, 0x0053CED0)


def instruction(ea):
    size = idc.get_item_size(ea)
    refs_from = sorted(set(idautils.CodeRefsFrom(ea, False)))
    return {
        "address": f"0x{ea:08X}",
        "bytes": (ida_bytes.get_bytes(ea, size) or b"").hex(" ").upper(),
        "text": ida_lines.generate_disasm_line(ea, ida_lines.GENDSM_REMOVE_TAGS) or "",
        "mnemonic": idc.print_insn_mnem(ea),
        "operands": [idc.print_operand(ea, i) for i in range(3) if idc.print_operand(ea, i)],
        "code_refs_from": [f"0x{x:08X}" for x in refs_from],
        "target_names": [ida_name.get_name(x) or "" for x in refs_from],
    }


def function_record(ea):
    func = ida_funcs.get_func(ea)
    if func is None:
        return {"address": f"0x{ea:08X}", "error": "function not found"}
    items = list(idautils.FuncItems(func.start_ea))
    calls = []
    for item_ea in items:
        insn = instruction(item_ea)
        if insn["mnemonic"].lower() == "call":
            calls.append(insn)
    try:
        pseudocode = str(ida_hexrays.decompile(func.start_ea))
    except Exception as exc:
        pseudocode = f"<decompilation failed: {exc}>"
    return {
        "address": f"0x{func.start_ea:08X}",
        "end_address": f"0x{func.end_ea:08X}",
        "name": ida_name.get_name(func.start_ea) or "",
        "code_refs_to": [f"0x{x:08X}" for x in sorted(set(idautils.CodeRefsTo(func.start_ea, False)))],
        "calls": calls,
        "instructions": [instruction(item_ea) for item_ea in items],
        "pseudocode": pseudocode,
    }


def main():
    idaapi.auto_wait()
    report = {
        "schema_version": 1,
        "input_file": idaapi.get_input_file_path(),
        "image_base": f"0x{idaapi.get_imagebase():08X}",
        "functions": [function_record(ea) for ea in TARGETS],
    }
    output = Path(os.environ["F1C_IDA_EXPORT_PATH"])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"MAS call-flow export written: {output}")
    idc.qexit(0)


main()
