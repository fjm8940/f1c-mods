# Free Cam — F1 Challenge 99-02

Free camera mod for `F1 Challenge Online.exe` (non-ASLR, image base 0x400000).
Sandbox mod (`production=false`), developed 2026-08-08 through live
in-game validation. This document is the complete engineering handoff: every
live-discovered fact, every dead end, and the current state.

---

## STATUS (end of session 2026-08-08 night — third pass, live battle)

| Capability | State |
|---|---|
| Toggle (F) | **WORKS** (raw-input keyboard path + focused-poll fallback — see Input saga) |
| Mouse look (yaw/pitch) | **WORKS** (raw deltas + EMA smoothing, `MouseSmooth` ini, default 30 ms) |
| WASD movement | **WORKS** (log-proven: camera position moves; W/A/S/D carried by raw-input path, `keys:` W=1 raw=2 live-proven) |
| Q/E vertical | **WORKS** |
| Look keys (arrows) | implemented (raw + poll), untested |
| Shift/Ctrl sprint/slow | **wired** (poll never read Shift/Ctrl; now mapped in the raw keyboard path) |
| Camera POSITION override | **WORKS** (`Rv18c=` = ours, live-proven; renderer reads receiver+0x18C) |
| Camera ORIENTATION (free look) | **BROKEN**: the mode-2 integrator reads the +0x15C matrix back next frame; ANY write to it (matrix rebuild OR euler-arg patch) mirrors the camera across the origin and eventually corrupts a call target → crash. Position-only build is the safe state. See Camera render saga. |
| Car input blocking | **THREE attempts**: (1) AIMainUpdate hook — never matched the player car (player input vehicle 0x7C54E0 doesn't flow through AI tick; throttle stayed 1.00). (2) Per-frame zeroing of 0x7C54E0 factors — didn't hold (later writer). (3) Physics-skip hooks on 0x4441F0/0x4C73F0 — installed; untested conclusively (game session instability). |
| Game crashes on unload | **FIXED** (two bugs: input thread ran after DLL free; trampolines freed under in-flight calls) |

**Session-level instability (unsolved):** repeated APPCRASH c0000005 / c0000096 at
"unknown" modules (EIPs 0x383436, 0x00E11000, 0x00E9000C, 0x0527000C), including a
file-access error on session load. The game has a heavy pre-existing plugin stack
(native_sc*.dll, fuel_fire_lab.dll, Lua.dll, SteeringWheel.dll, Mp3dec.asi) and a
crash history in WER that predates this mod's session (14:11/14:33 same day). It is
NOT proven that these crashes are the mod's fault — the game also crashes on session
entry with the mod loaded but freecam never toggled. Next session should first run a
control: launch + session WITHOUT injecting freecam at all.

Known open items at the bottom.

---

## ARCHITECTURE — four hooks + one input thread

### 1. `0x00409A00` CameraModeViewportUpdate (camera apply) — `camera_apply_hook`
- Runs the original FIRST, then drives the freecam state per frame:
  `frame_delta_seconds()` + `update_freecam()` (input integration, movement).
- The render ENTRY of this function (`director[4..12]` per mode, `mode = director[13]`)
  is **NOT** what the renderer reads — writing it does nothing visible (proven live,
  "camera teleports underground" build). Its post-original write of entry+16 (Y) landed
  but the renderer ignored it.
- Instrumentation: `entry:` dump of the game's own camera state every 60 calls.

### 2. `0x00541CD0` camera transform funnel — `camera_funnel_hook`
- THE render path. Every camera apply funnels through it.
- Stub ABI (PE-verified disasm): thiscall, ECX receiver + 6 stack args:
  `541DB0(receiver, a4, a5, a6)` (euler→matrix into `receiver+0x15C`, euler
  store into `+0x30/34/38`); then `541D10(receiver, a1, a2, a3)` stores the
  vector at `receiver+0x24/28/2C` and writes `R·v` at `receiver+0x18C/190/194`.
- **The renderer reads `receiver+0x18C` as world x/y/z** — live-proven again
  2026-08-08 night: with freecam active, `Rv18c=` = our position and the
  on-screen camera moved to it.
- **THE MATRIX IS POISON (night session, three builds live-proven):**
  - Build A: post-original `541DB0` matrix rebuild → mirror (entry pos =
    −(car pos)) + crash.
  - Build B: pre-original euler-arg patch (a4,a5,a6) → the funnel's own
    541DB0 writes the SAME matrix → mirror persists + crash at 0x383436
    (twice, same EIP).
  - The mode-2 integrator (vtable+24 call inside `CameraModeViewportUpdate`
    0x409A00, mode 2 path) reads `+0x15C` back next frame to compute the
    camera position. ANY write to the matrix (direct or via args) mirrors
    the camera and eventually corrupts a call target → wild-call crash.
  - **The `+0x24` readback claim in earlier sessions was mode-specific /
    incomplete: the integrator ALSO consumes the matrix.**
- **Current safe state (Build C): position-only.** Run original, then
  overwrite `+0x18C..0x194` = our world x/y/z. NEVER touch the matrix, NEVER
  patch args. Consequence: camera position follows freecam, view orientation
  stays the game's own (free look needs a different mechanism — see open
  items).
