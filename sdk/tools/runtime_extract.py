"""runtime_extract.py — deterministic RUNTIME extraction for the F1C corpus.

The static pipeline (skeleton_extract.py -> skeleton_facts_all.json) sees the flat
struct but NEVER the heap child objects those structs allocate at runtime (malloc'd,
non-deterministic addresses). This is the runtime analog: attach to the live game,
dissect a struct-array across all live instances, follow its heap children, and emit
runtime_facts.json.

TRUST: everything here is tier OBSERVED (below FACT). Structure is stable across
launches; absolute heap addresses are NOT. So facts are keyed by OFFSET-PATH
(veh+0x6A8 -> child+0x10), never by absolute address. Absolute addresses appear only
as ephemeral `sample_addr` for the current process and must never be promoted.

Usage:
  python runtime_extract.py --struct vehicle          # uses built-in vehicle spec
  python runtime_extract.py --base 0x5EC6D8 --stride 0x48C0 --count 104 \
        --live-off 0x32C8 --size 0x48C0 --name vehicle
Output: runtime_facts.json (merged per struct-name), runtime_structs/<name>.md

Structural extraction only (multi-instance classify + child graph + name bridge).
Behavior-delta tagging is a separate mode (snapshot->act->diff) — not in this pass.
"""
import ctypes, ctypes.wintypes as wt, struct, sys, json, os, argparse, time

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.dirname(HERE)
PROC = "F1 Challenge Online.exe"

# built-in struct specs (extend as globals get characterized)
SPECS = {
    "vehicle": dict(base=0x5EC6D8, stride=0x48C0, count=104, live_off=0x32C8,
                    live_sentinel=-1, size=0x48C0),
}

# ---- process access ----------------------------------------------------------
class PE(ctypes.Structure):
    _fields_=[('dwSize',wt.DWORD),('cntUsage',wt.DWORD),('th32ProcessID',wt.DWORD),
    ('th32DefaultHeapID',ctypes.POINTER(wt.ULONG)),('th32ModuleID',wt.DWORD),
    ('cntThreads',wt.DWORD),('th32ParentProcessID',wt.DWORD),('pcPriClassBase',wt.LONG),
    ('dwFlags',wt.DWORD),('szExeFile',ctypes.c_char*260)]
class MBI(ctypes.Structure):
    _fields_=[('BaseAddress',ctypes.c_void_p),('AllocationBase',ctypes.c_void_p),
    ('AllocationProtect',wt.DWORD),('RegionSize',ctypes.c_size_t),('State',wt.DWORD),
    ('Protect',wt.DWORD),('Type',wt.DWORD)]

class Proc:
    def __init__(self):
        self.k = ctypes.windll.kernel32
        snap=self.k.CreateToolhelp32Snapshot(2,0); e=PE(); e.dwSize=ctypes.sizeof(PE)
        pid=None
        if self.k.Process32First(snap,ctypes.byref(e)):
            while True:
                if e.szExeFile.decode(errors='replace').lower()==PROC.lower(): pid=e.th32ProcessID; break
                if not self.k.Process32Next(snap,ctypes.byref(e)): break
        self.k.CloseHandle(snap)
        if not pid: sys.exit(f"'{PROC}' not running")
        self.pid=pid
        self.h=self.k.OpenProcess(0x38|0x400,False,pid)
        self.regions=self._heap_regions()
    def rd(self,a,n):
        b=ctypes.create_string_buffer(n); g=ctypes.c_size_t()
        if not self.k.ReadProcessMemory(self.h,ctypes.c_void_p(a),b,n,ctypes.byref(g)): return None
        return b.raw[:g.value] if g.value else None
    def _heap_regions(self):
        regs=[]; addr=0; mbi=MBI()
        while addr < 0x7FFF0000:
            if not self.k.VirtualQueryEx(self.h,ctypes.c_void_p(addr),ctypes.byref(mbi),ctypes.sizeof(mbi)): break
            base=mbi.BaseAddress or 0
            if mbi.State==0x1000 and mbi.Type==0x20000: regs.append((base,base+mbi.RegionSize))
            addr=base+mbi.RegionSize
        return regs
    def in_heap(self,v):
        for lo,hi in self.regions:
            if lo<=v<hi: return True
        return False

# ---- classification ----------------------------------------------------------
def classify_flat(p, base, stride, size, live):
    blocks={s: p.rd(base+s*stride, size) for s in live}
    s0=blocks[live[0]]
    fields=[]; child_offs=[]
    for o in range(0,size,4):
        vals=[struct.unpack('<I',blocks[s][o:o+4])[0] for s in live]
        v0=vals[0]; distinct=len(set(vals))
        run=0
        while o+run<size and 32<=s0[o+run]<127: run+=1
        nheap=sum(p.in_heap(v) for v in vals)
        is_child = nheap>=len(live)//2 and distinct>=len(live)//2 and run<4
        if run>=4: kind="string"
        elif is_child: kind="child_ptr"; child_offs.append((o,vals))
        elif distinct==1: kind="static"
        else:
            f=struct.unpack('<f',struct.pack('<I',v0))[0]
            kind="float" if (f==f and 1e-6<abs(f)<1e9) else "int"
        fields.append(dict(off=f"0x{o:04X}", kind=kind, distinct=distinct,
                           sample_u32=f"0x{v0:08X}"))
    return blocks, fields, child_offs

