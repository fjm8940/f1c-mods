# AI Brain Lab — live control of AI driver behaviour (development)

> **Shareable app plan (2026-10-03):** [SHARE_PLAN_2026-10-03.md](SHARE_PLAN_2026-10-03.md) - one folder, run.bat +
> system Python, unified viewer + lab window, presets / experiments / share zips. Owner decided: public, everything,
> offline single-player only -> multiplayer refusal is a release blocker (DeepSeek batch E).

. Development product, created 2026-10-03 by Claude at the owner's request.

**Status: BUILT + offline tests PASS (owner-run 2026-10-03); NOT INSTALLED, NOT LIVE-TESTED.** Pipeline stage:
verify (offline done) -> human validate (live) pending.

## Verification record

**2026-10-14 Lightloader Mods list version (1.0.6 candidate, NOT shipped):** `dll\ai_brain_lab.meta.json` is embedded as `F1C_MOD_META` (`dll\ai_brain_lab.rc`, `build.bat` links it), so the manager shows the name and version. Code unchanged; the plug-in bytes change, so the package must be 1.0.6. Build in `%TEMP%`, offline tests PASS, nothing published. [Record](META_VERSION_EMBED_2026-10-14.md) and [release prep](../../lightloader/v2.0.0-hub-dev.1/release_toolkit_1.0.6_2026-10-14/MANIFEST.md).

**2026-10-08 Driver AI Toolkit 1.0.5 SHIPPED** (owner "ship this"): build `releases/driver_ai_toolkit/1.0.5-r3`, zip `9835C60A...`, catalog `8AF04A38...`, served bytes verified. Contents = the 1.0.5 redesign + F1.exe attach entries below. [Release record](../../lightloader/v2.0.0-hub-dev.1/release_toolkit_1.0.5_2026-10-08/MANIFEST.md).

**2026-10-08 F1.exe support (1.0.5-r2 candidate):** the viewer now finds the game by its code fingerprint instead of the exe name, so `F1.exe` (Desktop copy, SHA-256 `D8FB5722...`) attaches; every plug-in / viewer address is byte-identical in that exe (71 checked, 0 different). Mocked attach test 9/9, package `2C57DDC9...`, plug-in unchanged. Live run of F1.exe pending. [Record](F1EXE_COMPAT_INVESTIGATION_2026-10-08.md).

**2026-10-08 Driver AI Toolkit 1.0.5 (candidate, display scaling + visual redesign):** player report of cut-off text in the drawer on scaled displays (fixed 420 px column vs DPI-scaled fonts). Every size now follows the Windows display scale, the drawer divider can be dragged (width saved), and a shared flat theme (`viewer/ui_theme.py`) replaces the default Tk widgets; no information removed. `test_abl_ui.py` 25/25 PASS, renders checked at 100 % and 150 %, package `AEDEF5FE...`, plug-in unchanged `6C60B666...`. Not installed; owner look-and-feel + live check pending. [Record](UI_REDESIGN_V2_DPI_2026-10-08.md).

**2026-10-08 Driver AI Toolkit 1.0.3-r2 (candidate, UI redesign):** containers ONE DRIVER / SELECTED DRIVERS / WHOLE GRID with per-driver proof (plug-in status + value read back from each car), unmoved sliders keep every driver's own value (fixes the old copy-to-all), ready-made presets + load at every session start, SIMPLE / ADVANCED explanations, address audit, latent `abl_shm.py` RULE_MASK_OFF bug fixed. Package `2E104E95...` (plug-in `6C60B666...`), `test_abl_ui.py` 25/25 PASS. Not installed; owner live check pending. [Record](UI_REDESIGN_AND_ADDRESS_AUDIT_2026-10-08.md).

**2026-10-08 Driver AI Toolkit 1.0.3 (candidate, Batch C / COMP-02):** the plug-in now skips an AI car before
writing any lever or hold while the SC/RF race-control block owns the field, or while Mechanical Failures has
marked that car limited (append-only ABI 2 `mf_limited` record). Precedence SC/RF > MF > Toolkit (owner
2026-10-08). Read-only: the Toolkit never writes the race-control block. DLL `1596E305...` (build.bat: rules ok,
`abl_logic_test` 130/0, `abl_precedence_test` PASS, `-Werror`), package `7928FA06...`.
[Batch C record](../COMP02_PRECEDENCE_2026-10-08.md). Not installed; live validation is an owner/combined-test step.

**2026-10-04 Driver AI Toolkit 1.0.2 (published):** players reported that Pit Stop Failures stops working with the
toolkit. Cause: the plug-in patched four call sites inside AIMainUpdate (0x4253EE/403/411/418) at load, and Pit
checks those bytes at startup and refuses. 1.0.2 patches them at the first AI tick (from the 0x42C748 wrapper);
`DriverAIToolkit.bat` starts the app from `%TEMP%` so Lightloader can update the folder while it is open. DLL
`6d9799fb...` (build.bat: rules ok, 130/0 logic, `-Werror`), package `DC7C1624...`. Owner in-game pass 2026-10-04 with Pit running alongside.
[Release record](../../lightloader/v2.0.0-hub-dev.1/release_2.2.1_2026-10-04/MANIFEST.md).

| Date | Check | Result |
|---|---|---|
| 2026-10-03 | `dll/build.bat` (owner shell): logic unit test | `abl_logic_test: 40 checks, 0 failed` |
| 2026-10-03 | `dll/build.bat`: DLL build (-Wall -Wextra -Werror) | built, no warnings |
| 2026-10-03 | `tools/test_abl_shm.py` (owner shell): Python layout vs C header | `0 failures` |
| 2026-10-03 | artifact `dll/ai_brain_lab.dll` (Claude, pefile read) | SHA-256 `f2e04ce5ef461e23404fb5da70e3700b2ceea020f1f6ab40133eb31dcd9836c7`; i386; PE timestamp 0 (reproducible); exports only `rfe_1d3b7a91`; imports KERNEL32 + msvcrt |
| 2026-10-03 | DS4-prep boot test (real Windows mapping, refactored init) | `abl_logic_test: 126 checks, 0 failed`; negative controls: v2-size view -> 7 FAIL, sub-header at main 0/4/8 -> 5 FAIL, baseline -> 0 FAIL |
| 2026-10-03 | `dll/build.bat` after the boot refactor | rules 4/4, gen_levers 29/15/14, test_abl_shm 0 failures, logic 126/0, DLL built clean (`-Werror`); dev DLL SHA-256 `81498134658856e9741b3cb8cd289938e3d68c9293f8e78856e6478b78a55908` (NOT installed) |

Game target: F1 Challenge Online.exe SHA-256 1A70E5ED (Desktop copy and Documents install identical).