- Vehicle receivers are SKIPPED (`receiver_is_vehicle`): 0x409A00 mode 2
  calls the funnel a second time with the chassis world transform
  (`ChassisWorldTransform` path). Writing freecam position into a chassis
  makes the CAR follow the camera and speed up when the mouse is shaken
  (live-proven).
- The render view matrix is built per-frame at 0x548C20 (mode-2 view render):
  it reads camera `+0x20/24/28` (pos) + `+0x2C/30/34` (euler) via
  0x54B420/0x54B4A0, then calls `541DB0(camera, π/2, -yaw, 0)` and pushes the
  matrix via `CameraProjectionUpdate` 0x541B70 (`EnumToVTableCallMapper(2,
  this+348)`). Free-look must hook THIS path (e.g. patch the euler at
  +0x2C/30/34 or the π/2/yaw args at 0x548CB2/0x548CBC), not the funnel.

### 3. `0x0040BE70` KeyboardInputActionMapper — `keyboard_mapper_hook`
- Drains the in-race pending-key queue (`word_7CB21C` DIK scancodes,
  `dword_7CB218` count; FACT finding `keyboard-action-queue-background-input`).
- While freecam is active: zero `dword_7CB218` so the game's action handlers
  (camera cycling, ESC, driving aids) see nothing.
- The queue is NOT a full keyboard state: it only ever contains the game's own
  mapped keys (Q/E/F/camera keys/F1-F10 appeared; W/A/S/D/arrows/numpad 4/6/8/2
  never appeared, even when held — proven live).

### 4. Car input blocking — three attempts, current = physics skip
- **Attempt 1 (AIMainUpdate 0x424E70 hook): REJECTED.** AIMainUpdate writes
  ThrottleFactor (+0x357C)/SteeringFactor (+0x3580)/BrakeFactor (+0x3584)
  every frame, but the PLAYER car does not flow through it: the player's
  DRIVER INPUT vehicle is the static struct at 0x7C54E0 (G_postProcessInput —
  DriverInputController 0x451720 reads the factors from it), and `ctl:`
  throttle stayed 1.00 with the hook installed (live-proven).
- **Attempt 2 (per-frame zeroing of 0x7C54E0 factors): REJECTED.** Did not
  hold; a later writer re-populates them before physics reads (ctl: showed
  0.21→1.00).
- **Attempt 3 (physics skip — current):** hooks on `VehicleMainPhysicsUpdate`
  (0x4441F0, `55 8B EC 83 E4 F8 81 EC`) and `VehiclePhysicsTimeStep`
  (0x4C73F0, `81 EC 8C 00 00 00 D9 84 24 94 00 00 00`). While freecam active
  + `BlockCarInput=1`, the ORIGINAL is skipped for the player vehicle
  (`is_player_vehicle` = 0x7C54E0 or *0x7C54E8): physics fully disabled, the
  car freezes in place (user request: "disable the physics on my car when I
  press F"). Vehicle array base 0x5EC6D8, stride 0x48C0 (disassembled at call
  site 0x4F0BDB: `imul ecx,ecx,0x48C0; add ecx,0x5EC6D8`). Untested
  conclusively — session crashes made live verification impossible.

### 5. Input thread (`raw_input_thread`)
- Hidden window (WS_EX_TOOLWINDOW|NOACTIVATE, WS_POPUP, offscreen — NOT
  message-only: WM_INPUT is never delivered to message-only windows, proven).
- `RegisterRawInputDevices` keyboard + mouse with `RIDEV_INPUTSINK`
  (registration confirmed: `reg=1 err=0`).
- Mouse: WM_INPUT deltas → `g_mouse_dx/dy` (event-driven), then an EMA with
  time constant `MouseSmooth` ms per frame before integration (kills
  event-batching jitter; EMA preserves the integral so no travel is lost).
