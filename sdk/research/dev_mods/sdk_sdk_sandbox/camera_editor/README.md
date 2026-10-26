# Live Camera Position Editor — F1 Challenge 99-02

> **2026-10-04 code review:** [REVIEW_2026-10-04.md](REVIEW_2026-10-04.md). It found 3 high-severity
> issues (scope layers overwrite each other, out-of-range stock values clamped on edit, runtime
> thread race). Fix these before any wider release.

> **Beta release:** use the Python editor / packaged Python EXE. The native
> C# prototype is experimental and is not a replacement. See
> [RELEASE_READINESS.md](RELEASE_READINESS.md) for installation, safe file
> behavior, known limitations, and final-release test requirements.

External GUI tool (Python/tkinter, no game hook). Reads the game's camera
config, edits every camera parameter, pushes changes LIVE into the running
game's camera view objects, and persists custom positions in a separate file —
**Default.cfg is never modified**.

## Quick start

```powershell
# from the repo root
python tools/f1c_mod.py camera-editor

# or directly (from mods/sdk_sandbox/camera_editor)
python cam_editor_gui.py
```

1. Start the game and enter a session (the camera views are built at session
   start — in the main menu there is nothing to edit yet).
2. Press **Connect**. Status shows the resolved camera manager.
3. Drag any slider — the camera moves live in the game (PositionOffset /
   OrientationOffset). Pull **Apply all to game** to push every override.
4. Values auto-save to `Save\Default.camera_editor.cfg`. Default.cfg stays
   untouched. **Export merged cfg...** writes `Save\Default.merged.cfg`
   (full file with overrides folded in) if you ever want a merged file.

When launched without `--cfg` or `--game-root`, the editor first detects the
currently running F1 Challenge executable and uses that installation's
`Save\Default.cfg`. This keeps it aligned with the game copy running
Lightloader. If the game is elevated, launch the editor as Administrator too;
otherwise Windows prevents both live editing and automatic game-root detection.

## Season and vehicle profiles

Use the **Profile** picker at the top of the editor:

- **Global** applies to every car unless a more specific profile overrides it.
- **Season YYYY** creates or edits a profile for that season.
- **Current vehicle** detects the player car in a live session and saves a
  profile for that car model next to its `.veh` file automatically.
- **Current driver** saves a profile for the exact loaded `.veh` file, so it
  can differ from a teammate in the same car.

Normal editing and saving is unchanged. The editor creates profile folders as
needed; never copy files by hand. `Save\Default.cfg` is the read-only virgin
baseplate for every scope and is never changed. The optional `camera_profiles.dll` runtime
loads them automatically in this order:

```text
Default.cfg → Global → Season → Current vehicle → Current driver
```

Install that runtime DLL once for automatic loading when entering sessions.

### Vehicle camera geometry

With **Current vehicle** selected in a live session, the editor also exposes
the active `.veh` file's **Eyepoint** and **CameraTarget** values. Press
**Save vehicle values** to write them to that exact vehicle file. The first
save creates a sibling `.camera_editor.bak` backup; **Restore backup** returns
the two values to that original state. The game reads these vehicle values when
it builds a session, so leave and re-enter the session after saving.

## What is editable

Every parameter the game's `LocalCam` blocks support:

| Param | Live effect |
|---|---|
| PositionOffset (X left/right, Y up/down, Z back/forward) | **live** — the per-frame camera apply consumes it |
| OrientationOffset (X pitch, Y yaw, Z roll) | **live** (swingman clamps X to [-2.95, -0.05] itself — the game re-clamps) |
| Fov, ClipPlanes, Radius, RadiusLimits, OrientationRate, LODMultiplier, Size, Center, MipmapLODBias, Clear, Color, Flags1/2 | written to the view object; some need a view switch or live validation (see below) |

## Where custom positions live

- Overrides: `Save\Default.camera_editor.cfg` (only edited cameras, full
  blocks, `//` header). The game does not read this file.
- Merged export: `Save\Default.merged.cfg` (explicit button, never the
  original name).
- Overrides are applied to the game from this store — live on slider drag,
  or via **Apply all to game** after connecting.

## How the live memory path works (evidence)

Camera manager / director = the static object at **0x005EA510**
(`Game_Init` → `CamMan_Init(&dword_5EA510)`; per-frame
`GamePostMainLoop` → `MainCameraDirectorUpdate(&dword_5EA510)`).
Camera views are 200-byte objects, built once at session start by
`CameraViewSystem` (0x00408E20) which parses `default.cfg` and writes the
results into the manager entries:

| Slot | Manager offset |
|---|---|
| NOSECAM | +0x10 |
| COCKPIT | +0x14 |
| TVCOCKPIT | +0x18 |
| SWINGMAN | +0x1C |
| generic `LocalCam` array (file order) | +0x20 (u32 count at +0x38, 200-byte stride) |
| current mode / generic index | +0x34 / +0x3C |

View-object fields (parse writes in 0x004562F0; per-frame consumer
`CameraModeViewportUpdate` 0x00409A00 reads/clamps them):

| Field | View offset |
|---|---|
| Fov (mirrored both pairs) | +0x30/0x34 (+0x38/0x3C) |
| ClipPlanes | +0x40/0x44 |
| LODMultiplier | +0x48 |
| Size / Center | +0x4C/0x50 / +0x54/0x58 |
| MipmapLODBias | +0x5C |
| Clear (byte) | +0x60 |
| Color (int32×3) | +0x64/0x68/0x6C |
| Flags1 / Flags2 (int32) | +0x74 / +0x78 |
| RadiusLimits | +0x80/0x84 |
| OrientationRate | +0x88/0x8C/0x90 |
| **PositionOffset** | **+0x94/0x98/0x9C** |
| **OrientationOffset** | **+0xA0/0xA4/0xA8** |
| Radius | +0xAC |

Verified live 2026-08-14 (read-only): game PID running, manager resolves at
0x005EA510, per-frame director path confirmed. The view objects themselves
are only allocated at session start — the running menu instance showed
NULL/stale entries, so the final in-session value check is the pending live
validation (see below).

## Tests

```powershell
python -m pytest mods/sdk_sandbox/camera_editor/camcfg_test.py
```

Covers: parser round-trip (incl. the game's `\\` and `//` comment styles,
negative values, multi-decimal floats), fixed-slot classification, memory
layout math (slot resolution, Fov double-write mirroring, read/write
round-trip, current-camera resolution) against an in-memory fake process.

## Open items

1. **In-session live validation** (the one thing left): start a guarded
   session, connect, check "Pull values from game" for a LocalCam matches
   Default.cfg, then drag PositionOffset and confirm the camera moves. If
   some fields don't take effect live (Fov/ClipPlanes especially — the
   freecam notes the render FOV path is separate from the entry), those are
   applied on next session via the override file instead.
2. FOV render path: `CameraProjectionUpdate` 0x541B70 reads a camera object
   not yet mapped to these entries — Fov live effect unproven.
3. The generic-array count in memory may be < the file's generic camera
   count if the game caps LocalCam parsing (32 max per CameraViewSystem);
   the editor currently requires file order == array order.
4. Multiple monitors / DPI scaling for the GUI not tuned.