Live 1 (2026-10-03, owner race, Claude read-only checks): installed DLL = F2E04CE5 in `plugins\lightloader`; log
`ACTIVE (Lightloader verified)` + 5 x `HOOK installed`; shared memory header magic OK, hooks 0x1F, ENABLE 1, ticks
advancing ~40/s per running car. Levers DID reach cars: car 12 pace held at 0.95 (its 0x453C = client value), car 1
commit filter held 25 overtake attempts. Owner report "nothing really seems to happen": the controlled car (12,
Montoya) was RETIRED (car_conditions 2, 0 km/h, lap 0) so the AI planner no longer ran for it (0 ticks / 2 s) and no
lever can act; the active values were also small (pace 0.95). Whether the preset combination (late braker +
aggression 1.5 + commit + never move aside) caused that start crash is UNKNOWN. Fix: panel banner shows the car's
planner tick rate and turns red "NOT RUNNING THE AI PLANNER" for retired / parked / garage / paused cars.

## Goal (owner, 2026-10-03)

"A separate AI brain DLL through which I can manipulate AI behaviour in real time with buttons, sliders etc. ...
such as when AI tries to overtake but pussies out and drops its overtaking intentions. I want ALL of these
behaviour actions within reach." Community goal: a better AI.

Non-goals (v1): replacing the AI driver, pedal/steer puppeteering (that is mechanical_failures' AI override API),
pit strategy writes (call-to-box with the rear-wing flag retires the car - not exposed), multiplayer.

## Files

| File | Role |
|---|---|
| `dll/ai_brain_lab.c` | the DLL: Lightloader v2 proof entry, code-pattern check, 5 call-site hooks, levers |
| `dll/abl_logic.h` | pure decision logic + shared-memory layout (no game access) |
| `dll/build.bat` | dev build: unit test first, then the DLL (32-bit MinGW, reproducible PE) |
| `tools/abl_logic_test.c` | offline unit test of the logic (40 checks) |
| `tools/abl_shm.py` | Python client of `Local\F1C_AI_BRAIN_LAB_V1` |
| `tools/test_abl_shm.py` | offline check that the Python layout equals the C header |
| `tools/ai_brain_lab_gui.py` | the control panel (buttons, sliders, presets, read-back) |

The Lightloader plugin ABI header is used from `../mechanical_failures/dll/lightloader_plugin_abi.h` (single
source of truth; `build.bat` adds that include path).

## How it works

Five verified 5-byte call-site patches inside the AI tick (no function prologue is touched, so the
mechanical_failures detours on AIMainUpdate 0x424E70 and the auto-shift 0x426330 keep working alongside):

| Site | Calls | Lab work |
|---|---|---|
| 0x4253EE | AIRacingLine 0x425920 | before: pace, cornering add/mult, wet factor, braking factor, brake ramp |
| 0x425403 | AISteering 0x4270A0 | before: commanded line, aggression, snapshot of the 5 opponent records; after: overtake-commitment filter, no-traffic-lift, read-back of 0x4408 / 0x4428 |
| 0x425411 | AISteeringCorrection 0x426CF0 | before: speed governor (lowers only), max speed; after: throttle/brake scaling, park/stop |
| 0x4290CC, 0x42923C | Overtaking 0x42AF70 | cooperation gate |

Verified in the exe (capstone, read-only, 2026-10-03): each site is `E8 rel32` to the named function; the first
bytes of all four callees are checked; AIRacingLine / AISteering / AISteeringCorrection end with plain `ret`
(thiscall, no stack args); Overtaking ends with `ret 8`.

**Overtaking ABI discovery (2026-10-03):** at both call sites ECX = `[edi+0x4174]` = the OPPONENT in the scanning
car's traffic slot; the scanning car and the mode are pushed as stack arguments. So Overtaking asks the *other*
car to side-step (lapped car moves over in a race; in qualifying the flying-lap car steps around the out-lap car).
The "drops its overtaking intention" behaviour is the scanning car's opponent-record mode falling from attempt
(1/2/7/8) back to hold/approach (5/16) inside AISteering; the lab's commitment filter acts there.

Every "hold" lever saves the AI's own value when switched on and writes it back when switched off (or on ARM off /
RELEASE ALL / unload). Every value is bounds-checked in the DLL; out-of-range values are refused and reported.

## Lever table (evidence)

| Lever | Field | What it does | Evidence |
|---|---|---|---|
| Racing line | 0x4424 forced, or 0x4548 + 0x4420 request (+ corridor widen) | moves the line the AI steers to | finding ai-brain-4408-44b4-writers-closed-2026-10-02; spec WRITE_STAGE1_SPEC_DRAFT.md; pull-off chain FACT |
| Pace | 0x453C | scales every corner target | ai-brain-corner-speed-live-confirmed / pace-multiplier-model-live (OBSERVED) |
| Corner speed x / + | 0x4504 / 0x4500 (CORNERINGMULT / ADD) | grip-speed affine terms | same formula, OBSERVED dry + wet + qualifying |
| Wet factor | 0x4580 | rain slowdown factor | ai-brain-wet-factor-4580-live (OBSERVED) |
| Braking confidence | 0x44CC (PASSING) | braking look-ahead loss per step; higher = brakes later | ai-brain-braking-step-live-confirmed (OBSERVED) |
| Brake build-up | 0x44C4 (RACEABILITY norm) | brake-demand ramp step | IR 2026-08-18 (DERIVED) |
| Aggression | 0x44F8 (TRACKAGGRESSION) | AISteering traffic thresholds | IR 2026-08-18 (DERIVED); mechanical_failures writes it live |
| Speed governor | 0x4484 | lower-only speed clamp (reset every tick by the game) | doc 54, write readiness (DERIVED) |
| Max speed | 0x4538 | top-speed cap | mechanical_failures live writes |
| Throttle / brake multiplier | 0x4434 / 0x4438 after AISteeringCorrection | scales the final pedal decision | doc 54 pipeline (output copy after) |
| Overtake commitment | opponent records 0x42D4 (5 x stride 28) | commit (no fall-back), give up at once, never attempt | D6 OVERTAKE_DEFEND (modes OBSERVED, meaning DERIVED) - EXPERIMENTAL |
| Cooperation | Overtaking 0x42AF70 call gate | car never side-steps when asked / never asks others | static ABI above + quali 14/14 live - EXPERIMENTAL |
| No traffic lift | 0x45FC cleared to 0 after AISteering | removes the 0.75 x difficulty throttle cap near traffic | IR 6593 (DERIVED); clearing only, never a pointer |
| Park / stop | 0x4510 = 4 / 7 | native park / stop states | mechanical_failures live (mode 4 owner-validated) |

Never written: 0x45FC (except clearing), 0x3D6C, 0x10C0, 0x33C8.

## Build and test (owner, game closed or running - the build does not touch the game)

```bash
& "%USERPROFILE%\Desktop\F1\development\mods\ai_brain_lab\dll\build.bat"
```

```bash
python "%USERPROFILE%\Desktop\F1\development\mods\ai_brain_lab\tools\test_abl_shm.py"
```

