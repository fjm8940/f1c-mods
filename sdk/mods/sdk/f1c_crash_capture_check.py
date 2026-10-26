"""Validate f1c_crash_capture_test outputs: python -I f1c_crash_capture_check.py <test dir> <stdout file>"""
import glob, json, os, re, sys
d, out = sys.argv[1], open(sys.argv[2], encoding="utf-8", errors="replace").read()
fail = []
def need(c, m):
    print(("PASS " if c else "FAIL ") + m)
    if not c: fail.append(m)
need("probe done cb=0" in out, "IsBadReadPtr probe in kernelbase not reported")
need(re.search(r"CB ACCESS VIOLATION first-chance code 0xC0000005 image 3 offset 0x[0-9A-F]+ thread \d+ module 3 crc 0", out) is not None,
     "mod callback line: ntdll fault = image 3 module 3")
need("FILTER reached" in out, "chained unhandled filter reached the game's filter")
files = glob.glob(os.path.join(d, "plugins", "lightloader", "reports", "usage", "coop_mode-*.json"))
details = [json.load(open(f, encoding="utf-8")) for f in files]
details = [j for j in details if j.get("end") == "crash_detail"]
need(len(details) == 2, "two crash_detail summaries (first-chance + fatal), got %d" % len(details))
path_like = re.compile(r"[A-Za-z]:[\/]|\\|/(?:users|home)/", re.I)
for j in details:
    lines = j["lines"]
    need(j["schema"] == 1 and j["mod"] == "coop_mode" and j["version"] == "9.9.9", "envelope fields")
    need(len(lines) <= 200 and all(len(l) <= 200 for l in lines), "server line caps")
    text = json.dumps(j)
    need(not path_like.search(text), "no path-like content")
    need(not re.search(r"Matias|kamil|ntdll|\.dll|\.exe", text, re.I), "no names / module names")
    need(lines[0].startswith("fault code 0xC0000005 module 3 "), "fault line: " + lines[0][:90])
    need(any(l.startswith("regs eip 0x") for l in lines), "registers line")
    need(any(re.match(r"stack 0( \d+(\.\d+)?:0x[0-9A-F]+)+$", l) for l in lines), "stack frames as module:offset")
    need(any(re.search(r"stack .* 1:0x", l) for l in lines), "a stack frame inside the exe (main)")
    need(any(l.startswith("modules ") for l in lines), "loaded-module list")
need(any(j.get("fatal") == 1 for j in details) and any(j.get("fatal") == 0 for j in details), "fatal flag 0 and 1")
dumps = glob.glob(os.path.join(d, "plugins", "lightloader", "reports", "crash", "*.dmp"))
need(len(dumps) >= 1 and all(open(p, "rb").read(4) == b"MDMP" for p in dumps), "minidump(s) written with MDMP signature: %d" % len(dumps))
print("CRASH CAPTURE TEST", "FAIL" if fail else "PASS")
sys.exit(1 if fail else 0)