- Keys: WM_INPUT keyboard events (RIM_TYPEKEYBOARD → `g_key_state_raw`,
  RI_KEY_MAKE/BREAK per tracked VKey, incl. Shift/Ctrl) ORed with the 1 kHz
  `GetAsyncKeyState` fallback poll → `g_key_state`. The raw path is
  focus-independent and is the primary path; the poll only adds anything
  when the game window is focused. First raw keyboard event logs
  `rawkbd: ... raw key path live`. W key anomaly RESOLVED: W now registers
  via raw input (`keys: W=1 raw=2` live-proven).
- MUST be stopped (flag + WaitForSingleObject) before the module unloads —
  it runs DLL code.

---

## THE INPUT SAGA (why keys were so hard — all live-proven)

1. **`GetAsyncKeyState`/`GetKeyState`/`GetKeyboardState` return ZERO for every
   key while the game's exclusive DirectInput keyboard is active** — even from
   inside the process, even from a message-pumping thread, even while focused.
   The game's exe imports only `GetKeyboardState` (menu translation); the
   in-race keyboard is read by the game itself (its own DInput device), and the
   OS-level key state is starved. (Frida probe: all keys 0 while held; keys:
   log identical.)
2. **The game's action queue (`0x7CB21C`) only contains the game's own mapped
   keys** (Q/E/F, camera keys 74/78/156/199/201/209/210, F1-F10, ESC, 3/4).
   WASD/arrows/numpad 4/6/8/2 are never queued even when held.
3. **`GetAsyncKeyState` DID work in the earliest builds (1-4)** — those ran
   `update_freecam` inside the FUNNEL hook, and the game window was focused at
   the time. It stopped returning keys after the window lost focus. The focus
   dependence was masked until the input thread's own poll confirmed it:
   **the poll works only while the game window holds focus** (the frida probe
   showed the game is frequently NOT the foreground window — the chat app is).
   "Why did it work just fine before?" → because the game was focused then.
4. **WM_INPUT (raw input) is delivered to a real hidden window** — mouse deltas
   arrive and drive the camera. Keyboard raw input was not conclusively observed
   (the poll path carries keys); a message-only window receives NOTHING.
5. The toggle (F) works only when the game is focused — the foreground gate was
   REMOVED from the code but the OS-side exclusivity still starves the poll when
   unfocused. The game must be clicked before pressing F.

### The car "moves" during freecam — SOLVED (pending live verify)
The queue zeroing blocks the game's ACTION handlers, but the car's driving
input comes from the per-vehicle tick (throttle factor observed 0.02-0.30
while freecam active). `AIMainUpdate` (0x00424E70) rewrites
ThrottleFactor/SteeringFactor/BrakeFactor every frame — that is why poking
the fields lost. The mod now hooks that writer post-original and zeroes the
three factors for the player vehicle while freecam is active (config:
`BlockCarInput=1`). Live check: `ctl:` log must stay at 0.00 while active;
if not, the player-input pipeline writes later in the tick and is the next
hook candidate.

---

## THE CRASHES (two real bugs, both fixed)

1. **Input thread ran after `FreeLibraryAndExitThread`** — the input thread's
   code lives in the DLL; the unload sequence freed the module while the thread
   still looped → next instruction = freed memory → process death. Every
   `unload` in the deploy cycle killed the game ("game died" incidents).
   Fix: `g_input_thread_stop` flag + `WaitForSingleObject(thread, 3000)` before
   any unload work.
2. **Trampolines freed under in-flight hook calls** — `f1c_hook_registry_remove_all`
   restores the bytes AND frees each trampoline; the funnel hook fires ~1000×/s,
   so a call mid-trampoline read freed memory. Fix: detach preserving trampolines
   (restore bytes manually), drain `g_active_calls`, THEN free the trampolines.

### Deploy loop trap
`stage` fails with WinError 32 while the DLL is loaded (file lock). The unload
takes ~3 s (input-thread wait). Sequence that works:
`unload → sleep 5s → stage → inject`. Never stage while the game has the DLL
loaded; a zombie worker (crashed) holds the lock until the game restarts.

---

## CONTROLS

