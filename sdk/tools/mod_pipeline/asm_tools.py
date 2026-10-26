#!/usr/bin/env python3
"""Deterministic helpers so a weak LLM never hand-disassembles or does rel32 math.

  resolve <addr>            one-shot function dossier: identity, signature hints,
                            callees/callers, string anchors, raw source, ret-imm.
  redirect <caller> <from> <to>
                            find E8 calls to <from> inside <caller>, emit retarget
                            byte patches, and CHECK thiscall stack balance
                            (ret-imm delta) so you don't corrupt the stack.

Everything reads pre-existing repo data + the exe. No IDA needed at runtime.
"""
import argparse
import csv
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TOOLS = os.path.join(ROOT, "tools")
# IDA corpus: raw Hex-Rays .c live in functions/<Name>_<8hex>.c (not ghidra's analyzed/)
ANALYZED = os.path.join(ROOT, "functions")
KG = os.path.join(ROOT, "knowledge_graph")
EXE = os.path.join(ROOT, "..", "F1 Challenge V10", "F1 Challenge Online.exe")
IMAGE_BASE = 0x00400000


def norm(a):
    v = a if isinstance(a, int) else int(a, 16)
    return f"0x{v:08x}"


# --------------------------------------------------------------------- PE map
class PE:
    def __init__(self, path=EXE):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        s = pe + 24 + opt
        self.secs = []
        for i in range(nsec):
            o = s + i * 40
            va = struct.unpack_from("<I", self.data, o + 12)[0]
            vs = struct.unpack_from("<I", self.data, o + 8)[0]
            pr = struct.unpack_from("<I", self.data, o + 20)[0]
            self.secs.append((va, vs, pr))

    def fo(self, va):
        rva = va - IMAGE_BASE
        for v, vs, pr in self.secs:
            if v <= rva < v + vs:
                return pr + (rva - v)
        return None

    def read(self, va, n):
        f = self.fo(va)
        return self.data[f:f + n] if f is not None else None


# ------------------------------------------------------------------- loaders
def raw_c(addr):
    v = int(addr, 16)
    m = glob.glob(os.path.join(ANALYZED, f"*_{v:08x}.c"))
    return m[0] if m else None


def load_xbin():
    p = os.path.join(TOOLS, "cross_binary_final.csv")
    out = {}
    if os.path.exists(p):
        for r in csv.DictReader(open(p, encoding="utf-8", errors="replace")):
            out[norm(r["f1c_addr"])] = r
    return out


def load_callers():
    p = os.path.join(KG, "callers.json")
    return json.load(open(p, encoding="utf-8", errors="replace")) if os.path.exists(p) else {}


def load_strings_for(addr):
    """Strings referenced by a function (from the .c header comment)."""
    p = raw_c(addr)
    if not p:
        return []
    head = open(p, encoding="utf-8", errors="replace").readline() + \
        open(p, encoding="utf-8", errors="replace").readlines()[1] if p else ""
    # simpler: read first 3 lines
    lines = open(p, encoding="utf-8", errors="replace").readlines()[:3]
    for ln in lines:
        m = re.search(r"Strings:\s*\[(.*)\]", ln)
        if m:
            return [s.strip() for s in m.group(1).split(",")]
    return []


def func_size(addr):
    p = os.path.join(TOOLS, "ida_functions.csv")
    if os.path.exists(p):
        v = norm(addr)
        for r in csv.DictReader(open(p, encoding="utf-8", errors="replace")):
            if norm(r["addr"]) == v:
                try:
                    return int(r.get("size", 0))
                except ValueError:
                    return 0
    return 0


def func_end(addr):
    p = os.path.join(TOOLS, "ida_functions.csv")
    if os.path.exists(p):
        v = norm(addr)
        for r in csv.DictReader(open(p, encoding="utf-8", errors="replace")):
            if norm(r["addr"]) == v:
                try:
                    return int(r["end_addr"], 16)
                except (ValueError, KeyError):
                    return None
    return None


def ret_imm(pe, addr):
    """Callee stack cleanup = imm of `ret imm16` (C2 xx xx), else 0 for `ret` (C3).

    Robust: use IDA's function end. The terminal instruction is the epilogue ret.
    A thiscall/stdcall callee uses the SAME `ret N` at every return, so we
    confirm by finding a `ret imm16` immediately followed by CC (int3) padding
    or sitting right at end_addr-3. This avoids picking up C2 operand bytes.
    """
    end = func_end(addr)
    if end:
        # last instruction: check end-3 (C2 imm16) then end-1 (C3)
        tail = pe.read(end - 3, 3)
        if tail:
            if tail[0] == 0xC2:
                return struct.unpack_from("<H", tail, 1)[0]
            if tail[2] == 0xC3:
                return 0
    # fallback: find `ret imm16` that precedes int3 padding (CC) — a real epilogue
    b = pe.read(addr, (end - addr) if end else 0x2000)
    if not b:
        return None
    for i in range(len(b) - 3):
        if b[i] == 0xC2 and b[i + 3] == 0xCC:
            return struct.unpack_from("<H", b, i + 1)[0]
        if b[i] == 0xC3 and b[i + 1] == 0xCC:
            return 0
    return None


