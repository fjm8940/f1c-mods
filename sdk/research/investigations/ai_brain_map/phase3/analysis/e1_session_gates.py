# e1_session_gates.py -- E1: session-global 0x7C56A4 branch census + 0x44C0 consumer search.
#
# Read-only. Capstone disassembly of the game exe, one slice per functions/*.c start (reliable
# alignment; a whole-.text linear sweep with skipdata desyncs and misses e.g. ServerSessionGateCheck).
#  (1) every memory operand whose absolute displacement is 0x7C56A4 (session-type global):
#      read vs write, containing function, compared value when statically visible.
#  (2) every memory operand with displacement 0x44C0 on any base register (QUALIFYINGABILITY):
#      read vs write.
# "AI set" = functions with a map in investigations/ai_brain_map/ (root), phase2/maps/,
# phase3/maps/, or a phase-1/2 named AI core function.
#
# No file is modified; stdout is redirected to %TEMP%.
import bisect
import re
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_REG, X86_OP_IMM

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
HANDOFF_ROOT = ROOT / 'investigations' / 'ai_brain_map'
SESSION_GLOBAL = 0x7C56A4
QUALI_ABILITY = 0x44C0

CORE_AI = {
    'AIMainUpdate', 'AISteering', 'AISteeringCorrection', 'AIRacingLine',
    'AIRacingLinePathFollowing', 'AISteeringRacingLineCorrection', 'AIAbility',
    'AIPushLap', 'AIComputePassScore', 'AIOvertake', 'Overtaking', 'sub_4258C0',
    'sub_426100', 'sub_426430', 'sub_429960', 'SteeringAngleController',
    'TireForceSolver', 'TireForce', 'BrakeThermalModel', 'PhysicsGearShiftLogicWithThresholds',
    'AIBehaviorInitialization', 'AIClearStateFlags', 'AICollision', 'AIDifficultyScalar',
    'AIStateFlagSetter', 'RacePhasePitEntryTrigger', 'ForcePitExit', 'PitStopDecisionTireCompound',
    'PostPitCleanup', 'RandomWeightedTierSelection', 'RandLCGRandomNumberGenerator',
    'ConditionalGate', 'ServerSessionGateCheck', 'ReplayAIStateCheck', 'AICommandMessageQueueDispatcher',
    'FuelMultiplierValue', 'FuelWeightLimitCheck', 'AICornerSpeedComparison', 'AIDriverSorting',
    'AIOvertakeDefendStateChange', 'AIPositionChangeEvent', 'AISessionPhaseChange',
    'AIDriverStateTransition', 'AIFormationLap', 'PitStopBlockingCondition', 'sub_4BE760',
}


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
    return base, data, off, pe


def corpus():
    out = {}
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if m:
            out[int(m.group(2), 16)] = (m.group(1), p)
    return out


def map_names():
    names = set()
    for d in (HANDOFF_ROOT, HANDOFF_ROOT / 'phase2' / 'maps', HANDOFF_ROOT / 'phase3' / 'maps'):
        if not d.is_dir():
            continue
        for p in d.glob('map_*.md'):
            n = re.sub(r'_part\d+$', '', p.stem[4:])
            names.add(n)
    return names


def collect(insns, target_disp, absolute_only):
    hits = []
    for idx, insn in enumerate(insns):
        if insn.id == 0:  # skipdata
            continue
        for oi, op in enumerate(insn.operands):
            if op.type != X86_OP_MEM or op.mem.disp != target_disp:
                continue
            if absolute_only:
                if op.mem.base != 0 or op.mem.index != 0:
                    continue
            else:
                if op.mem.base == 0 and op.mem.index == 0:
                    continue
            access = 'read'
            m = insn.mnemonic
            if oi == 0 and m.startswith(('mov', 'fst', 'fild')) and not m.startswith('movs'):
                access = 'write'
            elif oi == 0 and m in ('and', 'or', 'xor', 'add', 'sub', 'inc', 'dec', 'shl', 'shr', 'imul'):
                access = 'rmw'
            hits.append((idx, insn, oi, access))
    return hits


def main():
    base, data, off, pe = load()
    corp = corpus()
    maps = map_names()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    text = None
    for s in pe.sections:
        if s.Name.rstrip(b'\x00') == b'.text':
            text = s
    text_lo = base + text.VirtualAddress
    text_hi = text_lo + max(text.Misc_VirtualSize, text.SizeOfRawData)
    starts = sorted(a for a in corp if text_lo <= a < text_hi)
    print('== per-function capstone slices: %d corpus functions, %d inside .text =='
          % (len(corp), len(starts)))

    def is_ai(fname):
        return fname in maps or fname in CORE_AI or fname.startswith('AI') or fname.startswith('sub_42')

    def slices():
        for i, st in enumerate(starts):
            end = starts[i + 1] if i + 1 < len(starts) else st + 0x4000
            if end - st > 0x10000:
                end = st + 0x10000
            yield st, end, corp[st][0]

    def describe(insn):
        return '%08X  %-22s %s %s' % (insn.address, insn.bytes.hex()[:20], insn.mnemonic, insn.op_str)

    def context(insns, idx, insn, access):
        ctx = []
        if insn.mnemonic in ('cmp', 'test') and insn.operands[0].type == X86_OP_MEM \
                and len(insn.operands) > 1 and insn.operands[1].type == X86_OP_IMM:
            ctx.append('IMMEDIATE_CMP=%d' % insn.operands[1].imm)
        dreg = None
        if access == 'read' and insn.operands[0].type == X86_OP_REG:
            dreg = insn.operands[0].reg
        if dreg is not None:
            for j in range(idx + 1, min(idx + 4, len(insns))):
                nx = insns[j]
                if nx.mnemonic in ('cmp', 'test') and nx.operands[0].type == X86_OP_REG \
                        and nx.operands[0].reg == dreg and len(nx.operands) > 1 \
                        and nx.operands[1].type == X86_OP_IMM:
                    ctx.append('cmp_next=%d' % nx.operands[1].imm)
                    break
                if nx.mnemonic.startswith(('j', 'ret')):
                    break
        return ' '.join(ctx)

    for target, absolute_only, label in (
            (SESSION_GLOBAL, True, 'session-type global [0x%X] (absolute)' % SESSION_GLOBAL),
            (QUALI_ABILITY, False, 'QUALIFYINGABILITY [base+0x%X]' % QUALI_ABILITY)):
        print()
        print('== memory operands touching %s ==' % label)
        n_read = n_write = n_rmw = 0
        for st, end, fname in slices():
            try:
                o = off(st)
                chunk = data[o:o + (end - st)]
                insns = list(md.disasm(chunk, st))
            except Exception:
                continue
            for idx, insn, oi, access in collect(insns, target, absolute_only):
                if access == 'read':
                    n_read += 1
                elif access == 'write':
                    n_write += 1
                else:
                    n_rmw += 1
                ai = 'AI' if is_ai(fname) else '--'
                ctx = context(insns, idx, insn, access)
                print('%s %s  in %-34s %-5s %s' % (ai, describe(insn), fname[:34], access.upper(), ctx))
        print('total hits: read=%d write=%d rmw=%d' % (n_read, n_write, n_rmw))


if __name__ == '__main__':
    main()
