"""lab.py — unattended experiment harness for F1 Challenge Online.

WHY: every live result in this corpus cost a human launching a race and watching
a screen, which caps investigation at ~3 experiments per session. That, not
missing knowledge, is the throughput bottleneck. The game turns out to support
unattended bring-up itself - config.ini `[AUTOLAUNCH] MODE=QuickRace` boots
straight into a race after LAUNCHTIME seconds (enum at off_5BE6C4:
Off | QuickRace | Replay | MultiServer | MultiClient, parsed by ConfigIniParser).
No patch, no injection.

FIRST PAYLOAD - the persistence map. For every vehicle-struct offset: does a poke
HOLD, or does the game recompute it? That single question decides whether a mod
can poke a value or must hook its writer, and getting it wrong is what killed
safety_car v3. It is only answerable by thousands of writes, i.e. only with a
harness.

    python tools/lab.py probe                 # is the game up, is a slot live
    python tools/lab.py sweep                 # READ-ONLY baseline volatility pass
    python tools/lab.py sweep --go            # the real thing: writes sentinels
    python tools/lab.py autolaunch quickrace  # arm unattended boot (EDITS config.ini)
    python tools/lab.py autolaunch off        # disarm + restore from backup
    python tools/lab.py race --kill           # boot straight into a race (guard
                                              # menu-skip hook; no config edits)

SAFETY, because this writes into a running game and can touch your install:
  * sweep writes a MINIMAL PERTURBATION of each value (int+1, float*1.01), never
    garbage, and restores the original immediately - far less likely to crash
    than random fills, and semantically plausible while it is live.
  * pointer-looking dwords are skipped by default (--include-pointers to opt in).
  * offsets that move on their own are detected in a read-only pre-pass and never
    written, because they cannot be classified anyway.
  * results are checkpointed after every offset, so a crash costs one offset and
    the run resumes where it stopped.
  * `autolaunch` takes a byte-exact backup of config.ini first, verifies it by
    hash, and refuses to proceed if the backup cannot be made.

Results are OBSERVED tier: one session, one car state. A latching offset can
still be rewritten by code that only runs in another phase.
"""
import argparse
import hashlib
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from poke import Game, find_pid, PROC_NAME          # noqa: E402  (shared plumbing)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_DIR = os.path.join(ROOT, "..", "F1 Challenge V10")
CONFIG_INI = os.path.join(GAME_DIR, "config.ini")
CONFIG_BAK = os.path.join(GAME_DIR, "config.ini.lab-backup")

# FACT (CLAUDE.md): vehicle array base / stride / liveness marker.
VEH_BASE = 0x5EC6D8
VEH_STRIDE = 0x48C0
LIVE_MARKER = 0x32C8            # int32 != -1  <=> slot is live

# A dword that looks like a mapped address. Overwriting one of these is the
# single most likely way to crash the game, so they are skipped by default.
PTR_LO, PTR_HI = 0x00400000, 0x7FFF0000

# Offsets never written, whatever the classifier would do with them: the
# liveness marker decides whether the game treats the slot as a car at all, and
# the retirement/active flags are known to drive teleports and state machines.
# Perturbing these does not answer a question, it starts an unrelated experiment.
NEVER_WRITE = {
    LIVE_MARKER,        # 0x32C8 slot liveness (int32 == -1 means dead)
    0x4410,             # RetiredFlag
    0x45F4,             # ActiveFlag
    0x4064,             # FinishState
    0x454C,             # callToBox
    0xD58,              # RearWingIntactFlag - arms the game's own retirement
    0x10C0,             # KILLS THE PROCESS: perturbing it crashed the game
                        # instantly (sweep 2026-07-27, write succeeded, the very
                        # next read failed). Inside the render object embedded at
                        # veh+0x6F0. Do not write it without a debugger attached.
    0x33C8,             # KILLS THE PROCESS: sweep 2026-07-27 died here too,
                        # same signature (write succeeded, next read failed).
}

MODES = ("off", "quickrace", "replay", "multiserver", "multiclient")


