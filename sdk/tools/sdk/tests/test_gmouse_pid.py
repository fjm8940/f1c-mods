#!/usr/bin/env python3
"""Verify that the public UI-click runner fails closed for PID targeting."""
from pathlib import Path
import os
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools/f1c_mod.py")]


def call_rect(*pid_args):
    return subprocess.run(
        [*CLI, "tool-run", "--acknowledge-risk", "click-game-ui", "--",
         "rect", *pid_args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def fail(message):
    raise AssertionError(message)


def main():
    invalid_cases = (
        (("--pid",), "--pid needs a positive F1 Challenge process ID"),
        (("--pid", "not-a-pid"), "--pid needs a positive F1 Challenge process ID"),
        (("--pid", "0"), "--pid needs a positive F1 Challenge process ID"),
    )
    for args, expected in invalid_cases:
        result = call_rect(*args)
        combined = result.stdout + result.stderr
        if result.returncode == 0 or expected not in combined:
            fail(f"PID validation did not fail closed for {args!r}: {combined!r}")

    # The test process cannot own a visible F1 Challenge window. Even if another
    # game instance is running, exact targeting must refuse to fall back to it.
    result = call_rect("--pid", str(os.getpid()))
    combined = result.stdout + result.stderr
    expected = f"F1 Challenge window for PID {os.getpid()} not found"
    if result.returncode == 0 or expected not in combined or "refusing" not in combined:
        fail(f"exact PID targeting did not refuse fallback: {combined!r}")

    print("gmouse PID targeting passed: invalid IDs and wrong-process fallback refused.")


if __name__ == "__main__":
    main()
