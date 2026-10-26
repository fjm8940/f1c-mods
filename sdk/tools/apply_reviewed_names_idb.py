"""Apply canonical reviewed function names to the isolated SDK-lab IDB only."""
import json
import os
import ida_auto
import ida_funcs
import ida_loader
import ida_name
import ida_pro

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OVERRIDES = os.path.join(ROOT, "tools", "sdk", "f1c_sdk_overrides.json")

ida_auto.auto_wait()
functions = json.load(open(OVERRIDES, encoding="utf-8"))["functions"]
failed = 0
changed = 0
for reviewed_name, metadata in functions.items():
    ea = int(metadata["address"], 16)
    if ida_funcs.get_func(ea) is None:
        print("[reviewed-names] FAIL no function", metadata["address"], reviewed_name)
        failed += 1
        continue
    old_name = ida_name.get_name(ea)
    if old_name == reviewed_name:
        continue
    if not ida_name.set_name(ea, reviewed_name, ida_name.SN_FORCE | ida_name.SN_NOWARN):
        print("[reviewed-names] FAIL", metadata["address"], old_name, "->", reviewed_name)
        failed += 1
        continue
    print("[reviewed-names] DONE", metadata["address"], old_name, "->", reviewed_name)
    changed += 1
print("[reviewed-names] summary:", changed, "renamed,", failed, "failed")
ida_loader.save_database("", 0)
ida_pro.qexit(1 if failed else 0)

