"""
gmouse.py — BACKGROUND mouse control for F1 Challenge menus/UI. No focus needed.

The UI screens are driven by ordinary window messages, so PostMessage of
WM_MOUSEMOVE / WM_LBUTTONDOWN / WM_LBUTTONUP clicks buttons without the game
being foreground and without touching the real cursor. Verified live 2026-07-27:
a posted click on the RACE RESULTS confirm button advanced to the car screen.

This is the UI half of the control surface; the in-race keyboard half is
tools/gkey.py (the game's own key queue). Neither steals focus, so both can run
while the machine is used for something else.

NOTE the keyboard queue does NOT drive these UI screens — ESC on RACE RESULTS did
nothing. UI = mouse, in-race actions = key queue.

COORDINATES are client pixels, matching tools/screenshot.py output exactly, so
you can read a target's position straight off a screenshot. --norm takes
0..1 fractions instead, which survives a resolution change.

!! NEVER CLICK THE TELEMETRY ICON !! (user rule: redundant feature, do not use.)
It sits in the bottom-left icon strip, so clicks in that strip require --force.

KNOWING WHERE YOU ARE, without reading pixels: the game names its own current
screen in ASCII at MessageDisplayBuffer (0x5EC4CC) — e.g. "SELECT CIRCUIT",
"accept", "loading". `screen` prints it; `--expect TEXT` refuses to click unless
it matches, which stops a blind click landing on the wrong screen.

USAGE
-----
  python tools/gmouse.py screen                   # what screen is up right now
  python tools/gmouse.py screen --pid 1234        # a specific F1C process
  python tools/gmouse.py click 2453 1396          # client pixels
  python tools/gmouse.py click --norm 0.96 0.97   # resolution-independent
  python tools/gmouse.py click --norm 0.96 0.97 --expect "SELECT CIRCUIT"
  python tools/gmouse.py move 1200 800
  python tools/gmouse.py rect                     # print client size

NOT CONTROLLABLE BY MEMORY: 0x7CB25C / 0x7CB260 mirror the cursor position, but
they are outputs the game writes, not inputs it reads — poking them and posting
button-only messages (lParam=0) clicked nothing. The coordinates must travel in
the message, so menu actions need a posted click. (In-race keys are different:
there a real command queue exists to arm — see tools/gkey.py.)
"""
import ctypes
import json
import os
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_RBUTTONDOWN = 0x0204
WM_RBUTTONUP = 0x0205
MK_LBUTTON = 0x0001

# Bottom-left icon strip: contains the TELEMETRY button, which must never be
# clicked. Expressed as fractions of the client area so it holds at any res.
GUARD_ZONES = [
    ("bottom-left icon strip (contains TELEMETRY — never click)",
     0.00, 0.90, 0.16, 1.00),
]


UI_MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ui_map.json")
MSG_BUFFER = 0x5EC4CC   # ASCII name of the current UI screen/element
MOUSE_X = 0x7CB25C      # read-only mirrors (see module docstring)
MOUSE_Y = 0x7CB260


def read_screen_name():
    """Current screen per the game's own string buffer, or None if unreadable."""
    import poke  # local import: only needed for the memory read
    try:
        game = poke.Game()
    except SystemExit:
        return None
    raw = game.read(MSG_BUFFER, 64)
    if not raw:
        return None
    text = raw.split(b"\x00")[0].decode("ascii", errors="replace").strip()
    return text or None


def find_game_window(target_pid=None):
    hwnd = [None]

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, lparam):
        length = user32.GetWindowTextLengthW(h)
        if length == 0 or not user32.IsWindowVisible(h):
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(h, buf, length + 1)
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(pid))
        if "F1 Challenge" in buf.value and (target_pid is None or pid.value == target_pid):
            hwnd[0] = h
            return False
        return True

    user32.EnumWindows(cb, 0)
    if hwnd[0] is None:
        if target_pid is None:
            sys.exit("F1 Challenge window not found — is the game running?")
        sys.exit(f"F1 Challenge window for PID {target_pid} not found — refusing to target another process")
    return hwnd[0]


