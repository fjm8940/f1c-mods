# d1_static.py -- D1.1: static confirmation of AIComputePassScore (0x42A780).
#
# Read-only. Disassembles the function with capstone, prints the TireForceSolver
# call with its register setup (which object reaches ecx, what esi holds), scans
# every executable section for direct `call 0x42A780` sites, and scans the file
# for absolute dword pointers to 0x42A780 (indirect call tables). Raw log only.
import re
import struct
import sys
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
FUNC = 0x42A780
TFS = 0x4E8CE0


class Image:
    def __init__(self):
        self.pe = pefile.PE(str(EXE), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.data = EXE.read_bytes()
        self.sects = []
        for s in self.pe.sections:
            name = s.Name.rstrip(b'\x00').decode(errors='replace')
            self.sects.append((name, s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData),
                               s.PointerToRawData, s.Characteristics))

    def off(self, va):
        for name, va0, vsz, praw, ch in self.sects:
            if va0 <= va - self.base < vsz:
                return praw + (va - self.base - va0)
        raise ValueError(hex(va))

    def va(self, off):
        for name, va0, vsz, praw, ch in self.sects:
            if praw <= off < praw + vsz:
                return self.base + va0 + (off - praw)
        return None

    def bytes_at(self, va, n):
        o = self.off(va)
        return self.data[o:o + n]


def corpus_starts():
    out = []
    for p in (ROOT / 'functions').glob('*.c'):
        m = re.match(r'^(.+)_([0-9a-fA-F]{8})\.c$', p.name)
        if m:
            out.append((int(m.group(2), 16), m.group(1)))
    return sorted(out)


def containing(starts, va):
    import bisect
    i = bisect.bisect_right([a for a, _ in starts], va) - 1
    if i < 0:
        return None
    a, name = starts[i]
    return name if va - a < 0x4000 else None


def disasm(img, va, nbytes):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    return list(md.disasm(img.bytes_at(va, nbytes), va))


def main():
    img = Image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    starts = corpus_starts()

    print('=== AIComputePassScore 0x%X full disassembly (first 0x140 bytes) ===' % FUNC)
    for ins in disasm(img, FUNC, 0x140):
        mark = ''
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            c = int(ins.op_str, 16)
            if c == TFS:
                mark = '   <-- TireForceSolver'
        print('%08X  %-24s %s%s' % (ins.address, ins.mnemonic, ins.op_str, mark))

    print()
    print('=== TireForceSolver 0x%X full disassembly (first 0x420 bytes) ===' % TFS)
    for ins in disasm(img, TFS, 0x420):
        mark = ''
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            c = int(ins.op_str, 16)
            names = {0x5895A2: ' _frnd'}
            mark = names.get(c, '')
        print('%08X  %-24s %-38s %s%s' % (ins.address, ins.mnemonic, ins.op_str, ins.bytes.hex(), mark))

    print()
    print('=== direct call 0x%X sites (executable sections) ===' % FUNC)
    hits = []
    for name, va0, vsz, praw, ch in img.sects:
        if not (ch & 0x20000000):
            continue
        blob = img.data[praw:praw + vsz]
        for i in range(len(blob) - 5):
            if blob[i] == 0xE8:
                rel = struct.unpack_from('<i', blob, i + 1)[0]
                target = (img.base + va0 + i) + 5 + rel
                if target == FUNC:
                    hits.append((img.base + va0 + i, name))
    hits.sort()
    for va, sec in hits:
        print('call at 0x%08X (.%s) in %s' % (va, sec, containing(starts, va) or '?'))

    print()
    print('=== call-site context (12 before / 6 after) ===')
    for va, sec in hits:
        print('--- call at 0x%08X in %s ---' % (va, containing(starts, va) or '?'))
        ins = disasm(img, va - 0x60, 0x60 + 0x30)
        for x in ins:
            tag = ''
            if x.address == va:
                tag = '   <-- call AIComputePassScore'
            if x.mnemonic == 'call' and x.op_str.startswith('0x'):
                c = int(x.op_str, 16)
                if c == TFS:
                    tag = '   <-- TireForceSolver'
                elif c == 0x5895A2:
                    tag = '   <-- _frnd'
            print('%08X  %-24s %s%s' % (x.address, x.mnemonic, x.op_str, tag))
        print()

    print('=== absolute dword pointers to 0x%X anywhere in the image ===' % FUNC)
    pat = struct.pack('<I', FUNC)
    data = img.data
    pos = 0
    count = 0
    while True:
        i = data.find(pat, pos)
        if i < 0:
            break
        print('offset 0x%X -> VA %s' % (i, hex(img.va(i)) if img.va(i) else '?'))
        pos = i + 1
        count += 1
    print('total: %d' % count)


if __name__ == '__main__':
    main()