def classify_child(p, vals, live, depth=0x80):
    heads={i: p.rd(vals[i],depth) for i in range(len(live))}
    ok=[i for i in heads if heads[i]]
    if not ok: return dict(kind="unreadable")
    nflt=nptr=ndyn=0
    for to in range(0,depth,4):
        col=[struct.unpack('<I',heads[i][to:to+4])[0] for i in ok if len(heads[i])>=to+4]
        if len(set(col))>1: ndyn+=1
        v0=col[0]
        if p.in_heap(v0): nptr+=1
        else:
            f=struct.unpack('<f',struct.pack('<I',v0))[0]
            if f==f and 1e-6<abs(f)<1e6: nflt+=1
    kind = "physics_float_buf" if nflt>nptr and nflt>=6 else ("ptr_list" if nptr>=4 else "mixed")
    return dict(kind=kind, floats=nflt, ptrs=nptr, dynamic=ndyn, sample_bytes=depth)

# ---- name bridge -------------------------------------------------------------
def load_vehicle_names():
    try: idx=json.load(open(os.path.join(CORPUS,"name_index.json")))
    except Exception: return {}
    # name_index vehicle fields may be under a 'vehicle_fields' key or globals; be lax
    out={}
    for k in ("vehicle_fields","fields","vehicle"):
        if isinstance(idx.get(k),dict):
            for off,meta in idx[k].items(): out[off.lower()]=meta
    return out

# ---- main --------------------------------------------------------------------
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--struct"); ap.add_argument("--name")
    ap.add_argument("--base"); ap.add_argument("--stride")
    ap.add_argument("--count",type=int); ap.add_argument("--live-off")
    ap.add_argument("--size"); ap.add_argument("--child-depth",default="0x80")
    a=ap.parse_args()
    if a.struct:
        spec=SPECS[a.struct]; name=a.struct
    else:
        spec=dict(base=int(a.base,0),stride=int(a.stride,0),count=a.count,
                  live_off=int(a.__dict__["live_off"],0),live_sentinel=-1,size=int(a.size,0))
        name=a.name or "unnamed"
    depth=int(a.child_depth,0)
    p=Proc()
    live=[s for s in range(spec["count"])
          if (v:=p.rd(spec["base"]+s*spec["stride"]+spec["live_off"],4))
          and struct.unpack('<i',v)[0]!=spec["live_sentinel"]]
    if len(live)<2: sys.exit(f"need >=2 live instances, got {len(live)}")
    print(f"[{name}] live instances: {len(live)}")
    blocks, fields, child_offs = classify_flat(p, spec["base"], spec["stride"], spec["size"], live)
    names=load_vehicle_names()
    for f in fields:
        if f["off"].lower() in names: f["name"]=names[f["off"].lower()].get("name")
    children=[]
    for o,vals in child_offs:
        info=classify_child(p, vals, live, depth)
        if info.get("kind") in ("unreadable","mixed") and info.get("floats",0)==0 and info.get("ptrs",0)==0:
            continue  # drop float-noise false positives
        children.append(dict(off=f"0x{o:04X}", **info,
                             sample_target=f"0x{vals[0]:08X}"))
    counts={}
    for f in fields: counts[f["kind"]]=counts.get(f["kind"],0)+1
    facts=dict(
        _meta=dict(struct=name, trust="OBSERVED",
                   note="offset-path keyed; sample_* are ephemeral, DO NOT promote as addresses",
                   base=f"0x{spec['base']:08X}", stride=f"0x{spec['stride']:04X}",
                   size=f"0x{spec['size']:04X}", live_instances=len(live),
                   captured=time.strftime("%Y-%m-%dT%H:%M:%S")),
        flat_field_counts=counts,
        real_children=children,
        fields=fields,
    )
    outj=os.path.join(CORPUS,"runtime_facts.json")
    allf={}
    if os.path.exists(outj):
        try: allf=json.load(open(outj))
        except Exception: allf={}
    allf[name]=facts
    json.dump(allf, open(outj,"w"), indent=1)
    print(f"flat fields: {counts}")
    print(f"real children (noise-filtered): {len(children)}")
    print(f"-> {outj} [{name}]")
    # human .md
    md=os.path.join(CORPUS,"runtime_structs"); os.makedirs(md,exist_ok=True)
    with open(os.path.join(md,f"{name}.md"),"w") as fp:
        fp.write(f"# runtime struct: {name}  (OBSERVED tier)\n\n")
        fp.write(f"base {facts['_meta']['base']} stride {facts['_meta']['stride']} "
                 f"size {facts['_meta']['size']} live {len(live)} captured {facts['_meta']['captured']}\n\n")
        fp.write(f"flat fields: {counts}\n\n## heap children ({len(children)})\n\n")
        fp.write("| off | kind | floats | ptrs | dynamic |\n|---|---|---|---|---|\n")
        for c in children:
            fp.write(f"| {c['off']} | {c.get('kind')} | {c.get('floats','')} | "
                     f"{c.get('ptrs','')} | {c.get('dynamic','')} |\n")
        fp.write("\n_Absolute addresses omitted by design (non-deterministic). "
                 "Names bridge from name_index.json where known._\n")
    print(f"-> runtime_structs/{name}.md")

if __name__=="__main__": main()