def client_size(hwnd):
    r = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    return r.right, r.bottom


def check_guard(x, y, w, h, force):
    fx, fy = x / w, y / h
    for label, x0, y0, x1, y1 in GUARD_ZONES:
        if x0 <= fx <= x1 and y0 <= fy <= y1:
            if not force:
                sys.exit(f"REFUSED: ({x},{y}) is in the {label}. "
                         f"Pass --force only if you are certain it is not the telemetry button.")
            print(f"  WARNING: clicking inside {label} (--force given)")


def lparam(x, y):
    return (y << 16) | (x & 0xFFFF)


def _hover_read(hwnd, x, y, settle):
    user32.PostMessageA(hwnd, WM_MOUSEMOVE, 0, lparam(x, y))
    time.sleep(settle)
    read_screen_name()              # absorb the lagged value
    time.sleep(settle)
    return read_screen_name()


def probe_labels(hwnd, w, h, step, settle, region):
    """Hover a grid and record the game's OWN label for each point.

    This is how to navigate without hardcoded pixel guesses: the game writes the
    hovered item's name into MSG_BUFFER, so we can discover what is where and
    then click it by name.

    The buffer is sticky: over empty space it keeps whatever was last hovered, so
    naively recording it attributes stale labels to unrelated points (one label
    claimed 63 grid points in the first run). So bounce off a neutral point
    before every sample and only accept a label that actually CHANGED — a change
    is the only evidence the label belongs to the point just hovered.
    """
    x0, y0, x1, y1 = region
    neutral = (int(0.06 * w), int(0.45 * h))     # left artwork panel: no item there
    hits, total = {}, 0
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            total += 1
            base = _hover_read(hwnd, *neutral, settle)
            label = _hover_read(hwnd, x, y, settle)
            if label and label != base:
                hits.setdefault(label, []).append((x, y))
    return hits, total


