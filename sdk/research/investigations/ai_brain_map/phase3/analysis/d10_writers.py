# d10_writers.py -- D10: every store in the exe .text whose memory displacement is one of
# 0x4408 / 0x43FC (=0x4408-0xC) / 0x44B4 / 0x44A8 (=0x44B4-0xC), any base register.
# For each hit: instruction, containing function (nearest functions/*.c start), and a backward
# trace (<= 20 instructions) to the last write of the base register. Read-only.
import bisect
import re
import sys
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_REG

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
DISPS = {0x4408: 'published_lateral_target', 0x43FC: 'braking_target_speed OR veh+0xC+0x4408',
         0x44B4: 'steering_lock_divisor', 0x44A8: 'command_queue_array OR veh+0xC+0x44B4'}
STORE_MN = ('mov', 'movss', 'fstp', 'fst', 'movsd', 'movups', 'movaps')
WRITE_MN = ('mov', 'lea', 'add', 'sub', 'pop', 'xor', 'and', 'or', 'imul', 'shl', 'shr', 'inc', 'dec',
            'movzx', 'movsx')


def corpus():
    out = {}
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if m:
            out[int(m.group(2), 16)] = m.group(1)
    return out


def main():
    pe = pefile.PE(str(EXE))
    data = EXE.read_bytes()
    base = pe.OPTIONAL_HEADER.ImageBase

    def off(va):
        return pe.get_offset_from_rva(va - base)

    text = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    md.skipdata = True
    code = data[text.PointerToRawData:text.PointerToRawData + text.SizeOfRawData]
    insns = list(md.disasm(code, base + text.VirtualAddress))
    starts = sorted(corpus())

    def func_of(va):
        i = bisect.bisect_right(starts, va) - 1
        return corpus()[starts[i]] if i >= 0 else '?'

    idx_of = {insn.address: i for i, insn in enumerate(insns)}
    print('== D10 store scan: disps 0x4408 / 0x43FC / 0x44B4 / 0x44A8 (%d instructions) =='
          % len(insns))
    hits = 0
    for insn in insns:
        if insn.mnemonic not in STORE_MN or getattr(insn, 'id', 0) == 0:
            continue
        try:
            ops = insn.operands
        except Exception:
            continue
        if not ops:
            continue
        mem = None
        for op in ops:
            if op.type == X86_OP_MEM and op.mem.disp in DISPS:
                mem = op
                break
        if mem is None:
            continue
        hits += 1
        fn = func_of(insn.address)
        dst = insn.op_str.split(',')[0].strip()
        print()
        print('%08X  %-28s %s %s   [%s]' % (insn.address, fn, insn.mnemonic, insn.op_str,
                                            DISPS[mem.mem.disp]))
        # trace: last write to the base register before this instruction
        i = idx_of[insn.address]
        base_reg = insn.reg_name(mem.mem.base) if mem.mem.base else None
        index_reg = insn.reg_name(mem.mem.index) if mem.mem.index else None
        print('    base=%s index=%s scale=%d disp=0x%X' % (base_reg, index_reg, mem.mem.scale,
                                                           mem.mem.disp))
        if base_reg:
            found = 0
            for j in range(i - 1, max(-1, i - 21), -1):
                p = insns[j]
                if p.mnemonic not in WRITE_MN or getattr(p, 'id', 0) == 0:
                    continue
                try:
                    pops = p.operands
                except Exception:
                    continue
                if pops:
                    d = pops[0]
                    if d.type == X86_OP_REG and p.reg_name(d.reg) == base_reg:
                        print('    trace: %08X  %s %s' % (p.address, p.mnemonic, p.op_str))
                        found += 1
                        if found >= 3:
                            break
    print()
    print('total store hits: %d' % hits)


if __name__ == '__main__':
    main()
