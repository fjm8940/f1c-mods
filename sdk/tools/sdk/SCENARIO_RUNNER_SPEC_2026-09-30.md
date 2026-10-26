# Guarded native scenario runner — specification (S5)

**Date:** 2026-09-30. **Stage:** specify. Nothing is implemented.
**Status: DEFERRED by the owner on 2026-09-30.** Native testing stays in person
for now. Resume only on request. The plan item
is S5 in [LLM_TOOLING_IMPLEMENTATION_PLAN_2026-09-30.md](LLM_TOOLING_IMPLEMENTATION_PLAN_2026-09-30.md).
It is checkpoint objective 1.

## Why

Almost every pending SDK and product capability has passed its deterministic
tests and waits only for **native gameplay acceptance** (checkpoint `pending`
fields, 2026-09-15…23). Today the owner rebuilds each scenario by hand, and the
evidence (logs, crash events, screenshots, memory samples) is collected ad hoc
and differently each time. A runner that drives the existing guarded tools
through a declared scenario and writes one comparable evidence bundle removes
most of that round trip. **It never replaces human validation** of
presentation, sound or feel.

## Existing building blocks (all through the front door)

| Need | Capability | Effect |
|---|---|---|
| Boot straight into a race weekend | `f1cm_guard.asi` menu-skip arm hook (one shot per boot; finding `sessionstart-5cc9e8-the-load-circuit-trigger`: `0x5CC9E8 = 1` at the menu starts the session load) | process start |
| Select track/session | `tool-run --acknowledge-risk configure-session` (player-file edit, with backup and restore) | config-write |
| Keys / menu clicks | `tool-run --acknowledge-risk send-game-key` / `click-game-ui` (the game's own pending-key queue) | live-write |
| Live state with freshness | `memory-mcp` (`f1c_health`, `f1c_drivers`, `f1c_safety_car`, `f1c_trace`) | read-only |
| Screenshots | `tool-run capture-window` | artifact-write |
| Field observation | `runtime-observe`, `memory-snapshot`/`memory-diff` | artifact-write |
| Mod staging / unload | `stage`, `load`, `unload` | process-control |
| Static answers during triage | `ida-query`, `evidence-query`, `inspect` | read-only |

## Scenario file (proposed `scenarios/<family>/<name>.json`)

```json
{
  "schema": "f1c.scenario.v1",
  "id": "reentry-extra-sc-3race",
  "family": "session-exit-reentry",
  "preconditions": {
    "game_closed_at_start": true,
    "installed": [{"path": "plugins/lightloader/safetycar.dll", "sha256": "..."}],
    "ini_expect": [{"file": "plugins/lightloader/safetycar.ini", "key": "sc.add", "value": "1"}]
  },
  "steps": [
    {"do": "launch_guarded"},
    {"wait": "race_running", "timeout_s": 180},
    {"do": "capture", "tag": "race1-start"},
    {"do": "exit_to_menu"},
    {"wait": "menu", "timeout_s": 60},
    {"do": "start_session"},
    {"wait": "race_running", "timeout_s": 180},
    {"repeat_from": 3, "times": 2}
  ],
  "invariants": ["process_alive", "no_new_wer_event"],
  "capture_on": ["step_end", "invariant_failure"],
  "acceptance": {"races_loaded": 3, "crashes": 0},
  "human_checks": ["SC text visible after U in race 2"]
}
```

- `race_running` means `f1c_health` reports the supported game with **advancing**
  simulation time across two samples at least 2 s apart. A static sample never
  counts.
- `menu` means the UI interface pointer is up and the session is not running.
  The exact predicate is to be defined in phase 1 from existing findings.
- `no_new_wer_event` means no new Windows Application Error 1000 for
  `F1 Challenge Online.exe` since the scenario began. Read it through a
  read-only event-log query. **Never copy or parse crash dumps**: they can
  contain sensitive memory.

## Evidence bundle (one directory per run, never overwritten)

`testing/scenarios/<scenario-id>/<UTC stamp>/` containing:

- `RUN.json`: scenario id and version, installed-file hashes before and after,
  EXE hash, timestamps, the step log with results and invariant outcomes,
  pass/fail/blocked, and the list of human checks (always `pending` until the
  owner answers).
- The time window of `LightLoader.log` and of each mod log for the run. Take
  bounded slices only; `pace_probe_standings_log.txt` can be very large.
- Screenshots per capture point, and `memory-mcp` samples with capture time
  and freshness.
- A WER event summary: time, faulting module and offset. No dump contents.

## Safety rules (non-negotiable)

1. The owner explicitly starts each live run in chat. The runner never
   launches the game by itself on a schedule.
2. No QuickRace. Use full race weekends through the guard only.
3. Install and replace mods only with the game closed, through `stage`, and
   with preserved before-copies. The runner itself never edits the game tree
   beyond the approved `configure-session` player-file edit (restored after
   the run).
4. Live writes are limited to the declared key/click inputs and the recorded
   session-start trigger. No other pokes.
5. Abort, and write the bundle as `blocked`, on: an unexpected EXE hash, a
   precondition mismatch, a timeout, the game not closing cleanly or any
   invariant failure.

## Implementation phases

| Phase | Content | Who |
|---|---|---|
| 1 (offline) | Scenario schema and validator; bundle writer; read-only WER query; bounded log slicer; `menu`/`race_running` predicates specified from existing findings; fixtures replaying recorded logs | DeepSeek, from a precise handoff (mechanical), with Claude review |
| 2 (live driver) | `scenario run ID` front-door subcommand chaining the capabilities above; guard launch; exit-to-menu and start-session sequences | Strong model with the owner present |
| 3 (baseline) | Run `reentry-extra-sc-3race` on **public 0.10.66 with add=1**. The expected result is **FAIL (crash)**, which proves the runner detects the known defect. Then run add=0 (expected PASS). | Owner + strong model |
| 4 (use) | Candidate repairs for the re-entry crash (checkpoint objective 2) run the same scenario. Add RF replay-roster and pit-voice families next. | Per product |

## Acceptance for the runner itself

- Phase 3 reproduces the known crash with add=1 and passes with add=0. Both
  bundles are complete.
- Aborted runs leave the game install byte-identical to the preconditions
  (hash check).
- Human checks remain `pending` in every bundle until the owner records a
  result.