def centroid(points):
    return (sum(p[0] for p in points) // len(points),
            sum(p[1] for p in points) // len(points))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]
    args = sys.argv[2:]
    target_pid = None
    if "--pid" in args:
        position = args.index("--pid")
        if position + 1 >= len(args):
            sys.exit("--pid needs a positive F1 Challenge process ID")
        try:
            target_pid = int(args[position + 1])
        except ValueError:
            sys.exit("--pid needs a positive F1 Challenge process ID")
        if target_pid <= 0:
            sys.exit("--pid needs a positive F1 Challenge process ID")
        del args[position:position + 2]
    force = "--force" in args
    norm = "--norm" in args
    nums = [a for a in args if not a.startswith("--")]

    expect = None
    if "--expect" in args:
        expect = args[args.index("--expect") + 1]
        nums = [a for a in nums if a != expect]
    item = None
    if "--item" in args:
        item = args[args.index("--item") + 1]
        nums = [a for a in nums if a != item]

    hwnd = find_game_window(target_pid)
    w, h = client_size(hwnd)

    if cmd == "rect":
        print(f"  client area: {w} x {h}  (hwnd 0x{hwnd:x})")
        return

    if cmd == "screen":
        name = read_screen_name()
        print(f"  current screen: {name!r}" if name else "  (screen name unreadable)")
        return

    if cmd == "probe":
        step = int(nums[0]) if nums else 100
        settle = 0.12
        if "--settle" in args:
            settle = float(args[args.index("--settle") + 1])
        # default region skips the guarded bottom-left icon strip entirely
        region = (int(0.17 * w), 0, w, int(0.90 * h))
        if "--full" in args:
            region = (0, 0, w, h)
        hits, total = probe_labels(hwnd, w, h, step, settle, region)
        out = {label: centroid(pts) for label, pts in hits.items()}
        with open(UI_MAP, "w", encoding="utf-8") as f:
            json.dump({"client": [w, h], "labels": out}, f, indent=1)
        print(f"  probed {total} points, found {len(out)} distinct labels -> {UI_MAP}")
        for label, xy in sorted(out.items()):
            print(f"    {label:34s} @ {xy[0]},{xy[1]}  ({len(hits[label])} hits)")
        return

    if cmd == "clickname":
        if not nums:
            sys.exit("clickname needs a label substring")
        want = " ".join(nums).lower()
        try:
            with open(UI_MAP, encoding="utf-8") as f:
                data = json.load(f)
        except FileNotFoundError:
            sys.exit(f"no {UI_MAP} yet — run: python tools/gmouse.py probe")
        matches = {k: v for k, v in data["labels"].items() if want in k.lower()}
        if not matches:
            sys.exit(f"no label matching {want!r} in {UI_MAP}")
        if len(matches) > 1:
            print("  ambiguous:")
            for k in matches:
                print(f"    {k}")
            sys.exit(1)
        label, (x, y) = next(iter(matches.items()))
        check_guard(x, y, w, h, force)
        lp = lparam(x, y)
        user32.PostMessageA(hwnd, WM_MOUSEMOVE, 0, lp)
        time.sleep(0.15)
        user32.PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp)
        time.sleep(0.08)
        user32.PostMessageA(hwnd, WM_LBUTTONUP, 0, lp)
        print(f"  clicked {label!r} @ ({x},{y})")
        return

    if expect is not None:
        actual = read_screen_name() or ""
        if expect.lower() not in actual.lower():
            sys.exit(f"REFUSED: expected screen {expect!r} but the game reports "
                     f"{actual!r} — not clicking blind.")
        print(f"  screen confirmed: {actual!r}")

    if len(nums) < 2:
        sys.exit(f"'{cmd}' needs x and y")
    if norm:
        x, y = int(float(nums[0]) * w), int(float(nums[1]) * h)
    else:
        x, y = int(nums[0]), int(nums[1])

    if not (0 <= x < w and 0 <= y < h):
        sys.exit(f"({x},{y}) is outside the {w}x{h} client area")

    lp = lparam(x, y)
    if cmd == "move":
        user32.PostMessageA(hwnd, WM_MOUSEMOVE, 0, lp)
        print(f"  moved to ({x},{y})")
    elif cmd == "click":
        check_guard(x, y, w, h, force)
        user32.PostMessageA(hwnd, WM_MOUSEMOVE, 0, lp)
        time.sleep(0.12)                       # let hover/hit-test settle
        if item is not None:
            # VERIFY-THEN-CLICK: the label buffer names whatever is under the
            # cursor, so hovering first and checking it is far more reliable than
            # trusting a pixel guess. Retry because the buffer lags a frame.
            seen = []
            for _ in range(6):
                lbl = read_screen_name()
                if lbl:
                    seen.append(lbl)
                    if item.lower() in lbl.lower():
                        break
                time.sleep(0.08)
            if not any(item.lower() in s.lower() for s in seen):
                sys.exit(f"REFUSED: wanted item {item!r} under ({x},{y}) but the game "
                         f"reported {seen[-3:]!r} — not clicking.")
            print(f"  item under cursor confirmed: {item!r}")
        user32.PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp)
        time.sleep(0.08)
        user32.PostMessageA(hwnd, WM_LBUTTONUP, 0, lp)
        print(f"  clicked ({x},{y})  [{x / w:.3f},{y / h:.3f} normalised]")
    elif cmd == "rclick":
        check_guard(x, y, w, h, force)
        user32.PostMessageA(hwnd, WM_MOUSEMOVE, 0, lp)
        time.sleep(0.12)
        user32.PostMessageA(hwnd, WM_RBUTTONDOWN, 0x0002, lp)
        time.sleep(0.08)
        user32.PostMessageA(hwnd, WM_RBUTTONUP, 0, lp)
        print(f"  right-clicked ({x},{y})")
    else:
        print(f"unknown command '{cmd}'")
        print(__doc__)


if __name__ == "__main__":
    main()
