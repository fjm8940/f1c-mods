**Camera Editor 1.0.6 shipped 2026-10-11** ([release record](../../../operations/release_management/camera_editor/RELEASE_1_0_6.md)). Origin - **2026-10-11 fisheye depth reference hardening:** `plugin/fisheye.h` now keeps its own reference to the game depth surface (`fe_hold_depth`) instead of an unreferenced pointer, released before device Reset. Candidate mitigation for the intermittent RETIRE TO PITS crash in d3d8.dll (cause unproven). build.bat PASS incl. FISHEYE TEST; dev DLL SHA-256 e3be56ff540b23c4103ade922d270dd046dac40b43a68e7a010ebc39a68425dc. Pending: live fisheye run, fullscreen alt-tab (Reset), retire test. Before-state: `workspace_organization_audit/ESC_RETIRE_CRASH_BEFORE_2026-10-11/`. [Crash report](../esc_pause_menu/verification/retire-crash-d3d8-2026-10-11/REPORT.md).

Camera Editor (development)
Last updated 2026-10-05
Author Matias

# Camera Editor — Lightloader tool + game plug-in

**Status 2026-10-05: 1.0.4 PUBLISHED 14:00 UTC ([release record](../../../operations/release_management/camera_editor/RELEASE_1_0_4.md)): per-camera Fisheye (experimental, plug-in 1.0.4, `plugin/fisheye.h`; research in `research/fisheye_probe/`). 1.0.3 (Rigid mount, presets, reference image) before it.**
**2026-10-05 later: fisheye black-screen fix, 1.0.5 PUBLISHED 2026-10-06 ([release record](../../../operations/release_management/camera_editor/RELEASE_1_0_5.md)) — [research/FISHEYE_BLACK_SCREEN_2026-10-05.md](research/FISHEYE_BLACK_SCREEN_2026-10-05.md).**
**Next:** owner in-game check of the final fisheye rules (HUD when tabbed out, garage) - shipped before that check.

Successor of the 2026-08-14/16 sandbox editor
(`f1c_ida_decompiled/mods/sdk_sandbox/camera_editor` + `camera_profiles`, untouched; copies of the originals in
`origin_20260816/`). Code review that started this work: [origin_20260816/REVIEW_2026-10-04.md](origin_20260816/REVIEW_2026-10-04.md).

Owner request (2026-10-04): "do all of this [the review fixes]. look into how the driver ai toolkit was published,
do the same treatment into this. redesign the GUI to be simple and fix any bugs. ensure the mod works with
ANY version of the game."

## Layout

| Path | What |
|---|---|
| `plugin/camera_editor.c` | Game plug-in (`plugins\lightloader\camera_editor.dll`): Lightloader v2 protected entry, signature resolution, profile layers, ONBOARD visibility + mirrors, shared memory |
| `plugin/cam_sig.h` | Code signatures: every routine and global found from instruction bytes (no fixed address) |
| `plugin/fisheye.h` | Fisheye: Direct3D 8 device wrappers, world picture, bent grid, HUD detection |
| `plugin/cam_shm.h` | Shared memory `Local\F1C_CAMERA_EDITOR_V1` (plug-in ↔ app ABI) |
| `plugin/camera_editor.meta.json` + `.rc` | `F1C_MOD_META` resource: Lightloader manager shows "Camera Editor (game plug-in)" |
| `plugin/build.bat` | signature test → runtime/refusal test → DLL (32-bit MinGW, reproducible PE) |
| `app/main.py` | GUI (redesigned) |
| `app/layers.py` | Profile layers, the same rules as the plug-in |
| `app/game.py` | Game link through the plug-in's shared memory (PID + resolved addresses) |
| `app/camcfg.py` | LocalCam parser (+ `RigidMount`, plug-in only key) |
| `app/overlay.py` | Reference image over the game window |
| `app/tool_setup.py`, `CameraEditor.bat` | Tool launcher: Python via winget, copies the plug-in, elevates, starts the app from `%TEMP%` |
| `tools/build_tool.py` | Builds `releases/<v>/camera_editor-<v>.zip` |
| `tests/` | `sig_test.c` (+`run_sig_test.py`), `runtime_test.c`, `test_app.py`, `fixtures/Default.cfg` (owner V10 copy, SHA-256 `9f07e830…`) |

## Any game version (owner requirement)

