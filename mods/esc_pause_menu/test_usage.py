"""ESC Pause Menu 1.0.9 usage summary (esc_usage.h) replay test.

Two real write paths against the source header:
  1. session end: escu_request() -> escu_pump() on the worker loop (the 1.0.9
     fix for "1.0.8 wrote no usage file on a normal return to the menu").
  2. crash: the shared crash VEH on an access violation in the main image.

Build output goes to %TEMP%. Replays a real-ish ESC log and injected path/name
lines, then checks schema/mod/version, size cap and no drive paths / names.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(HERE, "..", "..", "..", "f1c_ida_decompiled", "mods", "sdk")
GCC = r"C:\msys64\mingw32\bin\gcc.exe"
LOG = os.path.join(HERE, "build", "esc_pause_menu_log.txt")

C = r'''
#include <stdio.h>
#include <string.h>
#include "esc_usage.h"
int main(int argc,char**argv){
    FILE*f;static char line[1024];int n=0;
    if(argc<3)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    while(fgets(line,sizeof line,f)){size_t k=strlen(line);while(k&&(line[k-1]=='\n'||line[k-1]=='\r'))line[--k]=0;escu_tap(line);n++;}
    fclose(f);
    g_escu.start_tick=GetTickCount()-120000;
    if(strcmp(argv[2],"session")==0){
        escu_request();      /* game-thread session-end callback */
        escu_pump();         /* worker loop performs the write */
    } else {
        EXCEPTION_RECORD rec; EXCEPTION_POINTERS ep;
        escu_veh_install();
        memset(&rec,0,sizeof rec); memset(&ep,0,sizeof ep);
        ep.ExceptionRecord=&rec;
        rec.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
        rec.ExceptionAddress=(PVOID)(uintptr_t)0x12345000u; f1c_usage_veh_cb(&ep); /* outside: none */
        rec.ExceptionAddress=(PVOID)&GetTickCount;         f1c_usage_veh_cb(&ep); /* another DLL: none */
        rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep); /* own module: one */
        rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep); /* once: none */
        rec.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;   f1c_usage_veh_cb(&ep); /* not AV: none */
    }
    printf("%d\n",n);return 0;}
'''


def run_case(work, exe, feed, mode, want_end):
    game = os.path.join(work, mode)
    os.makedirs(os.path.join(game, "plugins", "lightloader"))
    local_exe = os.path.join(game, "t.exe")
    shutil.copyfile(exe, local_exe)
    subprocess.check_call([local_exe, feed, mode])
    usage = os.path.join(game, "plugins", "lightloader", "reports", "usage")
    files = sorted(os.listdir(usage)) if os.path.isdir(usage) else []
    fails = []
    if len(files) != 1:
        fails.append("%s expected 1 summary, got %d" % (mode, len(files)))
        return fails
    raw = open(os.path.join(usage, files[0]), "rb").read()
    text = raw.decode("utf-8")
    j = json.loads(text)
    if j.get("schema") != 1 or j.get("mod") != "esc_pause_menu" or j.get("version") != "1.1.1":
        fails.append("schema/mod/version")
    if j.get("end") != want_end:
        fails.append("%s: end=%s want %s" % (mode, j.get("end"), want_end))
    if len(raw) > 40 * 1024:
        fails.append("size %d" % len(raw))
    if not files[0].startswith("esc_pause_menu-"):
        fails.append("file name " + files[0])
    for bad in (r"[A-Za-z]:\\", r"\\", "Users", "Matias", "Schumacher", r"\.veh", r"\.ini", r"\.PLR"):
        if re.search(bad, text, re.I):
            fails.append("leak: " + bad)
    print("  %s: size %d lines %d counts %s end=%s" % (
        mode, len(raw), len(j["lines"]), j["counts"], j["end"]))
    return fails


def main():
    work = tempfile.mkdtemp(prefix="esc_usage_")
    try:
        game = os.path.join(work, "build")
        os.makedirs(game)
        src = os.path.join(work, "t.c")
        open(src, "w").write(C)
        exe = os.path.join(work, "build", "t.exe")
        subprocess.check_call([GCC, "-O2", "-std=c11", "-Wall", "-Wextra",
                               "-Wno-unused-function", "-I", HERE, "-I", SDK,
                               src, "-o", exe])
        feed = os.path.join(work, "feed.txt")
        lines = open(LOG, encoding="utf-8", errors="replace").read().split("\n")[-3000:]
        lines += ["RETIRE command received (session gen 3)",
                  r"esc_pause_menu loaded; SDK 1.0; target %USERPROFILE%\Documents\x.exe",
                  'RETIRE: refused: "Michael Schumacher" path /home/user/x.ini',
                  "ACCESS VIOLATION at 004109D0"]
        open(feed, "w", encoding="utf-8").write("\n".join(lines))
        fails = []
        fails += run_case(work, exe, feed, "session", "session_end")
        fails += run_case(work, exe, feed, "crash", "exception")
        print("ESC USAGE TEST:", "PASS" if not fails else "FAIL " + "; ".join(fails))
        return 1 if fails else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
