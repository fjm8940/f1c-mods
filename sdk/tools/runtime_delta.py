"""runtime_delta.py — behavior attribution for the runtime corpus.

Tags which per-car offset-paths (flat fields AND heap-child bytes) move when a
specific behavior happens. This is how runtime fields get NAMED by cause instead
of guessed: capture -> cause the behavior in-game -> capture -> diff --label.

Two-shot (scriptable, no interactive prompt):
  python tools/runtime_delta.py capture before --struct vehicle
  # ... cause the behavior in the game (brake / pit / crash / retire) ...
  python tools/runtime_delta.py capture after  --struct vehicle
  python tools/runtime_delta.py diff before after --label braking --slot 0

diff records moved offset-paths into runtime_facts.json under
behavior_deltas[label] (OBSERVED tier, offset-path keyed). Child changes are
reported as veh+0xCHILD -> +0xINNER so they stay launch-independent.

Captures live in runtime_captures/<tag>.json (flat block + child-target bytes,
hex). Absolute addresses are NOT stored — only offsets and the bytes at them.
"""
import ctypes, ctypes.wintypes as wt, struct, sys, json, os, time, argparse

HERE=os.path.dirname(os.path.abspath(__file__)); CORPUS=os.path.dirname(HERE)
PROC="F1 Challenge Online.exe"
SPECS={"vehicle":dict(base=0x5EC6D8,stride=0x48C0,count=104,live_off=0x32C8,
                      live_sentinel=-1,size=0x48C0)}
CAPDIR=os.path.join(CORPUS,"runtime_captures"); os.makedirs(CAPDIR,exist_ok=True)

class PE(ctypes.Structure):
    _fields_=[('dwSize',wt.DWORD),('cntUsage',wt.DWORD),('th32ProcessID',wt.DWORD),
    ('th32DefaultHeapID',ctypes.POINTER(wt.ULONG)),('th32ModuleID',wt.DWORD),
    ('cntThreads',wt.DWORD),('th32ParentProcessID',wt.DWORD),('pcPriClassBase',wt.LONG),
    ('dwFlags',wt.DWORD),('szExeFile',ctypes.c_char*260)]
class MBI(ctypes.Structure):
    _fields_=[('BaseAddress',ctypes.c_void_p),('AllocationBase',ctypes.c_void_p),
    ('AllocationProtect',wt.DWORD),('RegionSize',ctypes.c_size_t),('State',wt.DWORD),
    ('Protect',wt.DWORD),('Type',wt.DWORD)]

def proc():
    k=ctypes.windll.kernel32
    snap=k.CreateToolhelp32Snapshot(2,0); e=PE(); e.dwSize=ctypes.sizeof(PE); pid=None
    if k.Process32First(snap,ctypes.byref(e)):
        while True:
            if e.szExeFile.decode(errors='replace').lower()==PROC.lower(): pid=e.th32ProcessID; break
            if not k.Process32Next(snap,ctypes.byref(e)): break
    k.CloseHandle(snap)
    if not pid: sys.exit("game not running")
    return k, k.OpenProcess(0x38|0x400,False,pid)

def rd(k,h,a,n):
    b=ctypes.create_string_buffer(n); g=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,ctypes.c_void_p(a),b,n,ctypes.byref(g)): return None
    return b.raw[:g.value] if g.value else None

def heap_regions(k,h):
    regs=[]; addr=0; mbi=MBI()
    while addr<0x7FFF0000:
        if not k.VirtualQueryEx(h,ctypes.c_void_p(addr),ctypes.byref(mbi),ctypes.sizeof(mbi)): break
        base=mbi.BaseAddress or 0
        if mbi.State==0x1000 and mbi.Type==0x20000: regs.append((base,base+mbi.RegionSize))
        addr=base+mbi.RegionSize
    return regs

