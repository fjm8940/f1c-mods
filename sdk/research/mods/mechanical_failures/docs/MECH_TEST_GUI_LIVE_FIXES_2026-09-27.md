# Panel live-use fixes — reader auto-start + text activity log (2026-09-27)

**Status:** development trial, ``. Off-game verification
passed (smoke + 16/16 unit tests). A live press of the updated *Find drivers*
path and the remaining driving checks are pending.

## What the owner hit live (fact)

- **"Not finding other drivers":** the reader roster needs the hidden elevated
  SDK session reader. It was not running, so `Find drivers` fell back to an
  error instead of the roster. The reader was started through the canonical
  `python tools\f1c_mod.py memory-mcp start-reader`, after which the roster
  listed the session (Matias slot 0, Jarno Trulli / Justin Wilson AI slots
  1–2; health captured 2026-09-27, `freshness: advancing`, game
  PID 908).
- **Error text was mojibake** (`'MÃ¤Ã¤ritettyÃ¤...'`): the SDK front door
  emits UTF-8; the panel's `subprocess` capture decoded with the Windows
  locale. Fixed by explicit `encoding="utf-8", errors="replace"`.
- **"Activity log appears broken":** owner screenshot showed color blocks /
  vanished text in the Treeview cells. A direct window capture on the same
  build rendered the same log normally — the artifact was not reproduced
  (inferred: Treeview selection/repaint quirk under load). The widget was
  replaced anyway for robustness and simplicity.

## Changes

1. **`Find drivers` auto-starts the reader** when the roster call fails and
   the game is running (`start_reader()` → canonical front door
   `memory-mcp start-reader`, then one retry). Repeated starts are safe; the
   front door reuses an existing healthy reader. The panel is already
   elevated, so no extra UAC step is expected. When the game is not running
   the connect path reports that instead of trying.
2. **Activity log is a read-only `tk.Text`** with time / source / message
   tags and a scrollbar (400-line window). No selection or repaint artifacts
   possible; visually simpler. The `events` list is unchanged.
3. **UTF-8 decoding** for both `memory-mcp` calls (`drivers`,
   `start-reader`).
4. `adapter_failed` text now states that `Find drivers` starts the reader
   automatically with the game running.
5. **Engine pill logic corrected.** Live state showed `running=1 reason=1`
   (latched failure from earlier testing) and the pill said red "ENGINE
   DEAD". Now red means the engine is actually stopped; a latched ReasonCode
   reads amber "ENGINE FAILED", glitches amber "ENGINE GLITCH". Pressing
   REPAIR EVERYTHING clears the latched flag.

Unchanged: ABI maps, `Ctl`, unload events, injection, adapter contract,
subject roster mapping. `tools\live_check_mech_panel.py` remains compatible.

## Verification

```
python -B tools\mech_failure_test_gui.py --smoke-test   -> PASS
python -B -m unittest discover -s tools -p "test_mech_failure_test_gui.py"
Ran 16 tests ... OK
```

Live evidence gathered while the owner was driving: health and drivers
snapshots above; the running panel showed the gearbox DLL armed (blocked /
redirected / neutral counters advancing, radio eid 81 played, 14/14 clips
unblocked at load). Those are observations of the previous process build;
the fixes above still need one live press to confirm the new connect path.

## Pending human validation

- Press **Find drivers** on the updated build with the game running (reader
  already up now; also verifies the auto-start path next reboot/expiry).
- The driving-dependent checks remain open from the 2026-09-26 record
  (gearbox refusals/redirects while moving, engine stage 1 on track,
  suspension stage 1/2/3 and wheel masks, smoke/fire visuals).
- Open question carried: player `FinishState` read 3 after a seizure test
  (see `trials\2026-09-26_live_check\REPORT.md` finding 4).

## Preservation

Before-states: `workspace_organization_audit\GUI_QUICK_REDESIGN_BEFORE_2026-09-27\`
(original panel, one-click v1 snapshot, both README states, restore map).
