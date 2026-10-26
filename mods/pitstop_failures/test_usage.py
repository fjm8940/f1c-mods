"""Pit Stop Failures 1.0.9 usage summary (pit_usage.h) replay test.

Two real write paths are exercised against the source header:
  1. session end: pitu_request() -> pitu_pump() on the worker loop (the 1.0.9
     fix for "1.0.8 wrote no usage file on a normal return to the menu").
  2. crash: the mod's own VEH on an access violation inside the module image.

Build output goes to %TEMP%. Validates the JSON envelope and words-only content.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "source")
SDK = os.path.join(HERE, "..", "..", "..", "..", "..", "f1c_ida_decompiled", "mods", "sdk")
GCC = r"C:\msys64\mingw32\bin\gcc.exe"

C = r'''
#include <stdio.h>
#include <string.h>
#include "pit_usage.h"
int main(int argc,char**argv){
    FILE*f;static char line[512];int n=0;
    if(argc<3)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    while(fgets(line,sizeof line,f)){size_t k=strlen(line);while(k&&(line[k-1]=='\n'||line[k-1]=='\r'))line[--k]=0;pitu_tap(line);n++;}
    fclose(f);
    g_pitu.start_tick=GetTickCount()-120000;
    if(strcmp(argv[2],"session")==0){
        pitu_request();      /* game-thread session-end callback */
        pitu_pump();         /* worker loop performs the write */
    } else {
        EXCEPTION_RECORD rec; EXCEPTION_POINTERS ep;
        pitu_veh_install();
        memset(&rec,0,sizeof rec); memset(&ep,0,sizeof ep);
        ep.ExceptionRecord=&rec;
        rec.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
        rec.ExceptionAddress=(PVOID)(uintptr_t)0x00410000u;  /* game .text range */
        pitu_crash_veh(&ep);                    /* writes the crash summary */
        rec.ExceptionAddress=(PVOID)(uintptr_t)0x12345000u;  /* out of range */
        pitu_crash_veh(&ep);                    /* must be ignored */
    }
    printf("%d\n",n);return 0;}
'''

LINES = [
    "Pit Stop Failures 1.0.5: initialized; required game layout and broker accepted",
    "Test driver: wheelgun/irreparable_nut; added 10.469146 seconds",
    "Test driver: incident wheelgun terminal",
    "Test driver: refused: native timing unavailable",
    r"Test driver: SEASONDATA\VEHICLES\FIA\1999_CLK55\1999_fia29.veh",
    'Test driver: "Michael Schumacher" retire to pit',
    "Test driver: retirement requested",
    "Test driver: retire-to-garage handoff queued",
    "ACCESS VIOLATION at 004109D0",
] * 5


def run_case(work, exe, feed, mode, want_end, expect_file):
    game = os.path.join(work, mode)
    os.makedirs(os.path.join(game, "plugins", "lightloader"))
    local_exe = os.path.join(game, "t.exe")
    shutil.copyfile(exe, local_exe)
    subprocess.check_call([local_exe, feed, mode])
    usage = os.path.join(game, "plugins", "lightloader", "reports", "usage")
    files = sorted(os.listdir(usage)) if os.path.isdir(usage) else []
    fails = []
    if len(files) != (1 if expect_file else 0):
        fails.append("share %s expected %d summary, got %d" % (mode, 1 if expect_file else 0, len(files)))
        return fails
    if not expect_file:
        return fails
    raw = open(os.path.join(usage, files[0]), "rb").read()
    text = raw.decode("utf-8")
    j = json.loads(text)
    if j.get("schema") != 1 or j.get("mod") != "pitstop_failures" or j.get("version") != "1.1.1":
        fails.append("schema/mod/version")
    if j.get("end") != want_end:
        fails.append("%s: end=%s want %s" % (mode, j.get("end"), want_end))
    if len(raw) > 40 * 1024:
        fails.append("size %d" % len(raw))
    counts = j.get("counts", {})
    if counts.get("incident") != 10 or counts.get("refused") != 5 or \
       counts.get("retire") != 15 or counts.get("crash") != 5 + (mode == "crash") or        counts.get("lines") != 45 + (mode == "crash"):   # 1.1.1: + exception line
        fails.append("counts %s" % counts)
    for bad in (r"[A-Za-z]:\\", r"\\", "Users", "Matias", "Schumacher", "SEASONDATA", r"\.veh", r"\.ini"):
        if re.search(bad, text, re.I):
            fails.append("leak: " + bad)
    print("  %s: size %d lines %d counts %s end=%s" % (
        mode, len(raw), len(j["lines"]), counts, j["end"]))
    return fails


def main():
    work = tempfile.mkdtemp(prefix="pit_usage_")
    try:
        game = os.path.join(work, "build")
        os.makedirs(game)
        src = os.path.join(work, "t.c")
        open(src, "w").write(C)
        exe = os.path.join(work, "build", "t.exe")
        subprocess.check_call([GCC, "-O2", "-std=c11", "-Wall", "-Wextra",
                               "-Wno-unused-function", "-I", SRC, "-I", SDK,
                               src, "-o", exe])
        feed = os.path.join(work, "feed.txt")
        open(feed, "w", encoding="utf-8").write("\n".join(LINES))
        fails = []
        fails += run_case(work, exe, feed, "session", "session_end", True)
        fails += run_case(work, exe, feed, "crash", "exception", True)
        print("PIT USAGE TEST:", "PASS" if not fails else "FAIL " + "; ".join(fails))
        return 1 if fails else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
