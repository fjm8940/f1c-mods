# dll/ — full_safety_car builds

## Built

| DLL | what it is | status |
|---|---|---|
| `pace_probe.c` / `.dll` | **The working safety car.** Deploys Bernd Maylander from his garage with the race-engineer voice lines, holds a pace speed across the field, assembles it into a queue in race order, prevents overtaking, runs the wave-around, and recalls him. Hotkey-driven, self-ejecting, logs to `<game>/pace_probe_log.txt` | **Phases 1, 3, 4 and 5 PASSED live 2026-07-31**, driver-verified |
| `pace_probe_standings.c` / `.dll` | **FORK (2026-08-07): the SC is NEVER listed.** Loads as an EXTRA car above the grid, hidden from every classification (quali + race) via a load-time count drop, takes the green from the back on his qualipos (game's own placement - no teleports), retires to the garage on lap 1, deploy/recall via the proven garage path. NEVER loaded in Test Day (session 0). Same caution engine otherwise. Logs to `<game>/pace_probe_standings_log.txt`. See `../HANDOFF_2026-08-07.md` | **In live testing 2026-08-07**; `pace_probe.c`/`.dll` unchanged by the fork |

**Controls: Numpad 0 deploys / recalls. Numpad 1 hides him from the pre-race
classification (phase 2 only, and see the handoff before relying on it).**

**HARD RULE: never write to the safety car while the start sequence still
holds the grid brakes.** Arming his retirement, hiding him from the active
array, or re-hiding him during the lights all leave him pinned at
`brake = 1.000`, RPM 0, lap 0, and NOTHING recovers it. If you see that
state, restart the race. First diagnostic for any "he will not move" report:
eject the DLL and run a race with nothing injected.

**Read `../HANDOFF_2026-08-01c.md` FIRST** — the caution laws were rewritten,
driven and accepted on 2026-08-01, and the safety car's own vehicle files were
finally located: he is `SeasonData/Vehicles/FIA/1999_CLK55/`, **not**
`1999_SafetyCar`. Editing `safety.hdv` does nothing. That doc also carries the
solved gear-ratio indexing (place, don't append) and the file trace that finds
which files the game really opens.

**Read `../PACE_PROBE.md` before changing it** — it documents the three control
laws, every offset (including the three that were wrong at first), and six
measurement traps that each produced a confident wrong answer.

Controls — **two keys and a menu** (rewritten 2026-08-01; there were seventeen
global hotkeys before, which is unusable while driving):

```
Num0   open / close the ON-SCREEN MENU   (Up/Down move, Left/Right change,
                                          Enter acts, Num0 closes)
Home   deploy the safety car; once out, bring him in and run the restart
```

Everything else — pace speed, target gap, caution laps, lapped-car policy,
bunching, auto-caution, yellow flag, player inclusion, ramp, parking, field
hiding, telemetry, field dump, panic release — is a row in the menu, drawn on
the game's own 8-slot message strip.

`Num1`/`Num2` stay unbound: they are the red-flag DLL's, which is now a
deliberate separate injection and is out of the mod pack.

**`Home` is the whole feature** — everything else is tuning. Since 2026-08-01
`Home` a second time runs the **full F1 restart**, not a bare release: the car
is recalled, "safety car in this lap" is shown, and when he reaches the pit lane
the cap comes off the whole field at once with the no-passing rule still armed,
each car freed as it crosses the start/finish line.

```bash
python tools/dll_unload_signal.py F1C_PACEPROBE_UNLOAD
```

**A build note:** the linker cannot overwrite `pace_probe.dll` while the game
has it loaded. Self-eject first (command above), build to a temporary name, then
rename over `pace_probe.dll`. Intermediate `pace_probe_vNN.dll` builds are
working files and are not kept — only `pace_probe.dll` and the two dated
verified builds.

## Not done

Penalties for passing under the safety car (F1C's own emitter `sub_41A720` is
unused for this), safety-car lights, and the `AllowAutoPace` / `.aiw
PaceCarReleaseDist` config plumbing.

| planned DLL | covers | NT2004 reference |
|---|---|---|
| `pace_player.c` | manual pace + skip + penalties + HUD | `sub_4FB930`, `sub_4FA450`, `sub_4DF490`, `sub_4DCEB0` |

`pace_lap.c`, `pace_car.c` AND `steward.c` from the original plan are **done** —
all three are `pace_probe.c`. The pace car needed no placement port: F1C's
safety car is a normal entrant, and NT2004's slot-48 hiding maps onto the
active-vehicle array plus `VehicleCount`. The steward needed no port either:
F1C already computes yellow flags per marshalling zone, so the incident scanner
is a **read** of the game's own table rather than a copy of `sub_4F93E0`.
See `../PACE_PROBE.md`.

Build rules (from `extended_vanilla_features/CLAUDE.md`):
- **Every DLL gets self-eject** — named event + `WaitForSingleObject(ev,0)` at
  the top of the poll loop + `FreeLibraryAndExitThread` after the loop breaks.
  Never eject from outside the DLL.
- A DLL that governs the field must **undo its writes before ejecting**.
  `pace_probe` releases the field first; without that every car keeps its
  commanded speed with nothing left to restore it.
- DLLs that hook the **same tick site** must share one DLL.
