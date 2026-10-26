"""
tour.py — screenshot the game window every N seconds with an audible beep, so
you can see exactly when each capture happens and click around while I watch.

USAGE
-----
  python tools/tour.py                 # every 2s, beep each shot, Ctrl+C to stop
  python tools/tour.py --interval 3
  python tools/tour.py --dir tools/tour_shots
"""
import ctypes
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import screenshot  # noqa: E402

BEEP_FREQ = 1000
BEEP_MS = 120


def main():
    args = sys.argv[1:]
    interval = 2.0
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tour_shots")
    it = iter(args)
    for a in it:
        if a == "--interval":
            interval = float(next(it))
        elif a == "--dir":
            out_dir = next(it)

    os.makedirs(out_dir, exist_ok=True)
    hwnd = screenshot.find_window()
    print(f"  tour started, every {interval}s -> {out_dir}  (Ctrl+C to stop)")

    i = 0
    try:
        while True:
            i += 1
            path = os.path.join(out_dir, f"tour_{i:04d}.png")
            screenshot.capture(hwnd, path)
            ctypes.windll.kernel32.Beep(BEEP_FREQ, BEEP_MS)
            print(f"  [{i}] {time.strftime('%H:%M:%S')} -> {path}")
            time.sleep(interval)
    except KeyboardInterrupt:
        print(f"  stopped after {i} shots")


if __name__ == "__main__":
    main()
