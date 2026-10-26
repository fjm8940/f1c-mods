"""Extended Damage Commentary 1.0.4 usage summary (edc_usage.h) replay test.
Proves BOTH write paths: the worker session-end pump (edcu_request/edcu_pump)
and the shared VEH crash summary (exactly one per process)."""
import glob, json, os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(HERE, "..", "..", "..", "..", "f1c_ida_decompiled", "mods", "sdk")
GCC = r"C:\msys64\mingw32\bin\gcc.exe"
C = r'''
#include <stdio.h>
#include <string.h>
#include "edc_usage.h"
int main(int argc,char**argv){(void)argc;FILE*f=fopen(argv[1],"rb");static char line[512];int n=0;
 if(!f)return 2;
 while(fgets(line,sizeof line,f)){size_t k=strlen(line);while(k&&(line[k-1]=='\n'||line[k-1]=='\r'))line[--k]=0;edcu_tap(line);n++;}
 fclose(f);
 g_edcu.start_tick=GetTickCount()-120000;
 edcu_request(); edcu_pump();          /* session-end: game-thread flag -> worker I/O */
 edcu_veh_install();
 EXCEPTION_RECORD rec; memset(&rec,0,sizeof rec);
 EXCEPTION_POINTERS ep; memset(&ep,0,sizeof ep);
 ep.ExceptionRecord=&rec;
 rec.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
 rec.ExceptionAddress=(PVOID)(uintptr_t)0x12345000u; f1c_usage_veh_cb(&ep);
 rec.ExceptionAddress=(PVOID)&GetTickCount;         f1c_usage_veh_cb(&ep);
 rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep);
 rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep);
 rec.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;   f1c_usage_veh_cb(&ep);
 printf("%d\n",n);return 0;}
'''

def main():
    work = tempfile.mkdtemp(prefix="edc_usage_")
    game = os.path.join(work, "game")
    os.makedirs(os.path.join(game, "plugins", "lightloader"))
    src = os.path.join(work, "t.c"); open(src, "w").write(C)
    exe = os.path.join(game, "t.exe")
    subprocess.check_call([GCC, "-O2", "-std=c11", "-Wall", "-Wextra", "-Wno-unused-function",
                           "-I", HERE, "-I", SDK, src, "-o", exe])
    feed = os.path.join(work, "feed.txt")
    lines = [
        "extended_damage_commentary: 6/6 sites active",
        "extended_damage_commentary: refused; executable layout is unavailable or a site is changed",
        r"extended_damage_commentary: %USERPROFILE%\Documents\x.veh",
    ] * 4
    open(feed, "w", encoding="utf-8").write("\n".join(lines))
    subprocess.check_call([exe, feed])
    usage = os.path.join(game, "plugins", "lightloader", "reports", "usage")
    files = sorted(os.listdir(usage))
    fails = []
    if len(files) != 2:
        fails.append("expected 2 summaries (session_end+crash), got %d" % len(files))
    else:
        ends = {}
        for name in files:
            raw = open(os.path.join(usage, name), "rb").read()
            text = raw.decode("utf-8")
            j = json.loads(text)
            if j.get("schema") != 1 or j.get("mod") != "extended_damage_commentary" or j.get("version") != "1.0.5":
                fails.append("schema/mod/version in " + name)
            if len(raw) > 40 * 1024:
                fails.append("size %d in %s" % (len(raw), name))
            if "<s>" in text or "<path>" in text:
                fails.append("placeholder leaked in " + name)
            for bad in (r"[A-Za-z]:\\", r"\\", "Users", "Matias", r"\.veh", r"\.ini"):
                if re.search(bad, text, re.I):
                    fails.append("leak in %s: %s" % (name, bad))
            ends[j.get("end")] = (name, j, len(raw))
        if "session_end" not in ends:
            fails.append("no session_end summary written by the worker pump")
        if "exception" not in ends:
            fails.append("no crash summary")
        if "session_end" in ends:
            name, j, size = ends["session_end"]
            # 1.0.4: every kept line is <EDC-Ecode> plus numeric values; never <s>.
            if j["lines"] and not all(re.match(r"^EDC-E[0-9A-F]{8}( \S+)*$", ln)
                                      for ln in j["lines"]):
                fails.append("session_end lines not coded: " + repr(j["lines"][:3]))
            if not any(re.search(r"^EDC-E[0-9A-F]{8} .*[0-9]", ln) for ln in j["lines"]):
                fails.append("session_end no coded line kept a number")
            print("session_end size", size, "lines", len(j["lines"]),
                  "counts", j["counts"])
    # E-a: DllMain must contain no flush call (loader lock / dead threads).
    src = open(os.path.join(HERE, "extended_damage_commentary.c"), encoding="utf-8").read()
    dm = src[src.index("BOOL WINAPI DllMain"):]
    if "edcu_flush" in dm:
        fails.append("DllMain must not call edcu_flush")
    # R2: the game-thread callback must only set the request flag.
    se = src[src.index("static void __cdecl edc_session_end"):]
    se = se[:se.index("}")]
    if "edcu_flush" in se:
        fails.append("edc_session_end must not call edcu_flush (worker pump only)")
    shutil.rmtree(work, ignore_errors=True)
    print("EDC USAGE TEST:", "PASS" if not fails else "FAIL " + "; ".join(fails))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
