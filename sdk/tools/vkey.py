"""
vkey.py — BACKGROUND virtual keypresses for F1 Challenge (no focus stealing).

Talks to vkeys.dll (mods/vkeys) through the named shared-memory block
"F1C_VKEYS": 256 bytes, one per DIK scancode. The DLL ORs these into the
keyboard state the game reads from DirectInput, so a "press" here is
indistinguishable from a real key to the game — but it does NOT touch OS focus,
so you can keep using your PC while it runs.

Contrast with tools/gamekey.py, which uses the Interception driver and DOES
steal focus. Use gamekey.py when you need a real hardware-level key (e.g. the
game menus if this fails); use vkey.py for anything unattended.

Requires vkeys.dll injected:  mods/vkeys/inject.exe vkeys.dll

USAGE
-----
  python tools/vkey.py tap add                # press+release numpad +
  python tools/vkey.py tap esc --hold 0.2
  python tools/vkey.py hold left 1.5          # hold left arrow 1.5s
  python tools/vkey.py down w                 # press and LEAVE held
  python tools/vkey.py up w                   # release
  python tools/vkey.py clear                  # release everything
  python tools/vkey.py state                  # what is currently held
  python tools/vkey.py keys                   # list known key names
"""
import ctypes
import mmap
import sys
import time

SHM_NAME = "F1C_VKEYS"
SHM_SIZE = 256

# DIK scancodes — the codes the game itself uses (confirmed live: InputActionMap
# at 0x7cb21c reported 0x4e for numpad+, 0x4a for numpad-, 0x48 for numpad8,
# 0x19 for P, all matching this table).
DIK = {
    "escape": 0x01, "esc": 0x01,
    "1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06,
    "6": 0x07, "7": 0x08, "8": 0x09, "9": 0x0A, "0": 0x0B,
    "minus": 0x0C, "equals": 0x0D, "backspace": 0x0E, "tab": 0x0F,
    "q": 0x10, "w": 0x11, "e": 0x12, "r": 0x13, "t": 0x14,
    "y": 0x15, "u": 0x16, "i": 0x17, "o": 0x18, "p": 0x19,
    "lbracket": 0x1A, "rbracket": 0x1B, "return": 0x1C, "enter": 0x1C,
    "lcontrol": 0x1D, "ctrl": 0x1D,
    "a": 0x1E, "s": 0x1F, "d": 0x20, "f": 0x21, "g": 0x22,
    "h": 0x23, "j": 0x24, "k": 0x25, "l": 0x26,
    "semicolon": 0x27, "apostrophe": 0x28, "grave": 0x29,
    "lshift": 0x2A, "shift": 0x2A, "backslash": 0x2B,
    "z": 0x2C, "x": 0x2D, "c": 0x2E, "v": 0x2F, "b": 0x30,
    "n": 0x31, "m": 0x32, "comma": 0x33, "period": 0x34, "slash": 0x35,
    "rshift": 0x36, "multiply": 0x37, "lmenu": 0x38, "alt": 0x38,
    "space": 0x39, "capital": 0x3A, "capslock": 0x3A,
    "f1": 0x3B, "f2": 0x3C, "f3": 0x3D, "f4": 0x3E, "f5": 0x3F,
    "f6": 0x40, "f7": 0x41, "f8": 0x42, "f9": 0x43, "f10": 0x44,
    "numlock": 0x45, "scroll": 0x46,
    "numpad7": 0x47, "numpad8": 0x48, "numpad9": 0x49, "subtract": 0x4A,
    "numpad4": 0x4B, "numpad5": 0x4C, "numpad6": 0x4D, "add": 0x4E,
    "numpad1": 0x4F, "numpad2": 0x50, "numpad3": 0x51,
    "numpad0": 0x52, "decimal": 0x53,
    "f11": 0x57, "f12": 0x58,
    "numpadenter": 0x9C, "rcontrol": 0x9D,
    "divide": 0xB5, "sysrq": 0xB7, "rmenu": 0xB8, "pause": 0xC5,
    "home": 0xC7, "up": 0xC8, "prior": 0xC9, "pageup": 0xC9,
    "left": 0xCB, "right": 0xCD, "end": 0xCF, "down": 0xD0,
    "next": 0xD1, "pagedown": 0xD1, "insert": 0xD2, "delete": 0xD3,
}

FILE_MAP_ALL_ACCESS = 0xF001F
k32 = ctypes.windll.kernel32


def open_shm():
    """Open the DLL's existing mapping. Fails if vkeys.dll is not injected."""
    h = k32.OpenFileMappingA(FILE_MAP_ALL_ACCESS, False, SHM_NAME.encode())
    if not h:
        sys.exit(f"shared memory '{SHM_NAME}' not found — inject vkeys.dll first:\n"
                 f"  cd mods\\vkeys && inject.exe vkeys.dll")
    k32.MapViewOfFile.restype = ctypes.c_void_p
    addr = k32.MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE)
    if not addr:
        sys.exit("MapViewOfFile failed")
    return (ctypes.c_ubyte * SHM_SIZE).from_address(addr)


def dik(name):
    n = name.lower()
    if n in DIK:
        return DIK[n]
    if n.startswith("0x"):
        return int(n, 16)
    sys.exit(f"unknown key '{name}' — see: python tools/vkey.py keys")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]
    args = sys.argv[2:]

    if cmd == "keys":
        names = sorted(DIK, key=lambda k: (DIK[k], k))
        for i in range(0, len(names), 6):
            print("  " + "  ".join(f"{n}(0x{DIK[n]:02x})" for n in names[i:i + 6]))
        return

    buf = open_shm()

    if cmd == "clear":
        ctypes.memset(ctypes.addressof(buf), 0, SHM_SIZE)
        print("  all virtual keys released")
        return

    if cmd == "state":
        held = [n for n, c in sorted(DIK.items(), key=lambda kv: kv[1]) if buf[c]]
        print(f"  held: {', '.join(held) if held else '(none)'}")
        return

    hold = 0.05
    it = iter(args)
    positional = []
    for a in it:
        if a == "--hold":
            hold = float(next(it))
        else:
            positional.append(a)

    if not positional:
        sys.exit(f"'{cmd}' needs a key name")
    code = dik(positional[0])

    if cmd == "down":
        buf[code] = 1
        print(f"  {positional[0]} (0x{code:02x}) held — release with: vkey.py up {positional[0]}")
    elif cmd == "up":
        buf[code] = 0
        print(f"  {positional[0]} (0x{code:02x}) released")
    elif cmd == "tap":
        buf[code] = 1
        time.sleep(hold)
        buf[code] = 0
        print(f"  tapped {positional[0]} (0x{code:02x}) for {hold}s")
    elif cmd == "hold":
        dur = float(positional[1]) if len(positional) > 1 else 1.0
        buf[code] = 1
        try:
            time.sleep(dur)
        finally:
            buf[code] = 0
        print(f"  held {positional[0]} (0x{code:02x}) for {dur}s")
    else:
        print(f"unknown command '{cmd}'")
        print(__doc__)


if __name__ == "__main__":
    main()
