# Driving the game from a script — the full control surface

How to observe and control a running `F1 Challenge Online.exe` **without touching
OS focus**, so experiments can run unattended while the machine is used for
something else. Established live 2026-07-27; every claim here was verified on
screen, and the dead ends are listed so nobody re-derives them.

Two independent input paths, because the game has two:

| Target | Tool | Mechanism |
|---|---|---|
| In-race actions (camera, driving aids, ESC) | `tools/gkey.py` | write the game's own pending-key queue |
| Menus / UI screens (buttons) | `tools/gmouse.py` | `PostMessage` mouse messages |
| Any game state with a known global | `tools/poke.py` | `WriteProcessMemory` |
| Seeing the screen | `tools/screenshot.py` | `PrintWindow` (DWM), game window only |
| Booting into a race with no menus | `tools/lab.py autolaunch quickrace` | `config.ini [AUTOLAUNCH]` |

None of these steal focus. `tools/gamekey.py` (Interception driver) is the one
exception — it is real hardware-level input, so it **does** hijack the desktop.
Keep it only as a fallback.

---

## 1. Sight — `tools/screenshot.py`

```bash
python tools/screenshot.py tools/shot.png
```

Captures the game window only (not the desktop), via `PrintWindow` with
`PW_RENDERFULLCONTENT` — plain BitBlt returns black for a DirectX window. Pure
GDI read: no focus change, no input, safe to call at any time. Output pixels are
client coordinates, so a target's position can be read straight off the image and
handed to `gmouse.py`.

## 2. In-race keyboard — `tools/gkey.py`

**FACT.** `KeyboardInputActionMapper_0040be70` and friends drain a shared queue
of pending DIK scancodes every frame:

* `word_7CB21C[]` — int16 queue of scancodes
* `dword_7CB218` — number of pending entries

Writing a scancode + count makes the game act exactly as if the key were pressed.
This is arming the game's own trigger, not forging an end state.

```bash
python tools/gkey.py keys                 # the whole mapped table
python tools/gkey.py press add            # numpad + : next car
python tools/gkey.py press numpadenter    # back to your own car
python tools/gkey.py press f6             # toggle Invulnerability
python tools/gkey.py raw 78               # any scancode
```

The queue has **five independent consumers**, each handling its own scancodes:

| Consumer | Scancodes | What |
|---|---|---|
| `KeyboardInputActionMapper_0040be70` | 74, 78, 156, 199, 201, 209, 210 | spectate camera, camera modes |
| `sub_42C440` | 59–68 (F1–F10) | driving aids incl. **F6 Invulnerability** |
| `RestartGate_004a0fd0` | 1 (ESC) | `MainMenuTransition` — **abandons the race** |
| `sub_4D8450` | 3, 4 | broadcast / display overlays |
| `MouseInputHandler_0045f030` | `0x1000\|DIK` for keys 1–0 | UI list select (note the flag bit) |

**Gotcha — the count is not self-clearing.** The handlers read the queue but only
the game's own per-frame input poll resets `dword_7CB218`. An armed count is
therefore re-processed on every frame until it is cleared: a naive hammer loop
cycled 8 cars from one intended press. `gkey.py` arms for ~one frame then
disarms. Even so, injection is reliable as a **trigger** but not precisely
countable — for an exact car, write the slot directly instead (see §4).

## 3. Menus / UI — `tools/gmouse.py`

**FACT.** UI screens are driven by ordinary window messages, so posted mouse
messages click buttons with no focus and without moving the real cursor.

```bash
python tools/gmouse.py rect                     # client size
python tools/gmouse.py click 2453 1396          # client pixels
python tools/gmouse.py click --norm 0.96 0.97   # resolution-independent
```

The key queue does **not** drive these screens — ESC on RACE RESULTS did nothing.
UI is mouse; in-race actions are the key queue. Don't mix them up.

### Don't click blind — the game names what is under the cursor

**FACT.** `MessageDisplayBuffer` (0x5EC4CC) holds an ASCII **UI label**: the
screen title (`SELECT CIRCUIT`), a button label (`accept`, `loading`), or — most
usefully — **the name of the item currently hovered** (`ITALY, MONZA`,
`AUTODROMO JOSE CARLOS PACE`). So the game tells you what a coordinate *is*; you
never have to trust a pixel guess.

```bash
python tools/gmouse.py screen                              # what is under/showing now
python tools/gmouse.py click 1805 960 --item "ITALY, MONZA" # hover, verify, then click
python tools/gmouse.py probe 90                            # map labels -> coordinates
```

