# Unified mechanical failures test panel — build record (2026-09-26)

**Status:** development trial, ``. GUI and tests verified
off-game (smoke + 16 unit tests) and live in a Test Day session on 2026-09-26
(final run 0 failures; see
[trials/2026-09-26_live_check/REPORT.md](../trials/2026-09-26_live_check/REPORT.md)).
Driving-dependent behaviours and visual checks remain pending human validation.

**Owner request:** fork the Pit Stop Failures test panel pattern
(`development\mods\pitstop_modding\tools\pit_test_gui.py`) into one panel that
can test all mechanical failure subsystems and variations on the player, with
AI-driver targets planned as the next step (owner selected "GUI now + AI
adapter next", 2026-09-26).

## What was created

| Path | What |
|---|---|
| `tools\mech_failure_test_gui.py` | The new panel (Python 3, tkinter). Local DLL mode works today; live adapter contract is implemented client-side and fails closed until an adapter answers. |
| `tools\test_mech_failure_test_gui.py` | 13 focused contract tests (ABI maps, block sizes, adapter validation, command builder). |
| `mech_test_gui.bat` | Elevating launcher (injection needs admin), mirrors the old `mechanical_failure_gui.bat` but with correct paths. |
| `docs\MECH_TEST_GUI_2026-09-26.md` | This record. |

The legacy GUI (`f1c_ida_decompiled\mods\extended_vanilla_features\tools\
mechanical_failure_gui.py`) and the old launcher are untouched and still valid.

## Backends

### 1. Local DLL mode (usable now)

The panel is a strict shared-memory client of the three existing DLLs and
never writes game memory itself:

| Tab | Control block | DLL (path resolution) |
|---|---|---|
| Gearbox | `F1C_GEARFAIL_CTL` (`'GFAI'`) | `dll\gear_failure.dll` |
| Engine | `F1C_ENGFAIL_CTL` (`'ENGF'`) | `dll\engine_failure.dll` |
| Suspension | `F1C_SUSFAIL_CTL` (`'SUSF'`, 64-byte `msg`) | SDK sandbox build `f1c_ida_decompiled\mods\sdk_sandbox\suspension_failure\build\suspension_failure.dll`, then `dll\suspension_failure_v5.dll`, then `dll\suspension_failure.dll` |

Mechanics: `Inject DLL` runs the existing `inject.exe`; `Unload DLL` signals
the DLL's named unload event (never `FreeLibrary` from outside); liveness is
detected through the unload event, not stale section magic.

### 2. Live adapter mode (contract only, not built yet)

`Connect to live adapter` probes `http://127.0.0.1:18745/v1/status` for
protocol `mech-test-v1`. No adapter exists yet, so the panel reports that
honestly, keeps the player as the only subject, and local mode stays fully
usable. The client-side validation and command builder are in place and unit
tested so the adapter step is a server-side build only.

Expected adapter contract (same shape as the pit panel's `pit-test-v1`):

- `GET /v1/status` → `protocol`, `test_mode`, `instance_id`,
  `session_generation`, `observed_unix_seconds` (≤3 s freshness), a
  `drivers` roster (`slot`, `object_generation`, `name`, `role` human/ai,
  `retired`), optional `events`.
- `POST /v1/commands` → validated identity + `operation` from
  `gear_stuck | gear_lost | gear_neutral | engine_stage1 | engine_terminal |
  engine_instant | engine_seizure | engine_cutout | engine_oil |
  suspension_stage | repair | clear`, `parameters`, `ttl_seconds` ≤ 300.
- Backend rules to inherit from the pit adapter: one-shot semantics, receipts
  (`accepted/armed/consumed/rejected/expired/cancelled`), no fake success, no
  raw game-thread calls from the HTTP layer, development test mode only.

AI coverage to plan against the documented AI-only vanilla levers: engine
wear timer `veh+0x3D70`, gearbox stuck byte `veh+0x39FA`, AI suspension timer
`veh+0x3D7C`, engine kill (`veh+0x39A0`/`+0xA0`). These are vanilla AI
mechanisms, not the player DLL feature set — AI parity (staged smoke, staged
bend, commentary) is **not** achievable through them and must not be promised
in the adapter.

## Test coverage in the panel

- **Gearbox:** stuck/lost/neutral/both modes; per-gear loss mask; skip vs
  drop-to-neutral; skip-by; neutral recovery timer; HUD banner; phase
  escalation + timing; radio delay; DNF retire; commentary toggle; isolated
  clip test line; blocked/redirected/neutral counters; clip played/GAVE UP
  state; hook health.
- **Engine:** stage 1, terminal, instant, seizure; white/black/fire emitter
  toggles + all off; misfire cutout; oil leak (with live pressure); repair
  all; state dump; telemetry HUD; running/reason/torque/rpm/phase/session
  readouts.
- **Suspension:** stages FAILING/FAILURE/COLLAPSED; per-wheel selection
  FL/FR/RL/RR; clear vs repair; dump; per-corner bend (degrees), accumulator,
  bent-flag readout; sandbox status `msg` line.
- **Panel-wide:** player/AI subject selector (AI populated only by the future
  adapter), timestamped timeline of every action, session phase shown from the
  engine block.

Bluesmoke (`dll\bluesmoke.dll`) is **not** in the panel: it exposes no shared
control block, so it remains controlled by its own Num0/Num*/Num/ hotkeys and
its effect path is exercised through the oil-leak glitch instead.

## Evidence

```
python -B tools\mech_failure_test_gui.py --smoke-test
PASS: panel opens; gear/engine/suspension tabs present;
      player subject selected while the adapter is absent

python -B -m unittest discover -s tools -p "test_mech_failure_test_gui.py" -v
Ran 16 tests ... OK

Connect / find drivers (no adapter present) -> read-only session roster:
  0 human Matias · 1 ai Rubens Barrichello (2002) · 2 ai Takuma Sato (2002)

python -B tools\live_check_mech_panel.py --game-dir "<install>"
final run: 0 failures (Test Day, phase 5, PID 3968, 2026-09-26)
```

Off-game tests run without admin rights and cover the ABI field maps against
the DLL sources (37/31/34(+64) int-slots, magics, eids 80–93, float fields),
path resolution (workspace/injector/sandbox DLL) and the adapter
validation/command builder (staleness, protocol, test mode, retired/duplicate
filtering, TTL bounds, operation whitelist).

The live run injected, exercised and self-ejected all three DLLs through the
panel's own control blocks: gearbox hook/arms/clip-88 playback, engine
dump/telemetry/cutout/oil-leak+eid130/seizure+eid119/emitters/repair,
suspension SDK gate/stage 1+2 bends/eid-75 playback/repair/auto-reset. It also
found and fixed a real defect: the panel's `WORKSPACE` path was off by one
level, so the injector and sandbox suspension DLL resolved wrong (first live
attempt could not inject anything). Regression test added.

## Limitations and risk

1. **Player-only** until the live adapter exists; the three DLLs use the
   static player object and player-only hook sites. The subject list can now
   find the session roster through the read-only SDK reader (added
   2026-09-26, requires the running memory reader), but selecting an AI row
   does not actuate anything yet; the panel labels AI as adapter-gated.
2. `dll\engine_failure.dll` on disk is the 2026-08-07 build; the source is
   2026-08-08. Panel controls match the built feature set (strings verified
   present in the binary during the feature inventory), but the DLL should be
   rebuilt before release use.
3. The suspension tab prefers the sandbox build; the dev-tree canonical
   `suspension_failure.dll` is the stale pre-retune build.
4. `Num8` still collides between engine (state dump) and suspension (wheel
   cycle) when both DLLs are injected; use the GUI for one of them.
5. Panel writes only to shared control blocks. No game memory writes, no
   exe changes, no config changes. Injection requires admin; the launcher
   elevates.
6. No live in-game validation of this panel has been recorded yet.

## Pending validation (human)

1. **Driving-dependent gearbox behaviour:** arm stuck / single lost / multiple
   lost / neutral / both while moving; confirm refused shifts, redirects,
   neutral drop, HUD banner, radio timing and the counters.
2. **Engine stage 1 while on track:** the arm is live-verified but a parked car
   does not drain life; confirm smoke, ReasonCode 1 and bank roll while driving.
   Also a visual pass on white/black/fire smoke and the telemetry HUD.
3. **Suspension on track:** stage 1/2 with front-only and all-wheel masks,
   stage 3 at the clamp; confirm `SusCtl.msg` and the auto-reset on session
   exit / pit return in a non-race session (already observed firing).
4. **Open question from the live run:** the player `FinishState` read 3 after
   the engine-seizure test while still driving (no mod writes 3). Check the
   session/classification screen; investigate only if it persists. See the
   live-check report finding 4.
5. Then bank the AI adapter as the next specified build.

## Preservation and restoration

- New files only; nothing deleted or moved.
- README before-state for the index edit:
  `workspace_organization_audit\MECH_TEST_GUI_BEFORE_2026-09-26\README.md`.
- Remaining before-states: `SHIP_PLAN_BEFORE_2026-09-26\`,
  `FEATURE_INVENTORY_BEFORE_2026-09-26\`.