No address, EXE name or file hash is used as a gate. `cam_sig.h` finds 3 routines (camera apply, camera switch,
mirror setup) and 7 globals (director, camera-target / player vehicle pointers, active pointers, vehicle array +
stride, roster) from instruction bytes with the absolute operands wildcarded, reading each global out of the
instruction that uses it. The camera-target and player setters are twin functions; they are told apart through the
player-pointer anchor. A different vehicle stride means a different code base and is refused.

Evidence (2026-10-04, `tests/sig_test.exe`, offline): all 11 values identical on F1 Challenge Online canonical
`1A70E5ED`, V10 clean `DE151845`, 99-02 online `C11919E0`, research 08-08 `19A338BD`, modified 99-02 `3D9592EE`,
CMT 2026 `B3A7390A` (+ a second 99-02 online modified copy); a moved-code control (same bytes at +0x7000) moves the
routines and keeps the globals. The retail disc EXE `3B4E0FBA` has encrypted code on disk: detected, and the
plug-in retries in memory for up to 3 minutes while it unpacks (not run in a game yet). The `.text` of every
readable build differs by 0–3114 patch bytes, none inside the camera, config-parser, vehicle-pointer or .veh-parser
code. The app needs no addresses at all: it reads what the plug-in resolved, and finds the game by the PID the
plug-in publishes (any EXE name).

## Review fixes (REVIEW_2026-10-04 numbering)

| # | Fix |
|---|---|
| 1 | Profiles store only the changed keys (`format=2`); the runtime merges key by key. Old whole-block files keep working: a key equal to Default.cfg counts as "not set". |
| 2 | No clamping anywhere: sliders are a window around the value, typing/±/wheel are free; finite-number check only. |
| 3 | The plug-in reads profiles and writes camera state only on the game thread inside its detours; plain C, no STL; the worker thread only retries resolution. |
| 4 | No hidden ONBOARD003-only default: with no rules every view stays on (vanilla). The ONBOARD dialog offers "Only ONBOARD003 (has mirrors)" as an explicit preset. One view always stays on. |
| 5 | Unknown camera names in a profile are ignored (logged; the app reports them) instead of breaking all mapping. |
| 6 | The app shows each value's source layer, warns when a later layer hides an edit ("click to edit that scope"), and writes the effective value live; the plug-in reloads after every save so both agree. |
| 7 | Driver scope = `<driver>.camera_editor.cfg` beside the exact `.veh` (unique per season/folder). Old `CameraProfiles\Drivers\<name>` files still load. Import accepts driver exports. |
| 8 | One season rule in both: first `19xx/20xx` with no digit beside it; live, the app uses the plug-in's season for the viewed car. |
| 9 | Game found through the plug-in's PID (any EXE name). |
| 10 | Lightloader host proof (protected entry), inert DllMain, no `inject.exe`. Refuses if the camera routine is already detoured (e.g. freecam). |
| 11 | Pending edits are saved to the car they were made for before the viewed car changes. |
| 12 | Suspend ("Pause my cameras") stops profiles **and** ONBOARD redirection; the plug-in itself restores Default.cfg values. |
| 13 | `.veh` Eyepoint/CameraTarget edits replace only those lines; every other byte (CRLF, non-ASCII) is preserved (latin-1 round trip); first save keeps `.camera_editor.bak`. |
| 14 | Dead clone/remove code dropped; layouts documented here. |

## GUI (simple redesign)

One coloured status bar that always says what to do next (grey not running / amber drive onto the track / green
live / blue paused / red fix this); one "Save my changes for" choice (All cars / Season / This car / This driver),
colour-coded and explained in one line; the camera you look at in game is selected automatically (can be turned
off); Position / Angle / Zoom first with plain labels ("Look down / up"), − / + buttons, slider window, typing and
mouse wheel; everything else under "More settings"; autosave with a "Saved ✓" indicator; Ctrl+Z undo (200 steps,
also for resets); "Changed for …" summary per camera; secondary features (ONBOARD on/off, car camera points,
export/import, reset all) under one "More" menu; no pop-ups except the one destructive action.

## Who gets the changes (owner request 2026-10-04: "All cars, specific season, selected car and/or multiple cars")

Top row: **All cars** · **Season** + year box (any season found in SeasonData or CameraProfiles, not only the
current car's) · **Cars** + "Choose cars…" (picker with season filter, search, tick one or many; "Just the car I'm in"
follows the car you drive) · **This driver**. With several cars, a change writes the same value into every chosen
car's `camera_editor.profile.cfg` (each file tagged with its own car) and one Ctrl+Z undoes it for all of them. The
editor shows the first chosen car (the one you are in, if chosen). Live writes happen only when a changed file is
used by the car in the game; otherwise the change applies when that car/season is driven. The plug-in is unchanged
(it already loads season and per-car layers for any car).