`--item` is the primitive that makes autonomous navigation safe: it hovers,
reads the label, and **refuses to click** if the wrong thing is under the cursor
(verified live — asking for `SUZUKA` over Monza's tile aborted). Prefer it to a
bare `click` for anything consequential.

Two traps, both hit live:

* **The buffer is sticky and lags ~1 frame.** Over empty space it keeps the last
  label. `probe` therefore bounces off a neutral point and only accepts a label
  that *changed*; even then attribution can be off by a cell, so treat
  `ui_map.json` as a lead and confirm with `--item` before clicking.
* **Screenshot pixels are not client pixels.** A capture may be displayed scaled
  (e.g. 2000x1125 shown for a 2560x1440 client). Feeding displayed coordinates
  straight in put me a whole column off. Use `--norm`, or scale by
  `client / displayed` — and let `--item` catch the mistake.

### DISPROVEN — mouse position is not writable control

`0x7CB25C` / `0x7CB260` mirror the cursor X/Y exactly (verified: posting a move to
640,360 then 1900,200 made them read 640/360 then 1900/200). But they are
**outputs the game writes, not inputs it reads**: poking them onto a circuit tile
and then posting button-only messages with `lParam=0` selected nothing. The
coordinates must travel inside the message, so menu actions need a real posted
click. Unlike the in-race keyboard, there is no mouse command queue to arm.

> ### !! NEVER CLICK THE TELEMETRY ICON !!
> User rule: it is a redundant feature and must never be used. It lives in the
> bottom-left icon strip, so `gmouse.py` refuses clicks there unless `--force`
> is passed. Do not pass `--force` in that strip without checking the screenshot
> first.

## 4. State — `tools/poke.py`

Direct memory read/write for anything with a known global or vehicle-struct
offset. Deterministic and instant, and the right choice whenever a backing value
exists.

```bash
python tools/poke.py write 0x7C54DC:int32 8     # spectate car in slot 8, exactly
```

**But arm triggers rather than forging state.** Writing `0x7C54DC = 0` holds the
value yet does **not** return the camera to the player's own car, because the
index is only an input to the camera targeting. `gkey.py press numpadenter`
(DIK 156 → `CameraTargetCurrentTrackedVehicle`) does work. Non-zero slots are
fine by direct write.

Check `persistence_map.jsonl` before relying on any poke, and never write
`veh+0x10C0` or `veh+0x33C8` (each kills the process instantly).

## 4b. Choosing the track WITHOUT the menus — `tools/session_setup.py`

Preferred over clicking, because it has no coordinates in it at all and is
therefore immune to resolution/layout changes.

**Can menu selection be done with memory writes? NO — tested.** Selecting a track
does change three globals, but they are records the game *writes*, not inputs it
reads:

| Global | After selecting Albert Park |
|---|---|
| `CurrentTrackPath` 0x7CE5D0 | ptr → `SEASONDATA\CIRCUITS\Australia\2001_Albert_Park\2001_Albert_Park.SCN` |
| `SettingsFilePath` 0x7CE5EC | ptr → `2001_Albert_Park` |
| `CurrentSceneIndex` 0x8AC48C | `10` |

Writing `CurrentSceneIndex` (10 → 14) was simply ignored: the selection stayed on
Albert Park. The UI's live selection lives in heap widget objects, not in a
global, so there is nothing to poke.

What the menu *actually does* is write the player file — so write that instead:

```bash
python tools/session_setup.py list            # 67 tracks found on disk
python tools/session_setup.py show            # current [ SCENE ] selection
python tools/session_setup.py set 2001_Monza  # rewrite it (byte-exact backup first)
python tools/session_setup.py restore
```

It rewrites only the `[ SCENE ]` keys in `Save\test\test.PLR`
(`Scene File`, `Scene Description`, `AI Database File`). The same file also holds
per-session-mode settings under `QUICK`/`GPRIX`/`CHAMP`/`PRACT`/`RPLAY` prefixes
(Weather, Failure Rate, Reconnaissance, …) plus `Damage Multiplier` and
`Race Session`, so most session parameters are file-settable too.

> Caveat: the game reads the player file at start-up and may rewrite it on exit.
> Set it while the game is **closed**, then launch — don't edit a running game and
> expect it to stick.

## 4c. Menu map (from a guided tour, 2026-07-27)

Walked with `tools/tour.py` (screenshots every 2s + beep) from the RACE/driver
screen through Options and into a live race. Screens confirmed, each identified
by its `MessageDisplayBuffer` / header text — not by pixel guessing:

```
RACE (driver select)  →  [some icon]  →  CONTROLLER ASSIGNMENTS
                                       →  DISPLAY
       ↓ ACCEPT
SELECT CIRCUIT  →  SESSION OPTIONS
       ↓ LOAD CIRCUIT
(loading splash — event program cover art)
       ↓
VEHICLE SETUPS (folder browser: country → session .set files)
       ↓
TYRE PRESSURE AND CAMBER  (one tab of a garage/setup section)
       ↓
track flythrough (pre-race cinematic)
       ↓
grid walk (pit-lane girls, numbered boards, standing cars)
       ↓
starting grid overlay (P1–P4 names/teams)
       ↓
GO — lights out, into the race (lap 1/58)
```

**Structural pattern, consistent across every menu screen:**
- Header bar = current section title (`SELECT CIRCUIT`, `DISPLAY`, `VEHICLE SETUPS`…)
- Bottom-right label = current screen/button name — this is `MessageDisplayBuffer`,
  so `python tools/gmouse.py screen` tells you exactly which of these you're on
  without a screenshot.
- Bottom icon strip = a tab bar between sibling screens in the same section (e.g.
  Controls has keyboard/wheel-device icons on the left, then a shared
  back-arrow/Controls/Display/Sound/etc. cluster on the right; the Garage/Setup
  section has its own tab set — tyre pressure, springs, folder browser, wrench).
  The **active tab is highlighted green**. The bottom-left telemetry icon rule
  (§ above) applies to every one of these strips — check before clicking there.
- `◁ Name ▷` arrows (seen on the driver screen) cycle through a list (drivers) in
  place, same pattern as other left/right steppers (`NUMBER OF AI DRIVERS`).

**Confirmed screen names to use with `--item`/`--expect`:** `RACE`,
`CONTROLLER ASSIGNMENTS`, `DISPLAY`, `SELECT CIRCUIT`, `SESSION OPTIONS`,
`VEHICLE SETUPS`, `TYRE PRESSURE AND CAMBER`, and every circuit's own name (see
§4b's `probe`/`clickname`).

**Not yet mapped:** the exact icon → screen names for the rest of the OPTIONS
and GARAGE tab strips (Sound, Vehicle setup tabs beyond tyres, replay/telemetry
screens on the garage side, championship/season screens). Same method applies:
`tour.py` while browsing, then `gmouse.py screen` per shot to label each one —
faster with a human driving than probing blind.

`python tools/lab.py autolaunch quickrace` arms `config.ini [AUTOLAUNCH]` so the
game boots straight into a race after `LAUNCHTIME` seconds. Combined with
data-driven session setup (`.plr`/`.gdb`/`.hdv`, see `DATA_INDEX.md`), most
research loops never need menu navigation at all. `lab.py autolaunch off`
restores the byte-exact backup.

`mods/no_tab_pause/` keeps the game running while unfocused (subclasses the
WndProc and forces the activation messages), and
`mods/no_tab_pause/watch_inject.py` re-injects it automatically on every launch.

---

## DISPROVEN — do not retry (all tested 2026-07-27)

| Route | Result |
|---|---|
| `SendInput` (VK **and** scancode) | game never sees it; `InputActionMap` unchanged |
| `PostMessage WM_KEYDOWN` | no effect, even with correct lParam scancode |
| `IDirectInputDevice8::GetDeviceState` | hook verified installed, **0 calls** in-race |
| `IDirectInputDevice8::GetDeviceData` | hook verified installed, **0 calls** in-race |
| `user32!GetKeyboardState` (IAT hook) | hook verified installed, **0 calls** in-race |

DirectInput8 is present only for the wheel / force feedback. The exe's import
table is the fast tell: `GetKeyboardState` present; `GetAsyncKeyState`,
`GetKeyState`, `GetRawInputData`, `GetCursorPos`, `SetCursorPos` all absent.

Diagnostic sources kept as evidence: `mods/vkeys/vkeys_dbg.c` (call counters),
`mods/vkeys/vkeys_kb.c` (IAT hook). `tools/input.py` and `tools/vkey.py` are dead
ends retained only for the record — **do not build on them.**

Lesson: instrument before iterating. Two hooks were provably installed yet never
called; counting calls settled in one build what three blind builds could not.
