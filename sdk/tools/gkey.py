"""
gkey.py — BACKGROUND keypress injection for F1 Challenge. No focus stealing,
no driver, no DLL: a pure memory write into the game's own pending-key queue.

HOW IT WORKS (all FACT, verified live 2026-07-27)
-------------------------------------------------
KeyboardInputActionMapper_0040be70 drains a queue of pending DIK scancodes
every frame:
    word_7CB21C[]  = int16 queue of scancodes
    dword_7CB218   = number of pending entries
Writing a scancode into the queue and setting the count makes the game act on
it exactly as if the key were pressed — it is the game's own trigger, not a
forged end-state. Because nothing goes through the OS input stack, focus is
irrelevant: you can keep using the PC while this runs.

Why not the other routes (all tested and eliminated):
  * SendInput / PostMessage WM_KEYDOWN  — game never sees them
  * IDirectInputDevice8::GetDeviceState/GetDeviceData — 0 calls in-race
    (DirectInput8 is only used for the wheel / force feedback)
  * user32!GetKeyboardState (IAT-hooked) — 0 calls in-race
  * Interception driver (tools/gamekey.py) — works, but is hardware-level so it
    follows OS focus and steals your desktop

TIMING: the game refills the count every frame (~3ms at 300fps), so a single
lazy write usually loses the race. Each attempt writes scancode-then-count and
immediately re-checks; camera keys are verified by watching SpectatedVehicleSlot
so one call = exactly one logical press.

USAGE
-----
  python tools/gkey.py press add            # numpad + : next car
  python tools/gkey.py press subtract       # numpad - : previous car
  python tools/gkey.py press numpadenter    # back to your own car
  python tools/gkey.py press home           # camera mode
  python tools/gkey.py press add --times 3  # three separate presses
  python tools/gkey.py raw 78               # any DIK scancode
  python tools/gkey.py keys
"""
import struct
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0])
import poke  # noqa: E402

Q_KEYS = 0x7CB21C   # int16[] pending scancodes
Q_COUNT = 0x7CB218  # int32 pending count
SLOT = 0x7C54DC     # SpectatedVehicleSlot — lets us confirm camera keys landed

# The queue is a SHARED key-event queue with several independent consumers, each
# handling its own scancodes (all read dword_7CB218 / word_7CB21C):
#   KeyboardInputActionMapper_0040be70 — spectate camera
#   sub_42C440                         — driving aids (F1..F10)
#   RestartGate_004a0fd0               — ESC -> MainMenuTransition
#   sub_4D8450                         — broadcast/display toggles
#   MouseInputHandler_0045f030         — UI list select, expects 0x1000|DIK
KEYS = {
    # --- spectate camera (KeyboardInputActionMapper_0040be70) ---
    "subtract":    (74,  "numpad -  : previous car"),
    "add":         (78,  "numpad +  : next car"),
    "numpadenter": (156, "numpad enter : back to your own car"),
    "home":        (199, "camera mode (CameraLockCheck 4)"),
    "pageup":      (201, "camera mode (CameraLockCheck 3)"),
    "pagedown":    (209, "camera mode (CameraLockCheck 5)"),
    "insert":      (210, "camera mode (CameraLockCheck 0)"),
    # --- driving aids, toggles (sub_42C440) ---
    "f1":          (59,  "toggle SteeringAssist"),
    "f2":          (60,  "toggle OppositeLockAssist"),
    "f3":          (61,  "toggle BrakingPointAssist"),
    "f4":          (62,  "toggle StabilityAssist"),
    "f5":          (63,  "toggle SpinRecovery"),
    "f6":          (64,  "toggle Invulnerability"),
    "f7":          (65,  "toggle AutoShifting"),
    "f8":          (66,  "toggle TractionControl"),
    "f9":          (67,  "toggle AntiLockBrakes"),
    "f10":         (68,  "toggle PitLaneAssistance"),
    # --- display (sub_4D8450) ---
    "2":           (3,   "display/broadcast toggle"),
    "3":           (4,   "toggle broadcast display overlay"),
    # --- DESTRUCTIVE: leaves the current session ---
    "escape":      (1,   "ESC -> MainMenuTransition — ABANDONS the running race"),
}

# Keys that tear down the current session; require --confirm.
DESTRUCTIVE = {1}
CAMERA_CYCLE = {74, 78, 156}  # these visibly move SpectatedVehicleSlot


ARMED = struct.pack("<i", 1)
CLEAR = struct.pack("<i", 0)


def inject_once(game, dik, timeout=0.5):
    """Inject exactly ONE logical press.

    The handler does not clear the count itself (only the game's own per-frame
    input poll does), so an armed count stays live across consecutive frames and
    gets acted on repeatedly — that is why a naive hammer loop cycled 8 cars.
    So: arm for at most ~one frame, disarm, then check. Repeat only if the frame
    was missed entirely.
    """
    before = game.read_val(SLOT, "int32")
    deadline = time.time() + timeout
    attempts = 0
    while time.time() < deadline:
        attempts += 1
        game.write(Q_KEYS, struct.pack("<h", dik))
        game.write(Q_COUNT, ARMED)

        if dik in CAMERA_CYCLE:
            changed = False
            spin_end = time.time() + 0.003          # ~one frame at 300fps
            while time.time() < spin_end:
                if game.read_val(SLOT, "int32") != before:
                    changed = True
                    break
            game.write(Q_COUNT, CLEAR)              # disarm before another frame
            if changed:
                return True, attempts
        else:
            time.sleep(0.003)
            consumed = game.read_val(Q_COUNT, "int32") == 0
            game.write(Q_COUNT, CLEAR)
            if consumed:
                return True, attempts
        time.sleep(0.01)
    return False, attempts


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]

    if cmd == "keys":
        for name, (code, desc) in KEYS.items():
            print(f"  {name:14s} DIK {code:3d} (0x{code:02x})  {desc}")
        return

    args = sys.argv[2:]
    times = 1
    positional = []
    it = iter(args)
    for a in it:
        if a == "--times":
            times = int(next(it))
        else:
            positional.append(a)
    if not positional:
        sys.exit(f"'{cmd}' needs a key name (see: gkey.py keys)")

    if cmd == "press":
        name = positional[0].lower()
        if name not in KEYS:
            sys.exit(f"unknown key '{name}' — see: python tools/gkey.py keys")
        dik = KEYS[name][0]
    elif cmd == "raw":
        dik = int(positional[0], 0)
        name = f"DIK {dik}"
    else:
        sys.exit(f"unknown command '{cmd}'")

    if dik in DESTRUCTIVE and "--confirm" not in args:
        sys.exit(f"'{name}' (DIK {dik}) tears down the running session "
                 f"(ESC -> MainMenuTransition). Re-run with --confirm if that is intended.")

    game = poke.Game()
    for i in range(times):
        ok, attempts = inject_once(game, dik)
        slot = game.read_val(SLOT, "int32")
        status = "consumed" if ok else "NOT consumed (timeout)"
        print(f"  press {i + 1}/{times}: {name} (0x{dik:02x}) {status} "
              f"after {attempts} attempt(s), slot={slot}")
        if times > 1:
            time.sleep(0.15)


if __name__ == "__main__":
    main()