GUI colours = Driver AI Toolkit theme (owner request; colours only).

## Verification record

| Date | Check | Result |
|---|---|---|
| 2026-10-04 | `plugin/build.bat`: signatures on 7 EXEs | 6 PASS + moved-code control, retail disc SKIP (encrypted on disk) |
| 2026-10-04 | `runtime_test.exe fixtures/Default.cfg` (real plug-in code, fake game memory, real profile files) | 23/23 incl. refusal: no host, forged host, unknown code |
| 2026-10-04 | DLL | `camera_editor.dll` SHA-256 `428e413bd36e2f9b78eb24b6a84ef4af565d4c73601e0ad1f74b51e9755fa0eb`; i386; PE timestamp 0; exports only `rfe_1d3b7a91`; imports KERNEL32 + msvcrt; `F1C_MOD_META` read by the 2.2.1 manager |
| 2026-10-04 | `python tests/test_app.py` | 41/41 (shm layout = C header, layers, no clamping, format=2 save, undo, scopes, reset, ONBOARD rules, byte-exact .veh, any season, several cars + one-step undo, car picker, live-write routing) |
| 2026-10-04 | Package `releases/1.0.0/camera_editor-1.0.0.zip` | SHA-256 `679EA0BF4E08B2B9898533B6896D76D2615A61C8C1A8C18A6188451D12AAF6F8` (rebuilt after the colour change and the car/season targets); `CameraEditor.bat --install` copies the plug-in; real 2.2.1 manager `ToolsCore.InstallBytes` installs it, version 1.0.0 |
| 2026-10-04 | Catalog `releases/1.0.0/deploy/public/catalog.json` `32674EEB…` | VALID with managers 2.0.2, 2.1.1, 2.2.0, 2.2.1; mods and toolkit entries unchanged from live `2CE019D4…` |
| 2026-10-04 | Defender | no threats (package folder and public/) |
| 2026-10-04 | Owner game (canonical Online `1A70E5ED`), plug-in `428e413b` installed by `tool_setup.py`, log `plugins\lightloader\camera_editor_log.txt` | `Lightloader verified`; in-memory resolution apply 00409A00, switch 00409440, mirrors 00409890, director@005EA528, target@007C54E0, player@007C54E8, roster@008AC390; `STATE 2: active`; profiles loaded for 2002_FW24 (season 2002, layer Global); repeated reloads while the owner edited (app saves) |
| 2026-10-04 | GUI colours | owner: "same color theme" as the Driver AI Toolkit (BG #14161b, panels #1c2027, text #e8e8e8, buttons #2a303a); colours only, layout unchanged; app tests 31/31 |
| 2026-10-04 | 1.0.2 per-number reset: `python tests/test_app.py` + `python -m pyflakes app/*.py` | **56/56** (11 new: ↺ only on changed numbers, single-number reset keeps the rest, key removed when all match, Ctrl+Z restores one number, two cars one click / one undo, group link wording); pyflakes clean; row layout checked in a rendered screenshot; plug-in unchanged (`E1BCA919…`) |
| 2026-10-04 | Package `releases/1.0.2/camera_editor-1.0.2.zip` | SHA-256 `7D532F1DD58959833BAEEC565727A2AA30CA1F62AC076A2A51EF10423A80D355` (173 624 bytes); catalog VALID with managers 2.0.2/2.1.1/2.2.0/2.2.1; real 2.2.1 `ToolsCore.InstallBytes` → 1.0.2; Defender no threats; deployed (`FILES_OK`, `LINKS_OK`, `LINK_OK` ×2, `CATALOG_OK 6A25EBAB…`); API 200 + 1.0.1→1.0.2 upgrade double check passed |

**Not verified yet:** owner's verdict on the live editing session (camera moves, mirrors, ONBOARD redirection, pause),
the per-number reset (1.0.2) in a live game session, the retail disc EXE in memory, the GUI with a live game, Python
install via winget on a clean PC.

## Build / release

```
development\mods\camera_editor\plugin\build.bat
python development\mods\camera_editor\tests\test_app.py
python development\mods\camera_editor\tools\build_tool.py <version>
```
Changelog: `operations/release_management/changelogs/camera_editor.json`. Deploy script and catalog:
`releases/<version>/deploy/`.