# ---------------------------------------------------------------------------
# config.ini / autolaunch
# ---------------------------------------------------------------------------
def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def backup_config():
    if not os.path.exists(CONFIG_INI):
        sys.exit("config.ini not found at %s" % CONFIG_INI)
    if not os.path.exists(CONFIG_BAK):
        shutil.copy2(CONFIG_INI, CONFIG_BAK)
    if sha256(CONFIG_BAK) != sha256(CONFIG_INI):
        # A backup already existed from an earlier armed run: keep the ORIGINAL,
        # never overwrite it with an already-modified file.
        print("note: existing backup differs from current config.ini - keeping "
              "the older backup as the restore point")
    return CONFIG_BAK


def set_autolaunch(mode, launchtime=None):
    """Rewrite [AUTOLAUNCH] MODE (and LAUNCHTIME) in place, nothing else."""
    backup_config()
    with io.open(CONFIG_INI, encoding="latin-1") as f:
        text = f.read()
    out, in_sec, changed = [], False, []
    for line in text.splitlines(True):
        stripped = line.strip()
        if stripped.startswith("["):
            in_sec = stripped.upper() == "[AUTOLAUNCH]"
        elif in_sec:
            m = re.match(r"^(\s*MODE\s*=\s*)(\S*)(.*)$", line, re.I)
            if m:
                line = "%s%s%s\n" % (m.group(1), mode, m.group(3).rstrip("\r\n"))
                changed.append("MODE=%s" % mode)
            elif launchtime is not None:
                m = re.match(r"^(\s*LAUNCHTIME\s*=\s*)(\S*)(.*)$", line, re.I)
                if m:
                    line = "%s%d%s\n" % (m.group(1), launchtime,
                                         m.group(3).rstrip("\r\n"))
                    changed.append("LAUNCHTIME=%d" % launchtime)
        out.append(line)
    with io.open(CONFIG_INI, "w", encoding="latin-1") as f:
        f.write("".join(out))
    print("config.ini updated: %s   (backup: %s)"
          % (", ".join(changed) or "nothing matched", CONFIG_BAK))


def restore_config():
    if not os.path.exists(CONFIG_BAK):
        sys.exit("no backup at %s - nothing to restore" % CONFIG_BAK)
    shutil.copy2(CONFIG_BAK, CONFIG_INI)
    ok = sha256(CONFIG_BAK) == sha256(CONFIG_INI)
    print("config.ini restored from backup (hash match: %s)" % ok)


# ---------------------------------------------------------------------------
# live state
# ---------------------------------------------------------------------------
def slot_addr(slot):
    return VEH_BASE + slot * VEH_STRIDE


def live_slots(game, limit=104):
    out = []
    for s in range(limit):
        v = game.read_val(slot_addr(s) + LIVE_MARKER, "int32")
        if v is not None and v != -1:
            out.append(s)
    return out


def cmd_probe(_args):
    pid = find_pid()
    if not pid:
        print("game not running (%s)" % PROC_NAME)
        return 1
    game = Game()
    slots = live_slots(game)
    print("pid %d, %d live slot(s): %s" % (pid, len(slots), slots[:24]))
    return 0 if slots else 2


# ---------------------------------------------------------------------------
# persistence sweep
# ---------------------------------------------------------------------------
def looks_like_ptr(u32):
    return PTR_LO <= u32 <= PTR_HI


def perturb(raw):
    """A minimal, plausible change: float -> *1.01+eps, int -> +1.

    Returns (new_bytes, kind). Garbage fills crash the game; a small delta of the
    value already there usually does not, and is just as detectable.
    """
    u = struct.unpack("<I", raw)[0]
    f = struct.unpack("<f", raw)[0]
    # All-zero tells us nothing about the type, and turning an int 0 into float
    # 0.25 (0x3E800000 = 1048576000 as an int) is a wild value for an int field.
    # +1 is safe under either reading.
    if u == 0:
        return struct.pack("<I", 1), "int"
    # -1 is this game's "empty/dead" sentinel (the slot liveness marker is
    # int32 == -1). Wrapping it to 0 would claim a dead slot is live, so step
    # DOWN instead of up.
    if u == 0xFFFFFFFF:
        return struct.pack("<I", 0xFFFFFFFE), "int"
    # Treat it as a float only if it is a sane finite magnitude.
    if f == f and abs(f) < 1e18 and 1e-6 < abs(f) < 1e9:
        return struct.pack("<f", f * 1.01 + 0.001), "float"
    return struct.pack("<I", (u + 1) & 0xFFFFFFFF), "int"