# ------------------------------------------------------------------- resolve
def cmd_resolve(args):
    addr = norm(args.addr)
    xbin = load_xbin()
    callers = load_callers()
    pe = PE()

    xrow = xbin.get(addr)
    ident = "UNKNOWN"
    tier = "none"
    if xrow and xrow.get("class"):
        ident = f"{xrow['class']}::{xrow.get('method','')}".rstrip(":")
        tier = "CONFIRMED (NASCAR match)"
    strings = load_strings_for(addr)
    if ident == "UNKNOWN" and strings:
        ident = f"(anchored by strings: {strings[:3]})"
        tier = "FACT (string anchor)"

    size = func_size(addr)
    ri = ret_imm(pe, int(addr, 16))
    cl = callers.get(addr) or callers.get(str(int(addr, 16))) or []

    print(f"== resolve {addr} ==")
    print(f"identity : {ident}    [{tier}]")
    print(f"size     : {size} bytes")
    if xrow:
        print(f"nascar03 : {xrow.get('nascar03_addr','')}   nascar04: {xrow.get('nascar04_addr','')}")
    print(f"ret imm  : {ri if ri is not None else '?'}   "
          f"({'thiscall/stdcall, cleans '+str(ri)+' stack bytes = '+str((ri or 0)//4)+' args' if ri else 'cdecl or plain ret'})")
    print(f"callers  : {len(cl)}  {cl[:8]}")
    if strings:
        print(f"strings  : {strings[:8]}")
    src = raw_c(addr)
    print(f"source   : {src if src else 'NONE'}")
    if src and args.source:
        print("---- source ----")
        sys.stdout.write("".join(open(src, encoding="utf-8", errors="replace").readlines()))


# ------------------------------------------------------------------ redirect
def cmd_redirect(args):
    caller = int(args.caller, 16)
    from_t = int(getattr(args, "from"), 16)
    to_t = int(args.to, 16)
    pe = PE()

    size = func_size(norm(caller)) or 0x2000
    region = pe.read(caller, size)
    if not region:
        print("ERROR: caller not mapped")
        return

    # find E8 rel32 calls to from_t
    sites = []
    for i in range(len(region) - 5):
        if region[i] == 0xE8:
            rel = struct.unpack_from("<i", region, i + 1)[0]
            cva = caller + i
            if cva + 5 + rel == from_t:
                sites.append(cva)
    if not sites:
        print(f"No E8 calls to {norm(from_t)} found in {norm(caller)} (size {size}).")
        return

    if args.at:
        want = int(args.at, 16)
        sites = [s for s in sites if s == want]
        if not sites:
            print(f"No call to {norm(from_t)} at {norm(want)}.")
            return
    elif len(sites) > 1:
        print(f"NOTE: {len(sites)} call sites to {norm(from_t)} in this function.")
        print(f"      Redirecting ALL of them. To target one, pass --at <call_va>.")
        print(f"      Sites: {[norm(s) for s in sites]}")
        print()

    # stack-balance check
    ri_from = ret_imm(pe, from_t)
    ri_to = ret_imm(pe, to_t)
    delta = (ri_from or 0) - (ri_to or 0)

    print(f"== redirect calls  {norm(from_t)} -> {norm(to_t)}  inside {norm(caller)} ==")
    print(f"from cleans {ri_from} bytes, to cleans {ri_to} bytes.  stack delta = {delta}")
    if delta != 0:
        print(f"!! STACK MISMATCH: target cleans {delta} fewer/more bytes.")
        print(f"   Caller pushes args for the OLD target. After redirect you must")
        print(f"   remove {abs(delta)//4} push(es) ({abs(delta)} bytes) before each call,")
        print(f"   e.g. NOP a `push imm` (6A xx -> 90 90).  Bytes just before the call shown below.")
    print()
    for cva in sites:
        newrel = struct.pack("<i", to_t - (cva + 5))
        oldbytes = pe.read(cva, 5)
        # show 10 bytes preceding for push detection (push imm8 = 6A xx, push imm32 = 68 ....)
        pre = pe.read(cva - 10, 10)
        print(f"  call @ {norm(cva)}  fo 0x{pe.fo(cva):x}")
        print(f"    {oldbytes.hex()}  ->  e8{newrel.hex()}")
        if delta > 0:
            hint = pre.hex()
            push_va = None
            for k in range(len(pre) - 1):
                if pre[k] == 0x6A:  # push imm8
                    push_va = cva - 10 + k
                    break
            print(f"    preceding 10 bytes: {hint}")
            if push_va is not None:
                print(f"    -> NOP the `push imm8` at {norm(push_va)} "
                      f"(fo 0x{pe.fo(push_va):x}): 6a?? -> 9090")
            else:
                print(f"    (no `push imm8` found in window — inspect manually for the arg push)")
    print()
    print("Emit these as patch edits (verify original bytes before writing, patch a COPY).")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("resolve", help="one-shot function dossier")
    r.add_argument("addr")
    r.add_argument("--source", action="store_true", help="dump full raw .c")
    r.set_defaults(func=cmd_resolve)

    d = sub.add_parser("redirect", help="find + retarget E8 calls, check stack balance")
    d.add_argument("caller")
    d.add_argument("from")
    d.add_argument("to")
    d.add_argument("--at", help="only redirect the call at this VA (else all sites)")
    d.set_defaults(func=cmd_redirect)

    a = ap.parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
