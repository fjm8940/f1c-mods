# e3_guess_static.py -- E3: static evidence dump for the 36 v1.3 GUESS fields.
#
# Read-only. For each field: (a) capstone hits in the game exe (memory operand displacement == offset,
# any base) with access classification, containing function and the previous instruction;
# (b) all functions/*.c lines that reference the offset as decimal byte, hex, or a this[N]/[N]
# dword index (N = offset/4). Intended as input for manual meaning/tier proposals.
import re
import struct
import sys
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')

FIELDS = [
    ('placement_pose_x', 0x34D4), ('placement_pose_y', 0x34D8), ('placement_pose_z', 0x34DC),
    ('opp_slot_vehicle_ptr', 0x4174), ('opp_slot_distance_sq', 0x41A8), ('opp_team_threshold', 0x42DC),
    ('opp_state', 0x42D8), ('unnamed_4000', 0x4000), ('unnamed_4040', 0x4040),
    ('unnamed_413c', 0x413C), ('unnamed_4170', 0x4170), ('outside_corridor_flag', 0x4140),
    ('unnamed_4364', 0x4364), ('unnamed_43f4', 0x43F4), ('unnamed_4400', 0x4400),
    ('unnamed_441c', 0x441C), ('unnamed_4444', 0x4444), ('time_margin', 0x4448),
    ('unnamed_444c', 0x444C), ('unnamed_4454', 0x4454), ('unnamed_4458', 0x4458),
    ('unnamed_4460', 0x4460), ('unnamed_4490', 0x4490), ('command_queue_write_index', 0x44B0),
    ('unnamed_4524', 0x4524), ('unnamed_4528', 0x4528), ('unnamed_452c', 0x452C),
    ('unnamed_4554', 0x4554), ('unnamed_455c', 0x455C), ('unnamed_4560', 0x4560),
    ('unnamed_456c', 0x456C), ('unnamed_4570', 0x4570), ('unnamed_4574', 0x4574),
    ('unnamed_4590', 0x4590), ('unnamed_45e8', 0x45E8), ('unnamed_461c', 0x461C),
]


def load():
    pe = pefile.PE(str(EXE), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    data = EXE.read_bytes()

    def off(va):
        for s in pe.sections:
            if s.VirtualAddress <= va - base < max(s.Misc_VirtualSize, s.SizeOfRawData):
                return s.PointerToRawData + (va - base - s.VirtualAddress)
        raise ValueError(hex(va))
    text = [s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text'][0]
    return base, data, off, text


def corpus():
    out = {}
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if m:
            out[int(m.group(2), 16)] = p
    return out


def main():
    base, data, off, text = load()
    corp = corpus()
    starts = sorted(corp)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    targets = {o: name for name, o in FIELDS}
    i = 0
    while i < len(starts):
        st = starts[i]
        end = starts[i + 1] if i + 1 < len(starts) else st + 0x4000
        if end - st > 0x10000:
            end = st + 0x10000
        i += 1
        try:
            o = off(st)
            insns = list(md.disasm(data[o:o + (end - st)], st))
        except Exception:
            continue
        fname = corp[st].stem.rsplit('_', 1)[0]
        for j, insn in enumerate(insns):
            if insn.id == 0:
                continue
            for oi, op in enumerate(insn.operands):
                if op.type != X86_OP_MEM or op.mem.disp not in targets:
                    continue
                tgt = op.mem.disp
                access = 'R'
                m = insn.mnemonic
                if oi == 0 and m.startswith(('mov', 'fst', 'fild')) and not m.startswith('movs'):
                    access = 'W'
                elif oi == 0 and m in ('and', 'or', 'xor', 'add', 'sub', 'inc', 'dec', 'shl', 'shr'):
                    access = 'RW'
                prev = insns[j - 1] if j else None
                pv = ('%s %s' % (prev.mnemonic, prev.op_str))[:42] if prev else '-'
                print('CAP %-26s %s %08X %-26s %s %s | prev: %s'
                      % (targets[tgt], access, insn.address, fname[:26], insn.mnemonic,
                         insn.op_str, pv))

    print()
    print('== C-source references (decimal byte / hex / dword index) ==')
    pats = {}
    for name, o in FIELDS:
        toks = ['+ %d]' % o, '+ %d)' % o, '+ %d ' % o, ' %d, ' % o, '(%d)' % o,
                hex(o), hex(o).upper(), '0x%x' % o]
        if o % 4 == 0:
            toks += ['[%d]' % (o // 4), ' + %d]' % (o // 4), '(%d)' % (o // 4)]
        pats[name] = (o, toks)
    for p in sorted(corp.values()):
        txt = p.read_text(encoding='utf-8', errors='replace').splitlines()
        for ln, line in enumerate(txt, 1):
            for name, (o, toks) in pats.items():
                for t in toks:
                    if t in line:
                        print('SRC %-26s %s:%d: %s' % (name, p.name, ln, line.strip()[:150]))
                        break
                else:
                    continue
                break


if __name__ == '__main__':
    main()
