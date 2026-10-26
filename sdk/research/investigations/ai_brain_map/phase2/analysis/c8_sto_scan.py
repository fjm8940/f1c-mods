# c8_sto_scan.py — C8.4: scan every mapped AI function for the dropped-x87-return pattern
# (call to a float/double-returning function whose Hex-Rays result is dropped, while the
# assembly consumes ST0 right after: `call X; fmul ...` / `fstp ...`).
# Read-only: PE image + corpus .c files + map addresses. Prints a TSV for review.
import re
import sys
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
MAP_DIRS = [ROOT / 'investigations' / 'ai_brain_map', ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'maps']

CONSUME = ('fmul', 'fadd', 'fsub', 'fsubr', 'fdiv', 'fdivr', 'fcom', 'fcomp', 'fucom', 'fucomp',
           'fchs', 'fabs', 'frndint', 'fsqrt', 'fst', 'fstp')
NON_CONSUME = ('fld', 'fild', 'fnstsw', 'fnstcw', 'fldcw', 'fwait', 'fdecstp', 'fincstp')


def load():
    pe = pefile.PE(EXE, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sects = [(s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData), s.PointerToRawData) for s in pe.sections]
    data = EXE.read_bytes()

    def off(va):
        for va0, sz, praw in sects:
            if va0 <= va - base < sz:
                return praw + (va - base - va0)
        raise ValueError(hex(va))
    return base, data, off


def corpus():
    """addr -> (name, returns_fp, path)"""
    out = {}
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if not m:
            continue
        addr = int(m.group(2), 16)
        head = p.read_text(encoding='utf-8', errors='replace').splitlines()[:12]
        ret = None
        for line in head:
            mm = re.match(r'\s*(double|float|long double)\s+(?:__\w+\s+)?([A-Za-z_]\w*)\s*\(', line)
            if mm:
                ret = mm.group(1)
                break
        out[addr] = (m.group(1), ret, p)
    return out


def mapped():
    addrs = {}
    pat = re.compile(r'^- Address: `0x([0-9a-fA-F]+)`', re.M)
    for d in MAP_DIRS:
        for p in d.glob('map_*.md'):
            txt = p.read_text(encoding='utf-8', errors='replace')
            m = pat.search(txt)
            fm = re.search(r'file `functions/([^`]+)`', txt)
            if m:
                addrs[int(m.group(1), 16)] = (p.name, fm.group(1) if fm else None)
    return addrs


def scan_func(base, data, off, va, end):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    nbytes = max(16, min(0x8000, end - va))
    code = data[off(va):off(va) + nbytes]
    ins = list(md.disasm(code, va))
    calls = []
    for i, insn in enumerate(ins):
        if insn.mnemonic != 'call' or not insn.op_str.startswith('0x'):
            continue
        callee = int(insn.op_str, 16)
        cons = None
        for j in range(i + 1, min(i + 8, len(ins))):
            m = ins[j].mnemonic
            if m in ('call', 'jmp', 'ret', 'retf') or m.startswith('j') or m.startswith('loop'):
                break
            core = m.split()[0]
            if core in NON_CONSUME or core.startswith('fnst') or core.startswith('fld'):
                continue
            if core in CONSUME:
                cons = ins[j]
                break
        calls.append((insn.address, callee, cons, ins))
    return calls, ins


def main():
    base, data, off = load()
    corp = corpus()
    maps = mapped()
    starts = sorted(corp)
    import bisect

    def func_end(va):
        i = bisect.bisect_right(starts, va)
        return starts[i] if i < len(starts) else va + 0x1000

    print('mapped functions: %d; corpus functions: %d' % (len(maps), len(corp)))
    print('function\tcall_va\tcallee\tcallee_ret\tafter\tc_uses_return\tc_line')
    for va in sorted(maps):
        fname, srcfile = maps[va]
        src = None
        if srcfile:
            fp = ROOT / 'functions' / srcfile
            src = fp.read_text(encoding='utf-8', errors='replace') if fp.exists() else None
        calls, ins = scan_func(base, data, off, va, func_end(va))
        for call_va, callee, cons, insn in calls:
            if callee not in corp:
                continue
            cname, cret, cpath = corp[callee]
            if not cret:
                continue
            used = None
            if src:
                pat = re.compile(r'=\s*' + re.escape(cname) + r'\s*\(')
                used = bool(pat.search(src))
            if cons is not None and used is False:
                line = ''
                if src:
                    for ln in src.splitlines():
                        if (cname + '(') in ln and '= ' + cname not in ln:
                            line = ln.strip()[:90]
                            break
                print('%s\t%08x\t%s@%x\t%s\t%s\tno\t%s' %
                      (fname, call_va, cname, callee, cret, '%s %s' % (cons.mnemonic, cons.op_str), line))


if __name__ == '__main__':
    main()