| Key | Action |
|---|---|
| F | Toggle freecam (raw-input path is focus-independent; poll fallback needs game focused) |
| W / S | Forward / back (along the camera's yaw) |
| A / D | Strafe left / right |
| Q / E | Down / up |
| Mouse | Look (raw input deltas) |
| Arrows | Look (poll path; untested live) |
| Shift / Ctrl | Sprint (×4) / slow (×0.25) |

Movement is world-space rotated by the camera's own yaw (screen-relative).
Y is world-up. Spawn = player vehicle world pos + 3 m.

## CONFIG (`freecam.ini`, next to the DLL)

`ToggleKey` (VK, currently unused by code — F is hardcoded as KB_F),
`MoveSpeed` (15 m/s), `SprintFactor` (4), `SlowFactor` (percent/100),
`YawSpeed`/`PitchSpeed` (rad/s ×10), `SpawnHeight` (3), `MouseLook` (1),
`MouseSens` (rad/px ×100000), `MouseSmooth` (ms EMA, 30, 0=raw),
`BlockCarInput` (1), `LogEvery`.

## BUILD / DEPLOY

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/freecam
python tools/f1c_mod.py doctor --exe "%USERPROFILE%\Desktop\F1 Challenge V10\F1 Challenge Online.exe" --require-exe
python tools/f1c_mod.py unload F1C_FREECAM_UNLOAD
# wait 5 s (unload drain + file lock release)
python tools/f1c_mod.py stage mods/sdk_sandbox/freecam "%USERPROFILE%\Desktop\F1 Challenge V10" --exe "...\F1 Challenge Online.exe" --replace-sdk-owned
"%USERPROFILE%\Desktop\F1 Challenge V10\inject.exe" "...\freecam.dll"
```

Log: `freecam_log.txt` next to the DLL (`keys:` = input state every 60 frames,
`entry:` = game camera state, `ctl:` = throttle/brake/queue, `CRASH:` =
unhandled exception record). The DLL refuses incompatible PE builds; the hook
install fails closed on byte mismatch.

## OPEN ITEMS (next session)

1. ~~W key anomaly~~ — **FIXED + live-proven**: WM_INPUT keyboard path
   implemented; `keys:` shows W=1 raw=2 with W held.
2. ~~Mouse jitter~~ — **ADDRESSED**: per-frame EMA (`MouseSmooth` ms,
   default 30; 0 disables). Verify feel live; tune if still coarse.
3. **Car input blocking (physics freeze) — NOT LIVE-VERIFIED**: hooks on
   0x4441F0/0x4C73F0 skip the player vehicle's physics while freecam active
   (`BlockCarInput=1`). The game's session crashes prevented a clean test.
   Next: control test first (launch WITHOUT inject, session entry) to
   separate game/plugin instability from mod crashes. Then test with
   blockcar=1: car must freeze in place while other cars race.
4. **Camera free-look (orientation) — THE remaining camera problem**: the
   mode-2 integrator consumes the +0x15C matrix, so the funnel must not
   write it (position-only is safe). The view matrix is built per-frame at
   0x548C20 from camera +0x20/24/28 (pos) + +0x2C/30/34 (euler) via
   0x54B420/0x54B4A0, then `541DB0(camera, π/2, -yaw, 0)` → projection.
   Candidate fix: hook 0x548C20's matrix-build args, or write the euler
   at camera +0x2C/30/34 post-integrator (that field is NOT the integrator
   input). Verify with the `funnel:` diag + on-screen view.
5. **Session crash control**: game crashes on session entry even with the
   mod injected and freecam never toggled (EIPs 0x383436, 0x00E11000,
   0x00E9000C, 0x0527000C — all "unknown" module = wild calls past freed
   plugin space; one c0000096 + one file-access error). WER shows crashes
   BEFORE this mod's session too. Run a no-inject control to establish
   the game's baseline stability with its plugin stack (native_sc*,
   fuel_fire_lab, Lua, SteeringWheel, Mp3dec.asi).
6. ~~Keyboard raw input~~ — done (item 1).
7. **FOV control** — entry+72 (fov) exists but the render path ignores the
   entry; the FOV for the renderer lives elsewhere (CameraProjectionUpdate
   0x541B70 reads a camera object; not yet mapped).
8. **Promote findings** — `Utility_Function_541cd0` (0x541CD0),
   `Orientation_Rotation_Matrix_Update` (0x541D10),
   `Random_Rotation_Matrix_from_Euler_Angles` (0x541DB0),
   `Keyboard_Input_Action_Mapper` (0x40BE70) are LEGACY-tier in the SDK; the
   live evidence in this document + findings.jsonl supports CONFIRMED.
   New evidence for the SDK: vehicle array base 0x5EC6D8 stride 0x48C0
   (call site 0x4F0BDB), VehicleMainPhysicsUpdate 0x4441F0 +
   VehiclePhysicsTimeStep 0x4C73F0 as the player physics skip points,
   player DRIVER INPUT vehicle = G_postProcessInput 0x7C54E0.
