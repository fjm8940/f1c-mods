"""Red Flag 1.1.7 usage summary (rf_usage.h) replay test.

Two real write paths against the source header:
  1. session end: rfu_request() -> rfu_pump() on the RF worker loop (the 1.0.9
     family fix for "no usage file on a normal return to the menu").
  2. crash: the shared crash VEH on an access violation in the main image.

Also asserts (static) that DllMain performs no usage flush, rf_session_end only
sets rfu_request and thread_main calls rfu_pump. Build output to %TEMP%."""
import json, os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "src")
SDK = os.path.join(HERE, "..", "..", "..", "..", "..", "f1c_ida_decompiled", "mods", "sdk")
GCC = r"C:\msys64\mingw32\bin\gcc.exe"
C = r'''
#include <stdio.h>
#include <string.h>
#include "rf_usage.h"
int main(int argc,char**argv){
 FILE*f;static char line[1024];int n=0;
 if(argc<3)return 2;
 f=fopen(argv[1],"rb");if(!f)return 2;
 while(fgets(line,sizeof line,f)){size_t k=strlen(line);while(k&&(line[k-1]=='\n'||line[k-1]=='\r'))line[--k]=0;rfu_tap(line);n++;}
 fclose(f);
 g_rfu.start_tick=GetTickCount()-120000;
 if(strcmp(argv[2],"session")==0){
  rfu_request();   /* game-thread session-end callback */
  rfu_pump();      /* worker loop performs the write */
 } else {
  EXCEPTION_RECORD rec; EXCEPTION_POINTERS ep;
  rfu_veh_install();
  memset(&rec,0,sizeof rec); memset(&ep,0,sizeof ep);
  ep.ExceptionRecord=&rec;
  rec.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
  rec.ExceptionAddress=(PVOID)(uintptr_t)0x12345000u; f1c_usage_veh_cb(&ep);  /* outside both: nothing */
  rec.ExceptionAddress=(PVOID)&GetTickCount;         f1c_usage_veh_cb(&ep);  /* kernel32: nothing */
  rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep);  /* own module: one */
  rec.ExceptionAddress=(PVOID)&main;                 f1c_usage_veh_cb(&ep);  /* once per process */
  rec.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;   f1c_usage_veh_cb(&ep);  /* not an AV */
 }
 printf("%d\n",n);return 0;}
'''

FEED = [
    "Red Flag loaded. Automatic race control is configurable through redflag.ini.",
    "TRIGGER: FULL RED FLAG (NUMPAD2) - AI to pits, restart pending.",
    "RED FLAG REFUSED: player finish/retired state is unsupported; no restart started.",
    r"BROKER BIND REFUSED: %USERPROFILE%\Documents\x.veh",
    "GREEN RESTORE WATCHDOG: game-thread callback still owns the transaction.",
] * 30 + [
    'RF KEY CONFLICT: "Michael Schumacher" /home/user/x',
    "CONFIG source: path=SEASONDATA\\VEHICLES\\FIA\\1999_fia29.veh present=1",
    "TRIGGER: auto after severe-incident announcement \u00e4\u00f6",
]


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
        fails.append("%s expected 1 summary, got %d (%s)" % (mode, len(files), files))
        return fails
    raw = open(os.path.join(usage, files[0]), "rb").read()
    text = raw.decode("utf-8")
    j = json.loads(text)
    if j.get("schema") != 1 or j.get("mod") != "red_flag" or j.get("version") != "1.1.8":
        fails.append("schema/mod/version")
    if j.get("end") != want_end:
        fails.append("%s: end=%s want %s" % (mode, j.get("end"), want_end))
    if len(raw) > 40 * 1024:
        fails.append("size %d" % len(raw))
    if not files[0].startswith("red_flag-"):
        fails.append("name " + files[0])
    if j["lines"] and not all(re.match(r"^RF-E[0-9A-F]{8}( \S+)*$", ln) for ln in j["lines"]):
        fails.append("lines not coded: " + repr(j["lines"][:3]))
    if not any(re.search(r"^RF-E[0-9A-F]{8} .*[0-9]", ln) for ln in j["lines"]):
        fails.append("no coded line kept a number")
    if "<s>" in text or "<path>" in text:
        fails.append("placeholder leaked in lines")
    for bad in (r"[A-Za-z]:\\", r"\\", "Users", "Matias", "Schumacher",
                r"\.veh", r"\.ini", r"\bSEASONDATA\b"):
        if re.search(bad, text, re.I):
            fails.append("leak: " + bad)
    if any(ord(c) > 126 or ord(c) < 32 for c in text):
        fails.append("non-ascii")
    print("  %s: size %d lines %d counts %s end=%s" % (
        mode, len(raw), len(j["lines"]), j["counts"], j["end"]))
    return fails


def main():
    work = tempfile.mkdtemp(prefix="rf_usage_")
    try:
        game = os.path.join(work, "build")
        os.makedirs(game)
        src = os.path.join(work, "t.c"); open(src, "w").write(C)
        exe = os.path.join(game, "t.exe")
        subprocess.check_call([GCC, "-O2", "-std=c11", "-Wall", "-Wextra", "-Wno-unused-function",
                               "-I", SRC, "-I", SDK, src, "-o", exe])
        feed = os.path.join(work, "feed.txt")
        open(feed, "w", encoding="utf-8").write("\n".join(FEED))
        fails = []
        fails += run_case(work, exe, feed, "session", "session_end")
        fails += run_case(work, exe, feed, "crash", "exception")
        # DllMain must contain no usage flush call (loader lock / dead threads).
        gr44 = open(os.path.join(SRC, "gr44.c"), encoding="utf-8").read()
        dm = gr44[gr44.index("BOOL WINAPI DllMain"):]
        if "rfu_flush" in dm:
            fails.append("DllMain must not call rfu_flush")
        if "rfu_veh_remove" not in dm:
            fails.append("DllMain should detach the shared VEH")
        se = gr44[gr44.index("static void __cdecl rf_session_end"):]
        se = se[:se.index("\n}")]
        if "rfu_request" not in se:
            fails.append("rf_session_end must call rfu_request")
        if "rfu_flush" in se:
            fails.append("rf_session_end must not call rfu_flush (game thread)")
        tm = gr44[gr44.index("static DWORD WINAPI thread_main"):]
        if "rfu_pump" not in tm:
            fails.append("thread_main must call rfu_pump")
        print("RF USAGE TEST:", "PASS" if not fails else "FAIL " + "; ".join(fails))
        return 1 if fails else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
