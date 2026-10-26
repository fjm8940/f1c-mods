# Grid Restore 0.1.0

Development candidate. [Initial build and verification record](trials/2026-09-27-initial/RESULT.md). Native in-game validation pending; not promoted for distribution.

Fixes the race starting grid when the game has changed it. Uses the qualifying order saved by [Qualifying Preservation](../qualifying_preservation/README.md) (`qualifying_*.tsv`) and lets the player edit it.

## Player use

Put these beside `qualifying_preservation.dll` (e.g. `plugins\lightloader\`):

- `grid_restore.dll`
- `GridEditor.ps1` and `Grid Editor.bat`
- optional `grid_restore.ini` (`[Mod] Enabled=0` disables the DLL)

1. After qualifying, double-click `Grid Editor.bat`. It loads the newest saved qualifying. Select a driver, then Move up/down (Ctrl+Up/Down), Move to, or Sort by qualifying time. Click **Save grid order**, which writes `grid_order.tsv`.
2. In the race session, before the start (grid/pre-race, race phase below 3, no lap started), press **Ctrl+Shift+G** with the game window focused.
3. The DLL writes the order and asks the game for its own mini-restart, which re-places the cars. `grid_restore_log.txt` lists every car's new and previous grid slot.

Skip step 1 to restore the saved qualifying order unchanged.

## Rules

- File choice: `grid_order.tsv` is used only if its `# source` line names the newest `qualifying_*.tsv`, or if no qualifying file exists. An edit from an earlier weekend is ignored and logged; the newest saved qualifying is applied instead. A broken edit made for the newest qualifying is refused, not silently replaced.
- The file and the race must hold exactly the same unique driver names (the archive's 64-byte name, tab/CR/LF turned into spaces). Positions must be 1..N. Anything else is refused before any write.
- Refused unless: race session (7), race phase 0..2, no restart already pending, every car lap 0 and finish state 0.
- Writes per car: `veh+0x4058` plannedPositionAtStart and `veh+0x4060` position = new slot; `veh+0x405C` qualifying time = the file's time for that driver (untimed stays `-1`). Then sets restart request byte `0x7CE25D`.
- Standalone like Qualifying Preservation 0.3.1: no EXE hash/filename, loader or broker checks; static MinGW runtime (imports KERNEL32, msvcrt, USER32). Pinned until game exit. The hotkey is read by a polling worker thread, so the writes happen on that thread, as in Red Flag's `apply_qualpos`.

## Native mechanism (static evidence)

- `RestartGate_004a0fd0`: when `byte_7CE25D` is set in a race session, runs `FieldCopy_c330` (0x4058 -> 0x4060) for every vehicle, then returns 4 (mini-restart).
- The mini-restart places cars via `SceneRenderUpdate -> sub_4D10E0 -> sub_403CD0(gridTable, veh+0x4058 - 1)`. That has no bounds check, hence the strict contiguous 1..N rule.
- The game raises the same request when a session loads (`SessionSynchronizer_0045f950 -> GlobalFlagSetter_004a0da0`).
- The qualifying -> race transition `FieldCopy_4bc320` copies 0x4060 -> 0x4058 and 0x409C -> 0x405C. That is where FINISH/NEXT simulation damage becomes the grid.
- Prior art: gr17 (CONFIRMED live) and Red Flag 1.1.3 re-grid by writing 0x4058 then this flag, but only during racing (phase 3/5).

## Build and verify

From the SDK repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/grid_restore
powershell -NoProfile -ExecutionPolicy Bypass -File mods/sdk_sandbox/grid_restore/tests/editor_test.ps1
```

The build runs `policy_test.cpp` (real 24-car archive and real editor output fixtures) and `apply_test.cpp` (the actual `apply_grid` against synthetic vehicle storage) before linking the DLL. `GridEditor.ps1 -SmokeCloseAfterMs 1500 -Folder DIR` opens the GUI, reports, moves one row and closes. It is for tests only.

## Pending native validation

Before the start, with the grid scrambled: Ctrl+Shift+G restores the saved order, an edited order applies, and repeated presses work. Check which race phase/game state the pre-race screens report, confirm there is no side effect of the mini-restart at phases 0..2 (fuel/setup/tyres/player state), and check the grid overlay and post-race qualifying times. Also check the refusal paths in game (after green, wrong field), coexistence with Red Flag / Safety Car, and a friend's PC.
