"""
apply_vehicle_retype_idb.py — Retype arg1 (`this`) to `F1CVehicle *` for the
functions listed in tools/sdk/vehicle_retype_reviewed.json, so Hex-Rays renders
`v->driverStatus` instead of `*(_DWORD *)(this + 17680)`.

Prereqs (in order):
  1. tools/gen_vehicle_struct_header.py      -> the headers
  2. tools/apply_vehicle_struct_idb.py       -> F1CVehicle in Local Types
  3. tools/gen_vehicle_retype_candidates.py  -> the candidate list
  BACK UP THE .i64 FIRST. This mutates function prototypes; there is no undo
  across sessions.

Headless: idat.exe -A -L"idat_retype.log" -S"apply_vehicle_retype_idb.py" <i64>
Dry run (reports, changes nothing):  set F1C_RETYPE_DRY=1
MUST end in qexit() or idat hangs forever in the batch loop.

Trust: the generated candidate list is only a lead. This script deliberately
consumes the separately reviewed allowlist, whose entries require constructor,
caller, or direct-layout evidence. Never point this script at raw candidates.
"""
import ida_typeinf
import ida_nalt
import ida_funcs
import ida_auto
import ida_loader
import ida_pro
import json
import os

IDA_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REVIEWED = os.path.join(IDA_ROOT, "tools", "sdk", "vehicle_retype_reviewed.json")
DRY = bool(os.environ.get("F1C_RETYPE_DRY"))


def log(m):
    print(f"[retype] {m}")


ida_auto.auto_wait()
til = ida_typeinf.get_idati()

# resolve `F1CVehicle *`
veh = ida_typeinf.tinfo_t()
if not veh.get_named_type(til, "F1CVehicle"):
    log("FATAL: F1CVehicle not in Local Types — run apply_vehicle_struct_idb.py first")
    ida_pro.qexit(2)
vehptr = ida_typeinf.tinfo_t()
vehptr.create_ptr(veh)
log(f"F1CVehicle sizeof={veh.get_size():#x}; retyping arg1 -> {vehptr}")
if DRY:
    log("DRY RUN — no changes will be written")

cands = json.load(open(REVIEWED, encoding="utf-8"))["approved"]
log(f"{len(cands)} reviewed entries from {os.path.basename(REVIEWED)}")

done = skipped = failed = 0
for c in cands:
    ea = int(c["addr"], 16)
    name = c["func"]
    if ida_funcs.get_func(ea) is None:
        log(f"  SKIP {c['addr']} {name}: no function at address")
        skipped += 1
        continue

    tif = ida_typeinf.tinfo_t()
    got = ida_nalt.get_tinfo(tif, ea)
    if not got:
        # no stored prototype yet — ask the decompiler/analysis for one
        got = ida_typeinf.guess_tinfo(tif, ea) == ida_typeinf.GUESS_FUNC_OK
    if not got or not tif.is_func():
        log(f"  SKIP {c['addr']} {name}: no usable prototype")
        skipped += 1
        continue

    ftd = ida_typeinf.func_type_data_t()
    if not tif.get_func_details(ftd) or len(ftd) < 1:
        log(f"  SKIP {c['addr']} {name}: prototype has no arg1")
        skipped += 1
        continue

    before = str(tif)
    if str(ftd[0].type) == str(vehptr):
        log(f"  OK   {c['addr']} {name}: already F1CVehicle *")
        skipped += 1
        continue

    ftd[0].type = vehptr
    newtif = ida_typeinf.tinfo_t()
    if not newtif.create_func(ftd):
        log(f"  FAIL {c['addr']} {name}: create_func failed")
        failed += 1
        continue

    if DRY:
        log(f"  DRY  {c['addr']} {name}")
        log(f"         before: {before}")
        log(f"         after : {newtif}")
        done += 1
        continue

    if ida_typeinf.apply_tinfo(ea, newtif, ida_typeinf.TINFO_DEFINITE):
        log(f"  DONE {c['addr']} {name}  ({c['evidence']})")
        log(f"         before: {before}")
        log(f"         after : {newtif}")
        done += 1
    else:
        log(f"  FAIL {c['addr']} {name}: apply_tinfo rejected")
        failed += 1

log(f"summary: {done} retyped, {skipped} skipped, {failed} failed"
    + ("  (DRY RUN — nothing written)" if DRY else ""))

if not DRY:
    ida_loader.save_database("", 0)
ida_pro.qexit(1 if failed else 0)