def capture(tag, sname):
    spec=SPECS[sname]; k,h=proc(); regs=heap_regions(k,h)
    inheap=lambda v: any(lo<=v<hi for lo,hi in regs)
    live=[s for s in range(spec["count"])
          if (v:=rd(k,h,spec["base"]+s*spec["stride"]+spec["live_off"],4))
          and struct.unpack('<i',v)[0]!=spec["live_sentinel"]]
    cap={"struct":sname,"live":live,"slots":{}}
    for s in live:
        base=spec["base"]+s*spec["stride"]
        flat=rd(k,h,base,spec["size"])
        children={}
        for o in range(0,spec["size"],4):
            v=struct.unpack('<I',flat[o:o+4])[0]
            if inheap(v):
                tgt=rd(k,h,v,0x100)
                if tgt: children[f"0x{o:04X}"]=tgt.hex()
        cap["slots"][str(s)]={"flat":flat.hex(),"children":children}
    p=os.path.join(CAPDIR,f"{tag}.json"); json.dump(cap,open(p,"w"))
    print(f"captured '{tag}': {len(live)} live, "
          f"{sum(len(cap['slots'][str(s)]['children']) for s in live)} child snapshots -> {p}")

def diff(tagA,tagB,label,slot):
    A=json.load(open(os.path.join(CAPDIR,f"{tagA}.json")))
    B=json.load(open(os.path.join(CAPDIR,f"{tagB}.json")))
    slots=[slot] if slot is not None else A["live"]
    moved=[]
    for s in slots:
        sa=A["slots"].get(str(s)); sb=B["slots"].get(str(s))
        if not sa or not sb: continue
        fa=bytes.fromhex(sa["flat"]); fb=bytes.fromhex(sb["flat"])
        for o in range(0,min(len(fa),len(fb)),4):
            if fa[o:o+4]!=fb[o:o+4]:
                va=struct.unpack('<f',fa[o:o+4])[0]; vb=struct.unpack('<f',fb[o:o+4])[0]
                moved.append({"path":f"+0x{o:04X}","kind":"flat","slot":s,
                              "before":fa[o:o+4].hex(),"after":fb[o:o+4].hex(),
                              "f_before":f"{va:.4g}","f_after":f"{vb:.4g}"})
        for coff,hexa in sa["children"].items():
            hexb=sb["children"].get(coff)
            if hexb and hexa!=hexb:
                ca=bytes.fromhex(hexa); cb=bytes.fromhex(hexb)
                for io in range(0,min(len(ca),len(cb)),4):
                    if ca[io:io+4]!=cb[io:io+4]:
                        moved.append({"path":f"+{coff}->+0x{io:04X}","kind":"child",
                                      "slot":s,"before":ca[io:io+4].hex(),
                                      "after":cb[io:io+4].hex()})
    # dedupe paths across slots -> which paths moved and on how many cars
    from collections import Counter
    pc=Counter(m["path"] for m in moved)
    summary=[{"path":p,"cars":c} for p,c in pc.most_common()]
    outj=os.path.join(CORPUS,"runtime_facts.json")
    allf=json.load(open(outj)) if os.path.exists(outj) else {}
    veh=allf.setdefault(A["struct"],{})
    veh.setdefault("behavior_deltas",{})[label]={
        "captured":time.strftime("%Y-%m-%dT%H:%M:%S"),"trust":"OBSERVED",
        "slots_examined":slots,"moved_paths":summary,"samples":moved[:200]}
    json.dump(allf,open(outj,"w"),indent=1)
    print(f"[{label}] moved offset-paths: {len(summary)} distinct across {len(slots)} slot(s)")
    for r in summary[:40]:
        ex=next(m for m in moved if m["path"]==r["path"])
        extra=f" {ex.get('f_before','')}->{ex.get('f_after','')}" if ex["kind"]=="flat" else ""
        print(f"  {r['path']:26s} {ex['kind']:5s} x{r['cars']}{extra}")
    print(f"-> runtime_facts.json [{A['struct']}].behavior_deltas.{label}")

def main():
    ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest="cmd",required=True)
    c=sub.add_parser("capture"); c.add_argument("tag"); c.add_argument("--struct",default="vehicle")
    d=sub.add_parser("diff"); d.add_argument("tagA"); d.add_argument("tagB")
    d.add_argument("--label",required=True); d.add_argument("--slot",type=int)
    a=ap.parse_args()
    if a.cmd=="capture": capture(a.tag,a.struct)
    else: diff(a.tagA,a.tagB,a.label,a.slot)

if __name__=="__main__": main()
