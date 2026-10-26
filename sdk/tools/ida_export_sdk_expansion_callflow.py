"""Export instruction evidence for the high-leverage SDK expansion anchors.

Run under IDA in autonomous mode. This script is read-only with respect to the
database: it writes the JSON path selected by F1C_IDA_EXPORT_PATH and exits
without saving.
"""
import json
import os
from pathlib import Path

import ida_bytes
import ida_funcs
import ida_lines
import ida_name
import idaapi
import idautils
import idc


TARGETS = (
    0x0040BE70, 0x0040CA30, 0x0042D8A0, 0x00451720,
    0x0045BD10, 0x004628A0, 0x00496030, 0x004C55B0,
    0x004C73F0, 0x004D82D0, 0x004E0ED0, 0x004EF5A0,
    0x004EF930, 0x004F24F0, 0x004F88A0, 0x0057A6A0,
)


def instruction(ea):
    size = idc.get_item_size(ea)
    refs_from = sorted(set(idautils.CodeRefsFrom(ea, False)))
    return {
        "address": f"0x{ea:08X}",
        "size": size,
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
    insns = [instruction(item_ea) for item_ea in items]
    callers = []
    for ref in sorted(set(idautils.CodeRefsTo(func.start_ea, False))):
        callers.append({
            "address": f"0x{ref:08X}",
            "function": f"0x{(ida_funcs.get_func(ref).start_ea if ida_funcs.get_func(ref) else ref):08X}",
            "instruction": instruction(ref),
        })
    entry_bytes = []
    consumed = 0
    for insn in insns:
        if consumed >= 32:
            break
        entry_bytes.append(insn)
        consumed += insn["size"]
    return {
        "address": f"0x{func.start_ea:08X}",
        "end_address": f"0x{func.end_ea:08X}",
        "name": ida_name.get_name(func.start_ea) or "",
        "callers": callers,
        "entry_instructions_through_32_bytes": entry_bytes,
        "calls": [x for x in insns if x["mnemonic"].lower() == "call"],
        "returns": [x for x in insns if x["mnemonic"].lower().startswith("ret")],
        "instructions": insns,
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
    print(f"SDK expansion instruction export written: {output}")
    idc.qexit(0)


main()