`build.bat` compiles and runs the logic unit test first (must print `0 failed`), then builds
`dll\ai_brain_lab.dll`. Record the SHA-256 of the DLL in this README.

## Install (owner-approved, game closed)

Copy `dll\ai_brain_lab.dll` to `%USERPROFILE%\Documents\F1 Challenge V10\plugins\lightloader\`. Lightloader
loads every DLL there; the lab activates only through the protected proof (no injector path). Log:
`ai_brain_lab_log.txt` in the game folder (ACTIVE line + 5 "HOOK installed" lines expected).
Remove = delete that copied DLL with the game closed (nothing else is changed on disk).

## Use

```bash
python "%USERPROFILE%\Desktop\F1\development\mods\ai_brain_lab\tools\ai_brain_lab_gui.py"
```

Elevated shell (same as the game). Pick a car -> tick a lever -> move its slider -> ARM. Esc / RELEASE ALL hands
everything back. Watch the effect in viewer v3 (`f1c_ida_decompiled/mods/sdk_sandbox/ai_mind_monitor`).

## v2 plan - "ALL controls" (owner 2026-10-03)

Owner: "I want ALL the controls to AI, for example the yielding distance - EVERYTHING that can be manipulated."

- Live 2 (2026-10-03, Coulthard slot 4, read-only check): CORNER x1.50 active -> corner targets +52 % (161 vs 105,
  174 vs 114, 130 vs 85 km/h); he ran off track (lateral -19 m / -24 m) and got car_conditions 1 (-> AIAbility pace
  0.85 x baseline). Lever proven to work; x1.50 is beyond tyre grip. Commit lock held 2 154 attempts.
- Kind 1, per-car fields to add: steering lock 0x44B4 (steering sharpness, init-only = holdable), watchdog limit
  0x4558, pace baseline 0x44F4 / push 0x44D8 / push norm 0x44D4, start ramp 0x44B8, stall chance 0x44BC, crash
  0x44D0, traffic throttle cap 0x457C.
- Kind 2, the AI's built-in rule constants (global, all AI cars): operand redirection. An AI instruction reading a
  fixed-address constant gets its 4-byte address pointed at a DLL float (live slider, restored on release); imm32
  operands are patched in place. Verified example: yield radius 625.0 (25 m squared) at 0x59E988, read only by
  `fcomp` at 0x42A8CA in ProximityScan 0x42A7D0 (same function: -5.5 at 0x59E984, 999998 sentinel at 0x59E980).
- Census of every AI tunable: DeepSeek batch D (`f1c_ida_decompiled/tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_PHASE3_2026-10-02.md`),
  output `investigations/ai_brain_map/phase3/TUNABLES.csv/.md` + generated `ai_tunables.h`; Claude then builds v2
  (generic redirect engine + auto-generated "Rules" tab with families, search, reset) for one owner build.
- Panel safety: corner multiplier slider to be narrowed (0.85-1.20, red above 1.10).

### v2 implemented (2026-10-03, source; owner build pending)

- Shared memory ABI 2 (append-only): rules block (256 x 16 B) + per-car extension block (32 x 64 B); header
  RULES_N / RULES_OK. A v2 panel still opens a v1 DLL (maps the header first, reads its size) and says "rebuild".
- Rules engine (`ai_brain_lab.c`): every site of `abl_rules.h` verified byte-for-byte at startup (a mismatch
  disables only that rule), 10 Hz worker redirects ABS operands to DLL-owned values / rewrites imm32/imm8, restores
  when unticked / ARM off / RELEASE ALL, restores and verifies every site before unload (pins the module if any
  site or hook cannot be verified).
- `dll/rules.json` (curated, append-only order) -> `tools/gen_rules.py` (capstone decode of each site in the
  installed exe, checks operand offset, constant address and value, refuses on any error) -> `dll/abl_rules.h`.
  Seed rules (Claude-verified 2026-10-03): yield radius 625 m^2 (0x42A8CA abs 0x59E988), side-step +3 / -3 m (imm32
  0x42B0B8 / 0x42B0C9), traffic throttle cap 0.75 (0x426D08 abs 0x59E7FC). gen_rules.py run by Claude: 4/4 ok.
  DeepSeek batch D census adds the rest (Claude curates rules.json from TUNABLES.md).
- Per-car "Driver traits": steering lock 0x44B4, watchdog limit 0x4558 (int), pace baseline 0x44F4, push 0x44D8,
  push norm 0x44D4, crash 0x44D0 - hold levers with native save/restore.
- Panel: tabs "This car" / "Rules (all AI cars)" (auto-built from rules.json, status per rule), Driver traits card,
  corner multiplier 0.85-1.20 with "BEYOND GRIP" above 1.10. Fixed a v1 layout bug: the Traffic (aggression) card and
  the Overtaking card shared one grid cell, hiding the aggression slider.
- `build.bat` now runs gen_rules.py + test_abl_shm.py before compiling (no table, no build).
- Checks run by Claude: test_abl_shm 0 failures (v2 layout), py_compile of all tools, control-character scan.
- First owner build failed: gen_rules.py wrote `625f` (invalid C float literal); fixed (always `625.0f`).
- Owner build 2026-10-03 (second): gen_rules 4 rules / 4 sites ok, test_abl_shm 0 failures, abl_logic_test
  48 checks 0 failed, DLL built. Artifact SHA-256 `dc40da5ed68bb1573cd4f43dc7e4427a4804ce8310f546447c3f5c5bf63fb2d0`
  (PE timestamp 0, exports only rfe_1d3b7a91, imports KERNEL32 + msvcrt); installed by owner, installed copy hash
  identical (Claude check, game not running). Supersedes v1 F2E04CE5. Live v2 check pending.

### v2.1 multiplayer guard (2026-10-03, source; owner build pending) - public release blocker

Research: DeepSeek batch E, `f1c_ida_decompiled/investigations/ai_brain_map/phase3/MULTIPLAYER_GUARD.md`, reviewed by
Claude (game gate AIMainUpdate L44 `veh+0x32C8 > 1 -> return` read; S1 writers/readers as cited). Implemented:
- Worker (100 ms) reads session state `(*0x7CD25C)+0x3FC4`; only `0` = offline allows control; unreadable / 1 / 2 / 3
  / 4 = refuse (fail-closed; `g_offline` starts 0). Change is logged (`SESSION GUARD: ...`).
- Every hook stage and both Overtaking-gate cars go through `car_want()`: no offline -> empty lever set -> the normal
  release paths restore the AI's own values; per car also `veh+0x32C8 == 1`.
- Rules engine: enabled only while offline (otherwise every site is restored).
- Header: OFFLINE (56), SESSION (60); panel status says "NOT OFFLINE ... control disabled" in red.
- Limit: a car that stops being processed by the AI (becomes a network car) keeps a held value until session
  re-initialisation or unload (the game re-initialises cars when a session starts).
- Live test (owner, from MULTIPLAYER_GUARD.md section 5): offline race must say "offline, levers allowed"; a LAN host
  / client session must say "NOT offline" and levers must do nothing. Host/client test needs a second peer.

### v2.2 Drive mode (2026-10-03, source; owner build pending)

Owner: "incorporate the AI car control (the other .bat, mechanical_failures ai_brake_gui) with the user able to change
the AI control keys." The Lab now has its own direct control, independent of the mechanical_failures DLL:
- Two more verified call-site wrappers (only callers, capstone 2026-10-03): `0x42C748` call AIMainUpdate 0x424E70
  (post-hook = full control, same timing as the mechanical_failures override) and `0x425418` call auto-shift 0x426330
  (manual gears via the game routine 0x4EF930(veh+0xC, gear), one shift per request, not mid-shift). The callee
  prologues are NOT fingerprinted because mechanical_failures detours them; both DLLs can be loaded together.
- Writes command + actuator (0x4434/0x357C, 0x4438/0x3584, 0x442C/0x3580) for the ticked controls only; guarded by the
  offline / local-AI rule like every lever.
- Panel "Drive this car yourself" card: DRIVE MODE, per-control "I control ...", sliders, gear buttons, invert
  steering, keyboard control (100 Hz global key polling: works while the game has focus), panic key (default End,
  releases everything even in-game), "Change keys..." dialog (click Change, press a key; Esc cancels; Defaults).
  Keys and invert are saved in `tools/ai_brain_lab_settings.json`. Defaults = the original panel: I throttle, K brake,
  J/L steer, U/O gears. Switching cars hands the previous car back.
- Do not drive the same car with both the mechanical_failures override and the Lab at once (both write the same
  fields; last writer wins).

## Pending

- Owner: build + both tests; install; first live session (suggested order: pace, braking confidence, line forced
  +1 m on a straight, overtake commit in a battle, cooperation in a race with lapping).
- Not yet verified anywhere: the whole DLL. Treat every lever as untested until a live record exists.

## 2026-10-03 drawer redesign (handoff action 1)

- `tools/ai_brain_lab_gui.py`: drawer now narrow (420 px, `DRAWER_W`), single column, scrollable, collapsible sections, tabs This car / Rules / Presets / Drive. Help text moved to hover tips. No-race / no-game / no-plugin states in plain words; car picker never lists fake slot rows; rescans every 2 s.
- `app/main.py`: drawer fixed width on the right, diagnostics take the rest (PanedWindow removed).
- Before-state: `trials/before_drawer_2026-10-03/`. Screenshot (no game, no plugin): `trials/drawer_2026-10-03_no_game.png`.
- Verified: py_compile, window renders all 4 tabs offline. NOT verified: live race, plugin loaded, hover tips, Drive keyboard in-game (owner test).
- Known (pre-existing): rule slider resolution (range/400) snaps yield radius 625 to 624 on display.

## 2026-10-03 fixes from the owner's live tab screenshots (race session, pid 32620)

Changed:
- One car for the whole window (`app/main.py` `link_cars`): picking a car in the viewer (menu, < >, Follow, All cars) or in the drawer now moves both. Before, the viewer showed Irvine while the drawer would have changed Schumacher. Logic tested with stub objects (no loop); not yet tested live.
- Driver names: the map labels and the Traffic tile showed every car as "(2001)" because they used `split()[-1]`. They now use `nm()`, which removes only the trailing "(year)" ("Eddie Irvine"). Viewer file: `f1c_ida_decompiled/mods/sdk_sandbox/ai_mind_monitor/ai_brain_gui.py`.
- Stuck tooltips over the alert-chip row: when the pointer left the window, the tooltip never got its Leave event, so it stayed on screen. Both tooltip classes (viewer and drawer) now check every 300 ms that the pointer is still over the widget, and close if not.
- Traffic table: driver column made wider (names were cut off: "Frentzen (200"); the -9999 "no value" marker in `state` now shows as "-". Speed tab pace text now wraps instead of running off the edge.
- Drawer sliders: while a lever is off, its slider follows the AI's live value. Before, Pace sat at 0.9949 while the AI was at 0.8457. Ticking a lever therefore changes nothing until the slider is moved. Native max_speed 1e5 (the game's "no cap" value) shows "AI no cap", and copying it clamps the slider to its top.
- Before-state: `trials/before_viewer_fixes_2026-10-03/`.

Seen, not fixed:
- Registry v1.4 type mismatches (an int read as a float, and floats read as ints), e.g. 0x4450 track_part_from_pit_exit, 0x4590, 0x4528/0x452C, 0x441C. Logged as OPEN finding `ai-brain-registry-v14-int-float-display-mismatch-2026-10-03`. The fix belongs in the registry review.
- One screenshot (All fields tab) showed the drawer drawn twice, offset by about 17 px. This could be a screen-capture tear during a redraw, but that is not confirmed. Only one occurrence; it needs a repeat before anything is changed.
- Waypoints tab: "plan km/h" goes negative far ahead (-3 to -16). These are the game's own projected values; the viewer shows them as-is.
- All cars: Mika Salo at 0 km/h with BRAKE 100% has interest 0. Follow scoring may not count a stopped car. Not checked.
- Next corner shows "? ahead" when the distance is unknown.
Pending owner check: live race with the linked car pickers, names on the map, tooltips gone, sliders tracking the AI.

## 2026-10-03 "fix everything" pass (follow-up to the screenshot review above)

All items from "Seen, not fixed" are now handled:
- Registry types: ABI v1.5, read from the decompile; OPEN finding resolved by `ai-brain-registry-v15-type-fixes-2026-10-03`.
- Doubled drawer: probable cause is constant redrawing. The drawer re-applied every label 5x per second; `setw()` now updates a widget only when its value changes. This is a mitigation, not a proven fix (only one occurrence was seen).
- Negative plan speeds: shown as "stop", with an explanation in the tab header.
- Stopped car: STOPPED alert and +3 Follow interest.
- "? ahead": now "ahead (beyond the 20-waypoint view)".
- Rule sliders: the step is now a power of ten (625 stays 625; was 624).
- Line-strip labels no longer overprint the caption.
- Replay of older recordings fixed. The two v1.0 test recordings are refused with a clear message.
Details: `f1c_ida_decompiled/mods/sdk_sandbox/ai_mind_monitor/README.md` (2026-10-03 section).
Pending owner check (live race): everything in this file's 2026-10-03 sections.

## 2026-10-03 live tour (race running, pid 32812) + writable-field census

Live tour: `%TEMP%\abl_live_tour.py` stepped through all 14 viewer tabs + 4 drawer tabs with a screenshot each, with no
callback errors. Viewer and drawer were on the same car (slot 1). The values matched across tabs and made sense:
registry v1.5 shows `unnamed_4590` = 45 (lap) and `track_part_from_pit_exit` = 4879 m; the All cars positions,
laps and pace look plausible; the drawer's AI values follow the car. Not cross-checked against the f1c-memory MCP
(reader not started: "WaitNamedPipe" error).

Fixed in this pass:
- Names without "(2001)" everywhere: viewer car menu, All cars, drawer car menu (All cars was also cutting names off).
- The map legend now sits on a dark plate, so far-away car labels no longer overprint it.
- No greyed-out buttons left: Speed governor / Brake x / Throttle x now have a working "reset" (to neutral); Crash
  tendency now reads the AI value from 0x44D0 (slider step 0.1).
- My presets: Save / Load, one JSON per preset in `presets/`, with ticks + values per car (share plan step 3,
  first part). Tested against a temp folder.

Writable-field census (registry v1.5, 140 fields). The tool exposes ~25: pace, corner x/+, wet, max speed, governor,
braking confidence, brake ramp, aggression, steer lock, watchdog limit, baseline, push, push norm, crash,
line offset (lateral request / forced / override), overtake policy, cooperation, traffic lift (cap), state cmd,
drive (throttle/brake/steer/gear), and 4 global rules.
Most of the rest are per-tick outputs, positions, timers or pointers (writing them does nothing or is unsafe), or
marked never-write (0x3D6C, 0x4410, 0x4064, 0x45F4, 0x454C).
Behaviour inputs NOT exposed yet (candidates; each needs a DLL lever, so an owner build, and a live check):
- 0x44C8 rain_ability_scaled (wet skill)
- 0x44B8 throttle_cap_race_start (launch)
- 0x4554 corner-target multiplier (1.0 neutral, init-only writer)
- 0x44FC random driver variant
- 0x44BC start stalls / 0x44C0 qualifying ability (outside the registry)
- pit plan: 0x4588 count, 0x458C next tyres, 0x4590 stop laps + 0x413C re-arm (FACT: organic pit on demand)
- 0x3D70 engine life, 0x4170 scheduled stall time
- 0x3A04 gear_max
Decision for Matias: which of these become levers (handoff action 6 / DeepSeek batch D).

## 2026-10-03 v3 "everything modifiable": GUI design (Claude) + DeepSeek handoff

Owner decision: everything modifiable becomes a control. Claude designs the GUI; DeepSeek does census / plugin / tests.
- `dll/levers.json`: the catalog (schema at the top of the file). 13 seed levers, `status: proposed`.
- `tools/abl_shm.py`: the v3 shared-memory contract (lever block after the v2 extension, 33 tables x 128 slots;
  ON / VALUE / ORIG / STATUS). `test_abl_shm.py` still passes.
- `tools/abl_everything.py`: new drawer tab **Everything**, built only from `levers.json`:
  - Find box, "only ticked" filter, counter.
  - One section per family.
  - Confidence dot per row: green = proven, yellow = from the code, orange = guess.
  - Slider / choice / action rows; risky actions ask for confirmation first.
  - A status per row.
  - It shows the real AI value now, with a read-only memory read, before the v3 plugin exists.
  - No control is ever disabled.
  - Presets also save the Everything levers.
- Drawer tabs renamed: Quick | Everything | Rules | Presets | Drive.
- Viewer: starts in a **Basic** view (Overview, Speed, Line & steering, Traffic, All cars); "Show all tabs" adds the
  9 research tabs back.
- Verified live (race, pid 32812): Everything tab shows the real values for Schumacher (next stop lap 45, 4 stops,
  top gear 7, launch 1.135, stall time FLT_MAX shown as "AI none"); no callback errors; Basic view correct.
- Next: DeepSeek runs `DEEPSEEK_HANDOFF_LEVERS_V3_2026-10-03.md` (DS1 census, then STOP for a Claude review).
- Before-state: `trials/before_levers_v3_2026-10-03/`.

## 2026-10-03 v3 DS1 census (DeepSeek) - ready for review (STOP 1)

- `dll/levers.json`: 12 seeds verified and corrected in place, 1 seed rejected (`start_stalls`: its only reader is
  the .rcd parse itself), 16 new verified entries appended. 29 entries: 28 `ready`, 1 `rejected`. Seed order and
  indices unchanged.
- Evidence, one row per registry v1.5 field (kept / skipped and why) and the open questions:
  `trials/levers_census_2026-10-03.md`. Before-state: `trials/before_DS1_2026-10-03/`.
- Checks run: `python tools\test_abl_shm.py` -> `0 failures`; catalog validation (unique ids, `lo < hi`, <= 128,
  labels <= 32 chars, evidence present) passed. No code changed; DS2 starts only after the Claude review.

## 2026-10-03 v3 DS2 plugin engine (DeepSeek) - ready for review (STOP 2)

- `tools/gen_levers.py` (new): validates `dll/levers.json` (schema, families, unique ids, int-choice ranges,
  `lo < hi`, never-write list incl. action targets) and generates `dll/abl_levers.h` (row table, action writes,
  catalog CRC32). `--check` exits 1 and writes nothing on any problem. Rules from the STOP 1 review: entries with
  `quick` and `status: rejected` get no engine row (index stays reserved); two engine entries may not share an
  offset except the intended `pit_now` / `next_stop_lap` pair on 0x4590.
- `dll/abl_levers.h` (generated): 29 catalog entries, 15 engine rows, 14 skipped (13 quick + `start_stalls`
  rejected); `ABL_LEVERS_CRC32 0x57B99FD8`.
- `dll/abl_logic.h`: `ABL_ABI` 3; v3 lever block exactly as `tools/abl_shm.py` (sub-header, 16-byte slot, status
  codes); the lever engine is pure (`abl_lever_step`, `abl_action_step`, `abl_lever_release`) over an injected
  memory accessor, so the offline test runs it on a fake vehicle buffer; `_BYTE` fields use read-modify-write.
- `dll/ai_brain_lab.c`: engine inside the existing AIMainUpdate call-site wrapper - `pre`/`once` levers before the
  call, `post` after, actions before it. Guards: ARM + offline session + `veh+0x32C8 == 1`; out of bounds ->
  STATUS 3, guard fail -> STATUS 5 (both restore and never write); `once` saves the AI's value on the 0->1 edge and
  restores on release; ORIG follows the game while ON = 0; skipped indexes STATUS 4, client indexes this build lacks
  STATUS 7; global-scope table (32) serviced by the 10 Hz worker; RELEASE ALL / unload restore every held field.
  `pit_now` is refused with STATUS 5 while `next_stop_lap` is ON (shared 0x4590).
- `tools/test_abl_shm.py`: every v3 offset/status code checked against the header (small macro resolver) plus
  generated-table count/CRC/row-map checks -> `0 failures`.
- `tools/abl_logic_test.c`: 55 new checks (apply / once / bounds refuse / guard refuse / int+byte / action once /
  action base-relative / release all) -> `95 checks, 0 failed`.
- `dll/build.bat` runs `gen_levers.py` next to `gen_rules.py` before the tests.
- Dev DLL built with the documented gcc line: SHA-256 `37CE1B705BB3BD6D81DB95A537ECC5DD29A42C84173ED9881BCD2828CC52E4A3`
  (NOT installed; the owner rebuilds with `dll\build.bat`).
- Before-state: `trials/before_DS2_2026-10-03/`. STOP 2 pending Claude review; DS3 not started.

## 2026-10-03 v3 DS2b fixes (STOP 2 review) - ready for STOP 2b

Two fixes required by the STOP 2 review; before-state: `trials/before_DS2b_2026-10-03/`.

1. **Pit row follows the cursor.** `dll/levers.json` `pit_now` and `next_stop_lap` now carry
   `"index": {"by": "0x45E0", "stride": 16}`; the live address is `0x4590 + 16 x [veh+0x45E0]`, resolved every
   tick. `gen_levers.py` validates the index block (vehicle-range cursor, stride 1..64) and emits `index_by` /
   `index_stride` on the row plus an indexed action write; `abl_logic.h` resolves it (`abl_lever_addr`,
   `abl_action_addr`, cursor clamped 0..4) and `AblLeverRt.held_addr` restores at the address that was written,
   never at a row the cursor has since moved to. Unit tests cover cursor 0 and cursor 1 (value lever and action)
   plus the clamp.
2. **Byte restore no longer clobbers neighbours.** all restores go through `abl_write_bits(..., type)`, so a byte
   lever writes back only its saved byte; new unit test moves the neighbour bytes while the lever is held and
   checks they survive the release.

- `python tools/gen_levers.py --check` -> 29/15/14 OK; `python tools/test_abl_shm.py` -> `0 failures`;
  `tools/abl_logic_test.exe` -> `114 checks, 0 failed`; DLL compiles clean (`-Wall -Wextra -Werror`).
- Dev DLL SHA-256 `2C91AB6BE60E24DCDD0CD53BEF4CE6F385C6011577A94ECEE477D6610F8C87C6` (supersedes `37CE1B70...`;
  NOT installed).
- Installed 2026-10-03 by owner command (game not running): copied to
  `%USERPROFILE%\Documents\F1 Challenge V10\plugins\lightloader\ai_brain_lab.dll`; installed hash identical
  (`2C91AB6B...`). Live validation pending (DS4); log expectation: `ACTIVE (Lightloader verified)` + 7
  `HOOK installed` lines. If STOP 2b review forces a source change, the owner rebuilds and reinstalls.

## 2026-10-03 DS1 STOP 1 review (Claude)
DS1 census accepted with changes:
- 13 duplicate entries are now linked to the Quick levers (`quick` key), so each offset has one write path. This also removes an unsafe max_speed `once` write that would lift the pit limiter.
- Rejected entries are hidden in the GUI.
- Risky sliders ask before switching on.
Open questions are answered in `DEEPSEEK_HANDOFF_LEVERS_V3_2026-10-03.md` (STOP 1 section). Live check: the Everything tab shows 28 levers with real values (lives, tyres, pit plan); linked rows show "via Quick". DS2 is next.

## 2026-10-03 DS2 STOP 2 review (Claude)
Tests re-run by Claude: gen_levers --check OK, test_abl_shm 0 failures, logic test 95/95. Accepted pending DS2b:
(1) pit stop lap must be written at `0x4590 + 16*cursor(0x45E0)` (AIPushLap L40), not row 0 (it breaks after the first stop);
(2) byte-lever restore must not rewrite the neighbouring bytes. DLL built by DS (SHA 37CE1B70...) is NOT to be installed before DS2b.
- 2026-10-03 STOP 2b accepted (Claude): DLL 2C91AB6B... ready for owner install; GUI reads indexed pit row.

## 2026-10-03 CRASH FIX: DS2b DLL crashed the game at startup
- Symptom: game closes right after loading. Windows Application log 1000 at 14:33:56 and 14:35:55: faulting module
  ai_brain_lab.dll offset 0x26C0, c0000005. Plugin log shows ACTIVE + SESSION GUARD offline just before.
- Cause (objdump of the 2C91AB6B build): the worker's 10 Hz lever-status sweep reads the v3 lever block. The plugin
  mapped the shared memory at ABL_SHM_SIZE (the v2 size), but the v3 block starts AT ABL_SHM_SIZE, so every v3
  access is past the end of the view. The unit tests use a fake buffer, so they could not see it.
- Fix (Claude): CreateFileMapping / MapViewOfFile / memset / header SIZE use ABL_SHM_SIZE_V3 (ai_brain_lab.c init).
- Rebuilt with dll\build.bat: rules 4/4, gen_levers OK, test_abl_shm 0 failures, logic test 114/114.
  New DLL SHA-256 23540431f635193f1a2a0843316dd6033f1f464a7590b420c10e4458fd181665 (supersedes 2C91AB6B, which crashes).
- Before-state: trials/before_shmfix_2026-10-03/. Pending: owner install + start test.
- Follow-up for DS: add a test that fails when the mapping size in ai_brain_lab.c is not ABL_SHM_SIZE_V3.
  (Done 2026-10-03 DS4-prep: shared `dll/abl_boot.h` + real-mapping boot test; see the DS4-prep section.)
- 2026-10-03 fix 2 (live check of 23540431): the game runs and is stable (log ACTIVE + 7 hooks), but the tool read ABI 15 /
  SIZE garbage. The plugin wrote the v3 sub-header (LEVERS_N/OK/CRC) at main-header offsets 0/4/8 instead of
  ABL_LEV_OFF + 0/4/8, clobbering ABI and SIZE, so the client fell back to v1. Fixed (HU(ABL_LEV_OFF + ABL_LH_*)).
  Rebuilt: tests pass. DLL SHA-256 efea7cc3e6ee13069204e4fdb36d0cb33e41de481ba0718561cd9d4b857778ca. Pending owner install.
  DS follow-up test: read the header fields after init on a real mapping (not only offsets).
  (Done 2026-10-03 DS4-prep: `abl_logic_test` runs the exact publication code on a real mapping; see the
  DS4-prep section.)
- 2026-10-03 live check of efea7cc3 (installed, owner race): game stable; header ABI 3 / SIZE 77920 / v3 True;
  lever header 29 / 15; ORIG live from the plugin (e.g. next stop lap 45, top gear 7). The Everything tab shows "idle"
  on every v3 row. GUI change: plugin status 5 is shown as "idle" when unticked and "ARM is off" when ticked unarmed.
  No lever was written. Next: DS4 live checklist (owner does the writes).

## 2026-10-03 DS4-prep: real-mapping boot test closes both live-only bugs (DeepSeek)

Both live bugs (view mapped at `ABL_SHM_SIZE`; v3 sub-header written at main-header 0/4/8) were invisible to the
fake-buffer tests because the tests never ran the mapping/init code. Fixed structurally:

- New `dll/abl_boot.h`: the exact map + publish block `ai_brain_lab.c activate()` runs (`abl_shm_map` maps
  `ABL_SHM_SIZE_V3`; `abl_shm_publish` refuses a smaller view and writes ABI/SIZE/CARS/STRIDE plus the v3
  sub-header at `ABL_LEV_OFF + 0/4/8`; `abl_shm_publish_magic` writes MAGIC last). `ai_brain_lab.c` now calls it.
- `tools/abl_logic_test.c`: new `test_shm_boot_real_mapping` creates a REAL Windows mapping (test-only name, never
  the game's), runs the same publication, and asserts `ABI == 3`, `SIZE == ABL_SHM_SIZE_V3`, MAGIC, and
  `LEVERS_N/OK/CRC` at `ABL_LEV_OFF + 0/4/8`; a second real mapping at the old crash size must be refused with
  no bytes written.
- Evidence (offline, this shell): `python tools/gen_levers.py --check` -> 29/15/14 OK;
  `python tools/test_abl_shm.py` -> 0 failures; `dll/build.bat` -> rules 4/4, logic `126 checks, 0 failed`,
  DLL built clean. Negative controls (mutated copy in %TEMP%): v2-size map -> 7 FAIL; sub-header at 0/4/8 -> 5 FAIL;
  restored baseline -> 0 FAIL.
- Dev DLL SHA-256 `81498134658856e9741b3cb8cd289938e3d68c9293f8e78856e6478b78a55908` (supersedes efea7cc3 as a
  build; NOT installed). DS4 runs on the installed `efea7cc3`; the refactor is offline-verified only.
- Before-state: `trials/before_DS4_2026-10-03/`. Checklist: `trials/LEVERS_LIVE_CHECKLIST_2026-10-03.md`
  (28 ready levers; order top_gear, next_tyres, pit_now, then the rest).
- Pending: owner live run of the checklist (writes happen in the panel only); passing rows become
  `status: live-checked` at the STOP 3 review.

## 2026-10-03 NO_LIFT fix: De La Rosa stuck at 75 % throttle (vanilla latch bug)
- Live (owner race): De La Rosa slot 20, NO_LIFT + rule cap 1.0 both APPLIED, no traffic target, yet throttle 0.75 at
  238 km/h. Cap 0x457C = 0.75.
- Cause (code): the cap is set to difficulty x 0.75 only while a traffic target exists (AISteeringCorrection L22-23) and
  is never restored; the clamp (L131-133) runs every tick. Finding `ai-throttle-cap-457c-latches-after-traffic-target-2026-10-03`.
- Fix: with NO_LIFT on, the plugin rewrites 0x457C = AIDifficultyScalar (pure from dword 0x7D01A0) in pre_correction.
  Unit tests +4 (130/130). DLL SHA-256 8ec0281b6b9c2511d6cad538319f4a85f44deb8f51b447eb396d63181973a246. Pending owner
  install + live check (De La Rosa throttle back to 100 % on straights).
- CORRECTION (same day): the De La Rosa "5 samples at 238 km/h" were one frame of a PAUSED game (sim 675.344 s and
  TICK 442156 frozen; owner confirmed the pause). The live part shows only a latched cap 0x457C = 0.75 with no target.
  The "stays capped while driving" part is code evidence only. Live check with the game running is still pending.
  Lesson for live reads: check that sim time / TICK advance before calling samples evidence.

## 2026-10-03 global switch "Never cap throttle at 75 % after yielding (all cars)" (owner request)
- Rules tab, top section, yellow checkbox. Client sets bit ABL_GF_NO_LIFT_ALL in the v3 sub-header GLOBAL_FLAGS
  (ABL_LEV_OFF + 12, client-written; abl_shm.py `set_global_flag`). The plugin, in pre_correction for every AI car,
  writes 0x457C = AIDifficultyScalar while the bit is set AND ARM on AND offline AND veh+0x32C8 == 1.
- Cars still get traffic targets (they still yield / side-step; blue flags work); only the latched 0.75 cut is gone.
  The per-car NO_LIFT lever additionally clears the target (no yield at all).
- RELEASE ALL clears the bit. The cap is not restored to 0.75 on untick (restoring a game bug is pointless; the game
  re-latches it on the next target).
- Build: test_abl_shm 0 failures (checks the GLOBAL_FLAGS offset vs header), logic test 130/130.
  DLL SHA-256 99823b1fdb737e4a53d4ac5f4f0ced5b7b44f553a6dfe243cbf49c49d780d026 (supersedes 8ec0281b, includes it).
  GUI checkbox verified offline (exists; RELEASE ALL clears it). Screen capture returned the wallpaper (screen locked?),
  so no screenshot. Pending: owner install + live check with the game RUNNING (unpaused).
- 2026-10-03 LIVE CHECK (game running: sim 155.2 -> 165.3 s, 50 sweeps of all AI cars over 10 s, ARM 1, offline,
  GLOBAL_FLAGS 1, DLL 99823b1f): no car had cap 0x457C < 1.0; no sample with throttle exactly 0.75. No car had a
  traffic target in this window, so a live yield was not observed. Note from the code: while a target is active, the
  game's own AISteeringCorrection L23 sets cap = difficulty x 0.75 and clamps in the same call, so the cut still
  applies DURING a yield. The switch removes the latch after it. To remove it during the yield too, also set the rule
  "Traffic throttle cap" = 1.0 (Rules tab).

## 2026-10-03 live read: Irvine vs Alonso (owner question "why not trying, only blips the brake")
- Game running: sim 275.2 -> 300.1 s, 248 samples at about 10 Hz (raw: %TEMP%\irv_alo.json). Irvine had no lab flags.
- Seen: record mode 16 (approach) at 8-10 m -> mode 1 (attack) at 277 s, published lateral +3 m (out of the tow),
  6-9 km/h faster, gap 11 -> 2.5 m, PASSED at 286.3 s (P18 -> P17). Brake 1.0 at 289 s = corner (target 94 km/h);
  Alonso braked the same. No interaction target, no lateral request, cap 1.0. The owner's "brake blip without
  attempting" case was not reproduced in this window.
- Aggressive-overtake recipe (existing levers): overtake COMMIT, aggression 1.6-2.0, braking confidence 1.10-1.15,
  pace +0.02-0.04, global no-75 % cap + NO_LIFT, cooperation never-move-aside.
- Gap: no lever forces an EARLIER approach->attack switch (threshold via the opponent record 0x42DC). Candidate for
  a DeepSeek hook spec.
- 2026-10-03 team orders: native teammate-yield branch found in Overtaking (finding ai-native-team-order-yield-overtaking-2026-10-03); per-car block = COOP never moves aside.

## 2026-10-03 off-track rejoin throttle (owner: Coulthard capped until back on track)
Live-confirmed vanilla rule (finding ai-offtrack-rejoin-throttle-05-04-2026-10-03): off the corridor + 1.5 m, the AI's throttle target is 0.5 (> 90 km/h) / 0.4 (<= 90 km/h).
New global rules (Rules tab, family "Off-track rejoin"): offtrack_throttle_fast (0x42608A, [0x59E450] 0.5) and offtrack_throttle_slow (0x426092, [0x59E8C8] 0.4). gen_rules verified 6/6 sites (it caught my swapped sites first). Tests pass.
DLL SHA-256 70d8c2601aadd6f1c0f7858f4b593d7c33692bcd07dcdf24a6e4faca83062d51 (includes NO_LIFT_ALL). Pending: owner install + live check.
- 2026-10-03: GUI layout fixes (glance autofit, canvases follow width, ATTACKING badge on the Traffic tile, drawer banner 'Game paused'); finding ai-no-defend-mode-2026-10-03.

## 2026-10-03 traction control + steer lift controls (owner request)
Rules tab, new families "Traction control" (6) and "Steer lift" (3), all `abs` operand redirects inside
AISteeringCorrection 0x426CF0 (decompile L24-66, exe 0x426D14-0x426E92):
tc_grip_threshold 0.9 @0x426DB3, tc_cut_steepness 1.1111 @0x426DEF, tc_min_speed 4.0 @0x426DCA,
tc_weight_worse 0.7 @0x426D7B+0x426D93, tc_weight_better 0.3 @0x426D83+0x426D8B, tc_brake_gain 4.0 @0x426E58,
steerlift_min_steer 0.1 @0x426D20, steerlift_min_speed 20.0 @0x426D33, steerlift_amount 0.7 @0x426D44.
Shared constants are untouched (per-instruction redirect). One-click buttons in "All AI cars": Traction control OFF,
Steer lift OFF, Full-throttle rejoin. Build: gen_rules 15 rules / 17 sites verified, tests pass.
DLL SHA-256 e002e2703f151271cfb52b0466ea28979b2370e825ab15bddb8f12adde6e2595. Pending: owner install + live check.
Not covered: the smoothing factor 0.5 @0x426DA9 (a weight, not a tunable; changing it breaks the average) and the
"safe throttle" target (physics helper 0x4E8780, meaning not identified).

## 2026-10-03 "Following & overtake decision" rules (owner: get close before changing lane)
8 rules / 11 sites (follow_min_gap 4 m, follow_backoff 5 m/s, attack_range 20 m x (aggr+1), attack_patience_decay 5,
attack_threshold 500, attack_patience_cap 450, alongside_gap 5.5 m, approach_patience_keep 0.85), plus button
"Tailgate before passing" (1.5 m / 1 m/s / 900). Finding ai-follow-and-lane-change-decision-constants-2026-10-03.
Build: 23 rules / 28 sites verified, tests pass. DLL SHA-256 c360220435005fd88db9378b2ec0bea38e0e4e93e25f3d552aa0f170837ddee1
(includes traction control + steer lift). Pending: owner install + live check.

## 2026-10-03 GUI v4 redesign (owner: many variable groups + apply to X / X and Y / ALL)
- `tools/abl_scope.py` (new): "Apply to: this car / picked [Pick...] / all AI" bar with a plain summary; a Fanout
  client sends every per-car write (Quick, Everything, Presets, Car state, Release) to the Apply-to cars; Drive mode
  never fans out; PARK/STOP to several cars asks first. New first tab **Active**: everything switched on, grouped per
  driver + "All AI cars (rules)", each with an undo x.
- Section titles show "· N on" in yellow; Rules families start folded and have hover help (dll/rules.json
  `family_help`, incl. the following / lane-change explanation). Rule quick buttons in a 2-column grid.
- Rules stay global until plugin v4 (per-rule car mask contract in abl_shm.py; DS5 in the DeepSeek handoff). The
  Rules tab says which applies. The car set of a rule is fixed when it is ticked.
- Verified offline: compile, test_abl_shm 0 failures, fan-out test (picked 5,7 -> PACE to 5 and 7; DRIVE/DRV_* stay
  on 3), badge count, screenshots of Active / Quick / Rules. Not yet checked live.
- Before-state: trials/before_gui_v4_2026-10-03/.

## 2026-10-03 Driver AI Toolkit 1.0.0 release (public product name)
- Package: `releases/driver_ai_toolkit/1.0.0-r2/` (zip ABBAD0AA..., 573 405 bytes; `1.0.0/` = first attempt, superseded,
  unpublished). Built by `tools/build_toolkit.py`; manual `MANUAL.md` by `tools/gen_manual.py` (value tables generated
  from the GUI's own sources). `DriverAIToolkit.bat` + `app/toolkit_setup.py` (Lightloader >= 2.2.0 gate via
  LightloaderManager.ini, plug-in install with .bak, installed.json). Viewer reads its data from the package
  (`F1C_CORPUS_ROOT`, brain_core.py change).
- Lightloader 2.2.0 Tools page + deploy: `development/lightloader/v2.0.0-hub-dev.1/release_2.2.0_2026-10-03/MANIFEST.md`.
- Plug-in 31632e1c... (= c3602204... + new levers.json CRC; levers.json help texts for next_tyres and launch cleaned).
- Pending: owner in-game test of the package, owner deploy, LAN test.
- 2026-10-03 r3: owner test found the gate read only LightloaderManager.ini (stale after a loader update: said 2.1.0 with loader 2.2.0). Gate now = max(loader dinput8.dll UTF-16 'Lightloader vX.Y.Z', INI). Zip 200B5E6F...
- 2026-10-03 r4: owner saw a CMD window popping up: brain_core.Game() ran tasklist without CREATE_NO_WINDOW under pythonw (every 2 s while waiting for the game). Fixed (creationflags 0x08000000, also in toolkit_setup); game EXE now matched by name prefix 'f1 challenge' / 'f1c' (renamed EXEs). Zip C9CCD01C...
- 2026-10-03 r5: owner: 'Lightloader is meant to do the version validation ... should just be a simple install and run'. Removed the toolkit's own Lightloader version gate and installed.json logic; setup = copy plug-in if missing/different (.bak), else nothing; open anyway if the copy is blocked. Zip AE9011F1...
- 2026-10-03 SHIPPED: Lightloader 2.2.0 + Driver AI Toolkit 1.0.0 (r5) deployed, verified externally, promoted to production/. See production/RELEASE_REGISTRY.md.
- 2026-10-03 1.0.1 SHIPPED (zip 19F449C3..., catalog 1C9F2404...): resolution fix, build fingerprint gate (bug report: scrambled names on another build), no console flash. 75 % changelog line removed (owner). The in-app 75 % switch + its tooltip/manual text are unchanged pending owner decision.
- 2026-10-03: update check 400 for toolkit users fixed server-side (telemetry v4, operations/vps/telemetry_v4_2026-10-03/RECORD.md).

## 2026-10-08 F1.exe compatibility investigation

Viewer process-name filter rejects `F1.exe`; DLL/levers compatible. See [F1EXE_COMPAT_INVESTIGATION_2026-10-08.md](F1EXE_COMPAT_INVESTIGATION_2026-10-08.md).
