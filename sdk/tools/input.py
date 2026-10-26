"""
input.py — send keystrokes to F1 Challenge Online.exe via SendInput.

Game reads DirectInput/raw keyboard state, not WM_KEYDOWN messages, so
PostMessage does not work here — SendInput is required, which means the
game window must be FOREGROUND when a key is sent (this tool brings it
to front first).

Keymap below is the F1C stock default control scheme (menus + camera),
user-remappable in-game — if a key doesn't do what's listed, the user
has rebound it; ask them what it actually does before trusting the name.

USAGE
-----
  python tools/input.py press ESC
  python tools/input.py press F2
  python tools/input.py tap "CAMERA_NEXT_CAR" --hold 0.05
  python tools/input.py list
"""
import ctypes
import sys
import time
from ctypes import wintypes

PROC_NAME = "F1 Challenge Online.exe"

user32 = ctypes.WinDLL("user32", use_last_error=True)

INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_SCANCODE = 0x0008

# virtual-key codes for named keys
VK = {
    "ESC": 0x1B, "ENTER": 0x0D, "TAB": 0x09, "SPACE": 0x20,
    "UP": 0x26, "DOWN": 0x28, "LEFT": 0x25, "RIGHT": 0x27,
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74,
    "F6": 0x75, "F7": 0x76, "F8": 0x77, "F9": 0x78, "F10": 0x79,
    "F11": 0x7A, "F12": 0x7B,
    "PAGEUP": 0x21, "PAGEDOWN": 0x22,
    "NUMPAD_ADD": 0x6B, "NUMPAD_SUB": 0x6D,
}
for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    VK[c] = ord(c)
for d in "0123456789":
    VK[d] = ord(d)

# semantic actions -> stock F1C default key. UNVERIFIED against this
# install's actual bindings — confirm live before relying on any of these.
ACTIONS = {
    "PAUSE": "F1",              # pause / options overlay while racing
    "CAMERA_COCKPIT": "F2",
    "CAMERA_CHASE": "F3",
    "CAMERA_TV": "F4",
    "CAMERA_NEXT_CAR": "NUMPAD_ADD",
    "CAMERA_PREV_CAR": "NUMPAD_SUB",
    "MENU_UP": "UP",
    "MENU_DOWN": "DOWN",
    "MENU_LEFT": "LEFT",
    "MENU_RIGHT": "RIGHT",
    "MENU_SELECT": "ENTER",
    "MENU_BACK": "ESC",
}


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [
        ("wVk", wintypes.WORD), ("wScan", wintypes.WORD),
        ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD),
        ("dwExtraInfo", ctypes.POINTER(wintypes.ULONG)),
    ]


class INPUT(ctypes.Structure):
    class _I(ctypes.Union):
        _fields_ = [("ki", KEYBDINPUT)]
    _anonymous_ = ("_i",)
    _fields_ = [("type", wintypes.DWORD), ("_i", _I)]


def find_window(name=PROC_NAME):
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
        sys.exit("F1 Challenge window not found — is the game running and not minimized?")
    return hwnd[0]


def focus(hwnd):
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.1)


MAPVK_VK_TO_VSC = 0
KEYEVENTF_EXTENDEDKEY = 0x0001


def send_vk(vk, keyup=False):
    # DirectInput games poll hardware scancodes, not VKs — send scancode form.
    scan = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    flags = KEYEVENTF_SCANCODE | (KEYEVENTF_KEYUP if keyup else 0)
    inp = INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(0, scan, flags, 0, None))
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def press(key, hold=0.05):
    key = key.upper()
    if key not in VK:
        sys.exit(f"unknown key '{key}' — see input.py VK table")
    hwnd = find_window()
    focus(hwnd)
    send_vk(VK[key])
    time.sleep(hold)
    send_vk(VK[key], keyup=True)


def tap(action, hold=0.05):
    action = action.upper()
    if action not in ACTIONS:
        sys.exit(f"unknown action '{action}' — python tools/input.py list")
    press(ACTIONS[action], hold)


def main():
    args = sys.argv[1:]
    if not args or args[0] == "list":
        print("ACTIONS (stock defaults, unverified against this install):")
        for k, v in ACTIONS.items():
            print(f"  {k:20s} -> {v}")
        print("\nraw keys:", ", ".join(sorted(VK)))
        return
    cmd = args[0]
    if cmd == "press" and len(args) >= 2:
        hold = float(args[3]) if len(args) > 3 and args[2] == "--hold" else 0.05
        press(args[1], hold)
        print(f"pressed {args[1].upper()}")
    elif cmd == "tap" and len(args) >= 2:
        tap(args[1])
        print(f"tapped action {args[1].upper()}")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
