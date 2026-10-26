"""
keyprobe.py — automated snapshot/diff loop for mapping a hotkey (or any
in-game trigger) to the global(s) it writes.

WHY: manually doing snapshot -> wait -> snapshot -> diff by hand, once per
key, drowns in noise from globals that jitter every frame regardless of
input (physics solver scratch space, camera transform, etc). This script
takes a NOISE BASELINE first (a quiet window with no input) and subtracts
whatever moved on its own, so only globals that changed *because of the
trigger* survive.

USAGE
-----
  python tools/keyprobe.py numpad_plus              # 1s baseline, 3s trigger window
  python tools/keyprobe.py numpad_enter --hold 5
  python tools/keyprobe.py look_up --baseline 2 --hold 2

Each run appends its result (label, timestamp, surviving candidates) to
tools/keyprobe_log.jsonl so a session builds up a searchable map instead of
losing prior results.

Requires the game running; uses the same SDK/global list as poke.py.
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import poke  # noqa: E402

LOG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "keyprobe_log.jsonl")


def countdown(seconds, msg):
    print(f"  {msg} ({seconds:.0f}s)...")
    for remaining in range(int(seconds), 0, -1):
        print(f"    {remaining}...", end="\r")
        time.sleep(1)
    print("    go!            ")


def diff_snaps(before, after, thresh):
    changed = {}
    for n, (addr, ptype, v0) in before.items():
        cur = after.get(n)
        if cur is None:
            continue
        v1 = cur[2]
        if ptype in ('float', 'double'):
            if v0 != v1 and abs(v1 - v0) > thresh * max(1.0, abs(v0)):
                changed[n] = (addr, ptype, v0, v1)
        elif v0 != v1:
            changed[n] = (addr, ptype, v0, v1)
    return changed


def run(label, baseline_s, hold_s, thresh):
    sdk = poke.load_sdk()
    game = poke.Game()

    print(f"=== keyprobe: {label} ===")
    print("phase 1/2: noise baseline — do NOT touch anything")
    base_before = poke._snapshot_all(sdk, game)
    countdown(baseline_s, "capturing baseline noise")
    base_after = poke._snapshot_all(sdk, game)
    noisy = set(diff_snaps(base_before, base_after, thresh))
    print(f"  {len(noisy)} globals jitter on their own — will be excluded")

    print("phase 2/2: trigger window")
    trig_before = poke._snapshot_all(sdk, game)
    countdown(hold_s, f"PRESS/HOLD THE KEY NOW for '{label}'")
    trig_after = poke._snapshot_all(sdk, game)
    changed = diff_snaps(trig_before, trig_after, thresh)

    candidates = {n: v for n, v in changed.items() if n not in noisy}

    print(f"\n{len(changed)} total changed, {len(candidates)} survive noise filter:")
    for n, (addr, ptype, v0, v1) in sorted(candidates.items()):
        print(f"  {n:44s} 0x{addr:08x} {poke.fmt_val(v0, ptype)} -> {poke.fmt_val(v1, ptype)}")
    if not candidates:
        print("  (nothing — try a longer --hold, or the trigger doesn't touch a known global)")

    record = {
        "label": label,
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "baseline_noisy_count": len(noisy),
        "candidates": {
            n: {"addr": f"0x{addr:08x}", "type": ptype, "before": v0, "after": v1}
            for n, (addr, ptype, v0, v1) in candidates.items()
        },
    }
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(json.dumps(record) + "\n")
    print(f"\nlogged -> {LOG_PATH}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    label = sys.argv[1]
    args = sys.argv[2:]
    baseline_s, hold_s, thresh = 1.0, 3.0, 0.0
    it = iter(args)
    for a in it:
        if a == "--baseline":
            baseline_s = float(next(it))
        elif a == "--hold":
            hold_s = float(next(it))
        elif a == "--thresh":
            thresh = float(next(it))
    run(label, baseline_s, hold_s, thresh)


if __name__ == "__main__":
    main()