def classify(orig, sentinel, after):
    if after == sentinel:
        return "LATCHED"          # the poke holds - a mod can just write it
    if after == orig:
        return "RECALCULATED"     # game restored it - must hook the writer
    return "VOLATILE"             # moved to a third value on its own


def cmd_sweep(args):
    if not find_pid():
        sys.exit("game not running - start it (or arm autolaunch) first")
    game = Game()
    slots = live_slots(game)
    if not slots:
        sys.exit("no live vehicle slot - the game is not in a session yet")
    slot = args.slot if args.slot in slots else slots[0]
    base = slot_addr(slot)
    print("slot %d @ 0x%08X   range 0x%X..0x%X step %d   %s"
          % (slot, base, args.start, args.end, args.step,
             "WRITING" if args.go else "read-only baseline"))

    out_path = os.path.join(ROOT, args.out)
    done = {}
    if os.path.exists(out_path) and not args.restart:
        with io.open(out_path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    r = json.loads(line)
                    done[r["off"]] = r
        print("resuming: %d offsets already recorded" % len(done))

    n_new = 0
    counts = {}
    with io.open(out_path, "a", encoding="utf-8") as log:
        for off in range(args.start, args.end, args.step):
            key = "0x%04X" % off
            if key in done:
                continue
            addr = base + off

            # A failed read means the process is gone, NOT that this offset is
            # uninteresting. The first version used `continue` here, so after a
            # crash the loop silently no-opped through every remaining offset and
            # reported a clean "done" - 4,144 offsets skipped with no warning.
            a = game.read(addr, 4)
            if a is None:
                print("!! read failed at %s - game gone? stopping (resume to "
                      "continue)" % key)
                break
            time.sleep(args.settle)
            b = game.read(addr, 4)
            if b is None:
                print("!! read failed at %s during settle - stopping" % key)
                break

            rec = {"off": key, "slot": slot, "orig": "0x%08X" % struct.unpack("<I", a)[0]}
            if a != b:
                # Moving on its own: a write tells us nothing, so never write.
                rec["class"] = "VOLATILE_BASELINE"
            elif off in NEVER_WRITE:
                rec["class"] = "SKIPPED_PROTECTED"
            elif not args.go:
                rec["class"] = "STABLE_UNTESTED"
            elif looks_like_ptr(struct.unpack("<I", a)[0]) and not args.include_pointers:
                rec["class"] = "SKIPPED_POINTER"
            else:
                sentinel, kind = perturb(a)
                rec["kind"] = kind
                if not game.write(addr, sentinel):
                    rec["class"] = "UNWRITABLE"
                else:
                    time.sleep(args.hold)
                    after = game.read(addr, 4)
                    game.write(addr, a)          # restore ALWAYS, immediately
                    if after is None:
                        rec["class"] = "READ_FAILED"
                    else:
                        rec["class"] = classify(a, sentinel, after)
                        rec["after"] = "0x%08X" % struct.unpack("<I", after)[0]

            log.write(json.dumps(rec) + "\n")
            log.flush()                          # checkpoint every offset
            counts[rec["class"]] = counts.get(rec["class"], 0) + 1
            n_new += 1

            if n_new % 200 == 0:
                print("  %5d offsets  %s" % (n_new, counts))
            if args.limit and n_new >= args.limit:
                break
            if not find_pid():
                print("!! game died at %s - results kept, rerun to resume" % key)
                break

    print("done: %d new offsets -> %s" % (n_new, args.out))
    for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
        print("  %-20s %d" % (k, v))
    return 0


def cmd_autolaunch(args):
    mode = args.mode.lower()
    if mode == "off" and os.path.exists(CONFIG_BAK):
        restore_config()
        return 0
    if mode not in MODES:
        sys.exit("mode must be one of: %s" % ", ".join(MODES))
    set_autolaunch(mode, args.launchtime)
    return 0


def cmd_race(args):
    """Boot the game straight into a race via the guard's menu-skip arm hook
    (finding menu-skip-arm-hook-485bb0: f1cm_guard.asi arms dword_5CC9E8=1 on
    the first Options_Setup-loop sub_485B90 call, so the session starts before
    the menu renders - no config edits, no pokes, no textures needed)."""
    game_exe = os.path.join(GAME_DIR, "F1 Challenge Online.exe")
    if not os.path.exists(game_exe):
        sys.exit("game exe not found at %s" % game_exe)
    guard_asi = os.path.join(GAME_DIR, "f1cm_guard.asi")
    if not os.path.exists(guard_asi):
        sys.exit("f1cm_guard.asi missing - the menu-skip arm hook requires it")
    pid = find_pid(PROC_NAME)
    if pid and not args.kill:
        sys.exit("game already running (pid %d) - pass --kill to restart it" % pid)
    if pid:
        subprocess.run(["taskkill", "/f", "/im", PROC_NAME],
                       capture_output=True)
        time.sleep(2)

    if args.track:
        import session_setup
        session_setup.cmd_set(args.track, False)

    trace = os.path.join(GAME_DIR, "LOG", "trace.txt")
    try:
        os.remove(trace)
    except OSError:
        pass
    log_asi = os.path.join(GAME_DIR, "f1cm_guard_log.txt")
    try:
        os.remove(log_asi)
    except OSError:
        pass

    t0 = time.time()
    subprocess.Popen([game_exe], cwd=GAME_DIR)
    print("launched %s" % os.path.basename(game_exe))
    if args.track:
        print("track: %s" % args.track)

    deadline = time.time() + args.timeout
    warnings = set()
    while time.time() < deadline:
        time.sleep(args.poll)
        if not find_pid(PROC_NAME):
            sys.exit("game process died during boot (trace tail below)")
        try:
            with io.open(trace, encoding="latin-1", errors="replace") as f:
                t = f.read()
        except OSError:
            continue
        for bad in ("CUBE Error", "Could not create"):
            if bad in t:
                warnings.add(bad)
        if "Entered Render::Restart()" in t:
            print("SESSION STARTED after %.0fs - race loading (clean path, "
                  "no particle-system creation)" % (time.time() - t0))
            for w in warnings:
                print("  WARNING: %s lines present in trace" % w)
            with io.open(log_asi, encoding="latin-1", errors="replace") as f:
                if "arm=1" in f.read():
                    print("  guard: menu-skip arm hook armed")
            return 0

    sys.exit("timeout: no session start after %ds" % args.timeout)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")

    sub.add_parser("probe")

    s = sub.add_parser("sweep")
    s.add_argument("--slot", type=int, default=0)
    s.add_argument("--start", type=lambda x: int(x, 0), default=0x0)
    s.add_argument("--end", type=lambda x: int(x, 0), default=VEH_STRIDE)
    s.add_argument("--step", type=int, default=4)
    s.add_argument("--go", action="store_true",
                   help="actually write sentinels (default: read-only baseline)")
    s.add_argument("--include-pointers", action="store_true")
    s.add_argument("--settle", type=float, default=0.08,
                   help="seconds between the two baseline reads")
    s.add_argument("--hold", type=float, default=0.12,
                   help="seconds to leave the sentinel in place")
    s.add_argument("--limit", type=int, default=0)
    s.add_argument("--restart", action="store_true")
    s.add_argument("--out", default="persistence_map.jsonl")

    a = sub.add_parser("autolaunch")
    a.add_argument("mode", help="off | quickrace | replay | multiserver | multiclient")
    a.add_argument("--launchtime", type=int, default=None)

    r = sub.add_parser("race")
    r.add_argument("--track", default=None,
                   help="set the .PLR [SCENE] to a track first (fuzzy match)")
    r.add_argument("--kill", action="store_true",
                   help="kill a running game instance before launching")
    r.add_argument("--timeout", type=int, default=120,
                   help="seconds to wait for the session start (default 120)")
    r.add_argument("--poll", type=float, default=5.0,
                   help="seconds between verification polls (default 5)")

    args = ap.parse_args()
    if args.cmd == "probe":
        return cmd_probe(args)
    if args.cmd == "sweep":
        return cmd_sweep(args)
    if args.cmd == "autolaunch":
        return cmd_autolaunch(args)
    if args.cmd == "race":
        return cmd_race(args)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
