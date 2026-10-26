"""Import F1CPitMachine and apply it only to reviewed pit-controller methods."""
import json
import os
import ida_auto
import ida_funcs
import ida_loader
import ida_nalt
import ida_pro
import ida_typeinf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HDR = os.path.join(ROOT, "tools", "sdk", "f1c_pit_struct_ida.h")
REVIEWED = os.path.join(ROOT, "tools", "sdk", "pit_retype_reviewed.json")

def log(message):
    print("[pit-types] " + message)

ida_auto.auto_wait()
til = ida_typeinf.get_idati()
failures = ida_typeinf.parse_decls(
    til, open(HDR, encoding="utf-8").read(), None, ida_typeinf.HTI_DCL)
pit = ida_typeinf.tinfo_t()
if failures or not pit.get_named_type(til, "F1CPitMachine"):
    log("FATAL: F1CPitMachine import failed (parse failures=%d)" % failures)
    ida_pro.qexit(2)
pitptr = ida_typeinf.tinfo_t()
pitptr.create_ptr(pit)
float_type = ida_typeinf.tinfo_t()
float_type.create_simple_type(ida_typeinf.BTF_FLOAT)
void_type = ida_typeinf.tinfo_t()
void_type.create_simple_type(ida_typeinf.BTF_VOID)
void_ptr = ida_typeinf.tinfo_t()
void_ptr.create_ptr(void_type)
SECOND_ARG_TYPES = {
    0x004A5D10: void_ptr,
    0x004A6200: float_type,
    0x004A68B0: float_type,
    0x004A6E50: float_type,
}
log("F1CPitMachine imported: sizeof=%#x" % pit.get_size())

entries = json.load(open(REVIEWED, encoding="utf-8"))["approved"]
log("%d reviewed pit-controller methods" % len(entries))
failed = 0
for entry in entries:
    ea = int(entry["addr"], 16)
    if ida_funcs.get_func(ea) is None:
        log("FAIL %#x: no function" % ea)
        failed += 1
        continue
    tif = ida_typeinf.tinfo_t()
    if not ida_nalt.get_tinfo(tif, ea):
        ida_typeinf.guess_tinfo(tif, ea)
    ftd = ida_typeinf.func_type_data_t()
    if not tif.is_func() or not tif.get_func_details(ftd) or len(ftd) < 1:
        log("FAIL %#x: no usable prototype" % ea)
        failed += 1
        continue
    before = str(tif)
    ftd[0].type = pitptr
    if ea in SECOND_ARG_TYPES:
        if len(ftd) < 2:
            log("FAIL %#x: reviewed ABI requires a second argument" % ea)
            failed += 1
            continue
        ftd[1].type = SECOND_ARG_TYPES[ea]
    updated = ida_typeinf.tinfo_t()
    if not updated.create_func(ftd) or not ida_typeinf.apply_tinfo(
            ea, updated, ida_typeinf.TINFO_DEFINITE):
        log("FAIL %#x: type application rejected" % ea)
        failed += 1
        continue
    log("DONE %#x %s: %s -> %s" % (ea, entry["func"], before, updated))

ida_loader.save_database("", 0)
ida_pro.qexit(1 if failed else 0)
