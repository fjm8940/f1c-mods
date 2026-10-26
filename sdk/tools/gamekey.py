"""
gamekey.py — send a real keypress into the game via the Interception driver
(kernel-level injection, bypasses exclusive DirectInput — plain SendInput does
NOT work against this game, this does).

Interception injects at the hardware level, so keystrokes go wherever the OS
focus is, exactly like a real key. That means focus MUST be on the game or the
key lands in another app (it once landed in Notepad++). Two guards:

  1. force_foreground() uses the AttachThreadInput recipe — attaching our thread
     to the current foreground thread lifts Windows' focus-steal prevention,
     which is why a bare SetForegroundWindow silently fails.
  2. HARD ABORT: focus is verified before the sequence AND re-verified before
     every single keystroke. If the game is not foreground, nothing is sent and
     the script exits non-zero. Never send blind.

USAGE
-----
  python tools/gamekey.py esc
  python tools/gamekey.py add --hold 0.5      # numpad +, held 0.5s
  python tools/gamekey.py down --presses 3    # arrow-down 3x
  python tools/gamekey.py p --dry-run         # focus + verify only, send nothing
"""
import ctypes
import sys
import time
from ctypes import wintypes

import interception

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
user32.AttachThreadInput.restype = wintypes.BOOL
user32.SetForegroundWindow.argtypes = [wintypes.HWND]
user32.GetForegroundWindow.restype = wintypes.HWND

SW_RESTORE = 9
SW_SHOW = 5


def find_game_window():
    hwnd = [None]

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, lparam):
        length = user32.GetWindowTextLengthW(h)
        if length == 0 or not user32.IsWindowVisible(h):
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(h, buf, length + 1)
        if "F1 Challenge" in buf.value:
            hwnd[0] = h
            return False
        return True

    user32.EnumWindows(cb, 0)
    if hwnd[0] is None:
        sys.exit("F1 Challenge window not found — is the game running?")
    return hwnd[0]


def force_foreground(hwnd, attempts=5):
    """AttachThreadInput recipe. Returns True only if the OS confirms focus."""
    for _ in range(attempts):
        if user32.GetForegroundWindow() == hwnd:
            return True

        fg = user32.GetForegroundWindow()
        fg_thread = user32.GetWindowThreadProcessId(fg, None) if fg else 0
        our_thread = kernel32.GetCurrentThreadId()

        attached = False
        if fg_thread and fg_thread != our_thread:
            attached = bool(user32.AttachThreadInput(our_thread, fg_thread, True))
        try:
            user32.ShowWindow(hwnd, SW_RESTORE)
            user32.ShowWindow(hwnd, SW_SHOW)
            user32.BringWindowToTop(hwnd)
            user32.SetForegroundWindow(hwnd)
            user32.SetActiveWindow(hwnd)
            user32.SetFocus(hwnd)
        finally:
            if attached:
                user32.AttachThreadInput(our_thread, fg_thread, False)

        time.sleep(0.2)
        if user32.GetForegroundWindow() == hwnd:
            return True
    return False


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    key = sys.argv[1]
    args = sys.argv[2:]
    hold, presses, dry_run = 0.05, 1, False
    it = iter(args)
    for a in it:
        if a == "--hold":
            hold = float(next(it))
        elif a == "--presses":
            presses = int(next(it))
        elif a == "--dry-run":
            dry_run = True

    hwnd = find_game_window()
    if not force_foreground(hwnd):
        sys.exit("ABORT: could not give the game foreground focus — nothing sent "
                 "(a keystroke now would land in whatever app IS focused)")
    print(f"  game window 0x{hwnd:x} confirmed foreground")

    if dry_run:
        print("  --dry-run: focus verified, no key sent")
        return

    interception.auto_capture_devices(keyboard=True, mouse=False)
    for i in range(presses):
        # re-verify every keystroke: focus can be lost mid-sequence
        if user32.GetForegroundWindow() != hwnd:
            sys.exit(f"ABORT: game lost focus before press {i + 1}/{presses} — stopped")
        interception.key_down(key)
        time.sleep(hold)
        interception.key_up(key)
        if presses > 1:
            time.sleep(0.1)
    print(f"  sent '{key}' x{presses} (hold {hold}s)")


if __name__ == "__main__":
    main()
