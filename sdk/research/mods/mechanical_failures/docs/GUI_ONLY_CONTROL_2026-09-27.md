# GUI-only control — all in-game hotkeys removed (2026-09-27)

**Status:** development trial, ``. Sources edited, engine
and sandbox suspension rebuilt and reinjected into the live race (PID 908).
Off-game tests pass. A 30-second manual key test and the usual driving checks
remain pending.

**Owner request (2026-09-27):** remove ALL keybinds from activating mechanical
failures; testing should be done ONLY through the GUI.

## What changed

1. **Engine (`dll\engine_failure.c`)** — removed the `HOTKEYS` table and its
   20 Hz polling loop, `handle_key()`, and every key reference (numpad
   Num3–Num0, top-row 1/2/3/4, `C`, `O`). Arming, cutout, oil leak, emitters,
   repair, dump and telemetry now arrive only through `F1C_ENGFAIL_CTL`.
   Header/comment/log text updated; the startup log now prints
   `GUI-controlled only (in-game hotkeys removed 2026-09-27)`.
2. **Suspension (sandbox port `suspension_failure.c`)** — removed
   `handle_num7()` / `handle_num8()` and the Num7/Num8 polling block. Stages,
   wheel masks and repairs arrive only through `F1C_SUSFAIL_CTL`. Comments,
   startup log and the sandbox README controls bullet updated.
3. **Gearbox** — already GUI-only (no keys existed); untouched.
4. **Test panel** — suspension Advanced hint now says GUI-only instead of
   mentioning Num7/Num8.
5. **README** — engine and suspension Control sections rewritten as
   GUI-only; the wheel-selector wording no longer references Num8.

## Rebuild and live swap (same race)

- Engine: `gcc -shared -O2 -Wall -o engine_failure.dll engine_failure.c
  -lkernel32` → exit 0, no warnings, 237,890 bytes. This rebuild also clears
  the previously flagged Aug-7-binary vs Aug-8-source mismatch.
- Suspension: `python tools\f1c_mod.py build mods/sdk_sandbox/suspension_failure`
  → `Build verified`.
- Both DLLs unloaded through their named events (self-eject observed), then
  reinjected: engine hb advancing, suspension hb advancing, both control
  blocks attaching. The game kept running throughout.
- Engine log (game dir) confirmed the new binary:
  `GUI-controlled only (in-game hotkeys removed 2026-09-27); commands arrive
  via F1C_ENGFAIL_CTL` at 2026-09-27.

Observed post-reinject engine state (game state, not a defect): `life=0.000
running=0 reason=0` — the earlier testing drained engine life; `REPAIR
EVERYTHING` in the panel revives the car.

## Deliberately unchanged

- Dev-tree suspension sources (`dll\suspension_failure.c`,
  `suspension_failure_v5.c`) keep Num7/Num8: they are the historical
  live-proven builds kept as evidence; the sandbox port is canonical.
- `bluesmoke.dll` keeps its own keys: separate experiment, not one of the
  three failure DLLs and not controlled by the panel.
- Control-block ABIs unchanged — old and new DLLs are interchangeable with
  the panel.

## Verification (off-game)

```
python -B tools\mech_failure_test_gui.py --smoke-test   -> PASS
python -B -m unittest discover -s tools -p "test_mech_failure_test_gui.py"
Ran 16 tests ... OK
```

## Pending human validation

- Key test in-race (30 s): Num3/Num6/Num8/Num9/1/2/3/4/C/O must do nothing
  now; confirm no "NO LIVE SESSION" HUD flashes from keys either.
- The driving-dependent checks from 2026-09-26 remain open, plus a visual
  pass on the rebuilt engine binary (its Aug-8 source features were never
  individually live-checked as a built pair).

## Preservation

Before-states: `workspace_organization_audit\GUI_ONLY_CONTROL_BEFORE_2026-09-27\`
(engine .c/.dll, sandbox suspension .c/.dll, panel, README, reconstructed
sandbox README, restore map). The sandbox README before-state is a
reconstruction from its pre-edit read; the copy is otherwise exact.
