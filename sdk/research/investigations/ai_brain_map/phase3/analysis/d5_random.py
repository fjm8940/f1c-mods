# d5_random.py -- D5: RandLCG call-site census (capstone) + RandomVariationScaler disassembly.
#
# Read-only. Loads the game exe, disassembles .text, finds every direct `call 0x5884ED`
# (RandLCGRandomNumberGenerator) and every call to the known wrappers
# (RandomVariationScaler 0x4BEA10, RandomWeightedTierSelection 0x429FD0,
# AIPathPerformanceFunction 0x4BFA00, AIOpponentSelection 0x4BF790).
# Containing function = nearest functions/*.c start <= call address; the matching C lines
# (in file order) are printed for context. Also disassembles RandomVariationScaler 0x4BEA10
# and the CRT helper sub_5881F0 (REVIEW_15 dropped-return question) with following sub_588212.
import bisect
import re
import sys
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
RAND = 0x5884ED
WRAPPERS = {0x4BEA10: 'RandomVariationScaler', 0x429FD0: 'RandomWeightedTierSelection',
            0x4BFA00: 'AIPathPerformanceFunction', 0x4BF790: 'AIOpponentSelection'}
AI_FUNCS = {'AIMainUpdate', 'AIAbility', 'sub_429960', 'sub_424080', 'RandomVariationScaler',
            'RandomWeightedTierSelection', 'AIOpponentSelection', 'AIPathPerformanceFunction',
            'CarCrashDamageProcessing', 'PitExit', 'FuelConsumptionAndWear', 'AIBehaviorInitialization',
            'AIPushLap', 'RacePhasePitEntryTrigger', 'PitStopDecisionTireCompound'}


def load():
    pe = pefile.PE(str(EXE), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sects = [(s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData), s.PointerToRawData)
             for s in pe.sections]
    data = EXE.read_bytes()

    def off(va):
        for va0, sz, praw in sects:
            if va0 <= va - base < sz:
                return praw + (va - base - va0)
        raise ValueError(hex(va))
    return base, data, off


def corpus():
    out = {}
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if m:
            out[int(m.group(2), 16)] = (m.group(1), p)
    return out


def c_lines(path, token):
    out = []
    for i, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
        if token in line:
            out.append((i, line.strip()))
    return out


def main():
    base, data, off = load()
    corp = corpus()
    starts = sorted(corp)
    md = Cs(CS_ARCH_X86, CS_MODE_32)

    def fend(va):
        i = bisect.bisect_right(starts, va)
        return starts[i] if i < len(starts) else va + 0x2000

    def func_of(va):
        i = bisect.bisect_right(starts, va) - 1
        return (corp[starts[i]][0], starts[i]) if i >= 0 else ('?', 0)

    # full .text disassembly
    text = None
    for s in pefile.PE(str(EXE), fast_load=True).sections:
        if s.Name.rstrip(b'\x00') == b'.text':
            text = s
    assert text is not None
    code = data[text.PointerToRawData:text.PointerToRawData + text.SizeOfRawData]
    md.skipdata = True  # linear sweep must not stop at the first undecodable byte
    insns = list(md.disasm(code, base + text.VirtualAddress))

    print('== RandLCG call-site census (direct `call 0x%X`) ==' % RAND)
    n_rand = 0
    for insn in insns:
        if insn.mnemonic != 'call' or not insn.op_str.startswith('0x'):
            continue
        tgt = int(insn.op_str, 16)
        if tgt != RAND:
            continue
        n_rand += 1
        fname, fstart = func_of(insn.address)
        src = corp.get(fstart, ('?', None))[1]
        lines = c_lines(src, 'RandLCGRandomNumberGenerator') if src else []
        ai = 'AI' if fname in AI_FUNCS else '  '
        print('%s %08X %-36s caller-relative +0x%-4X  c-lines=%s'
              % (ai, insn.address, fname[:36], insn.address - fstart, [l for l, _ in lines]))
        if fname in AI_FUNCS and src:
            for l, line in lines:
                print('        L%-4d %s' % (l, line[:110]))
    print('total RandLCG direct call sites: %d' % n_rand)

    print()
    print('== callers of the random wrappers ==')
    for waddr, wname in WRAPPERS.items():
        hits = [insn.address for insn in insns
                if insn.mnemonic == 'call' and insn.op_str.startswith('0x')
                and int(insn.op_str, 16) == waddr]
        print('  %s @0x%X: %d call sites' % (wname, waddr, len(hits)))
        for h in hits:
            fname, fstart = func_of(h)
            print('     %08X in %s' % (h, fname))

    print()
    print('== disassembly RandomVariationScaler 0x4BEA10 (REVIEW_15 dropped-return check) ==')
    va = 0x4BEA10
    for insn in md.disasm(data[off(va):off(va) + (fend(va) - va)], va):
        print('  %08X  %-24s %s %s' % (insn.address, insn.bytes.hex(), insn.mnemonic, insn.op_str))

    print()
    print('== disassembly sub_5881F0 0x5881F0 / sub_588212 ==')
    for va in (0x5881F0, 0x588212):
        end = fend(va)
        print('-- 0x%X..0x%X' % (va, end))
        for insn in md.disasm(data[off(va):off(va) + (end - va)], va):
            print('  %08X  %-24s %s %s' % (insn.address, insn.bytes.hex(), insn.mnemonic, insn.op_str))


if __name__ == '__main__':
    main()
