# Simplified one-click test panel — GUI rework (2026-09-27)

**Status:** development trial, ``. Off-game verification
only so far (smoke + 16/16 unit tests + import check). A live in-game pass of
the new layout has **not** been run yet, and human presentation validation is
pending.

**Owner request (2026-09-27):** go through the GUI and make it simple
and customized for the mechanical failures mod in-game testing — not cluttered,
not "millions of options", but a simple tool to test all the mech-fail mods.

## What changed

`tools\mech_failure_test_gui.py` (UI layer only; module contracts untouched):

1. **One-click faults.** Each tab leads with big preset buttons that configure
   the shared-memory block and arm in a single press:
   - Gearbox: `JAM IN CURRENT GEAR`, `LOSE 2ND`, `LOSE 3RD`,
     `NO DRIVE (NEUTRAL)`, `CLEAR FAULT`.
   - Engine: `SMOKE (STAGE 1)`, `WEAR TO DEATH`, `BLOW UP NOW`,
     `KILL ENGINE (SEIZURE)`, `OIL LEAK` toggle, `MISFIRE CUTOUT` toggle,
     `REPAIR EVERYTHING`; emitters as three state-highlighted toggle buttons
     plus `ALL OFF`.
   - Suspension: wheels as four toggle buttons (blue = selected, default
     rear), `LIGHT BEND (1)`, `HARD BEND (2)`, `TOTAL COLLAPSE (3)`,
     `REPAIR (zero corners)`, `STOP BENDING (clear)`.
2. **Advanced hidden by default.** The previous wall of settings (modes, per-
   gear masks, skip behavior, recovery, realism phases, radio delay, DNF,
   commentary test line, manual arm, dump/telemetry, per-DLL inject) now lives
   behind one "Advanced options" checkbox in the header. Nothing was removed.
3. **Global controls.** `LOAD ALL 3 DLLs` (injects whichever are missing,
   skips loaded ones) and `RESET EVERYTHING` (gearbox clear; engine repair +
   conditional cutout/oil off + emitters off; suspension clear + repair).
4. **Always-visible status pills** for Gearbox / Engine / Suspension
   (green loaded, amber failing/glitch, red dead/hook-failed, grey not loaded)
   so the state is readable without switching tabs.
5. **State-reflecting buttons.** Oil leak, misfire cutout and emitter buttons
   relabel and highlight from live control-block state each 400 ms tick.
6. **Compact activity log** (5 rows visible, widget capped at 300 rows).
   Events remain in memory; the cap only trims the visible widget.

## What deliberately stayed

- All ABI field maps, magics, `Ctl` semantics, named unload events, injection
  path resolution, adapter contract/validation and `roster_from_reader` — the
  unit tests and `tools\live_check_mech_panel.py` still pass / import unchanged.
- Player-only behavior and the documented AI-adapter gate.
- `mech_test_gui.bat` (unchanged; still self-elevating).

## Verification (off-game, 2026-09-27)

```
python -B tools\mech_failure_test_gui.py --smoke-test
PASS: panel opens; three one-click tabs; advanced hidden by default and shown
      on request; player subject selected while the adapter is absent

python -B -m unittest discover -s tools -p "test_mech_failure_test_gui.py" -v
Ran 16 tests ... OK

python -B -c "import live_check_mech_panel ..."
import OK
```

The smoke test now also asserts the new contract: advanced frames start
unpacked and pack on request. No game memory is written by the panel; it only
writes shared control blocks.

## Pending human validation

1. Open the new layout in a live session and confirm the one-click buttons
   drive the same behaviors as the 2026-09-26 panel (same control paths, new
   UI). The existing driving-dependent checks remain open from the previous
   record: gearbox refusals/redirects/neutral drop while moving, engine stage
   1 while on track, suspension stages with front-only/all-wheel masks.
2. Visual pass on smoke/fire and the status pills' colors.
3. The open `FinishState 3` observation from the 2026-09-26 live check still
   needs a human look at the session/classification screen.
4. A live re-run of `tools\live_check_mech_panel.py` is still valid and can be
   used after the UI pass (it does not depend on the panel widgets).

## Limitations carried over

- `dll\engine_failure.dll` on disk is the 2026-08-07 build; source is
  2026-08-08. Rebuild before release use.
- Suspension prefers the sandbox build; the dev-tree canonical DLL is stale.
- `Num8` collides between engine and suspension hotkeys (irrelevant to the
  panel; use the panel when both DLLs are injected).
- New layout not yet exercised in-game; live evidence for this revision is
  the pending item above.

## Preservation and restoration

- Before-state of the edited files (panel source and README):
  `workspace_organization_audit\GUI_QUICK_REDESIGN_BEFORE_2026-09-27\`
  (`tools_mech_failure_test_gui.py`, `README.md`, plus the restore map).
- Nothing deleted or moved. New doc only.

## Files

| Path | What |
|---|---|
| `tools\mech_failure_test_gui.py` | Rebuilt panel (this record) |
| `mech_test_gui.bat` | Unchanged launcher |
| `tools\test_mech_failure_test_gui.py` | Unchanged contract tests (16 pass) |
| `tools\live_check_mech_panel.py` | Unchanged live-check harness (imports verified) |
