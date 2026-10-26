# synthetic_pitstop — two-phase synthetic pit stop DLL

Injects a pit stop onto an AI car at race start with **zero vanilla
activation** — no pit call, no XPITIN crossing, no `StateChangeNotify`
dispatch. The stop is split into two independent phases, exactly as designed
after the 2026-08-05 full-mechanism trace.

## The two phases

| Key | Phase | What happens |
|---|---|---|
| **Numpad 1** | A — ACTIVATE | Car teleported into its own pit box (game's own box resolver `sub_4D1100`), `pittingStatus=3`, entry latch, laneMode 1, `serviceReady=0`, crew activated (`sub_4A3390`) + positioned (`sub_4A3320`). **`PitStopReadyCheck` (0x4A6200) is GATED** so the state machine cannot start service. Car sits activated, crew standing, nothing else. |
| **Numpad 2** | B — SERVICE | Stop-active flag (`veh+0x32F8=1`, crew anim clock) + tyre/fuel job flags, **gate removed**. From the next tick the game's own machinery runs: `serviceReady` latches, `PitStopOperationTimer` schedules `stopEndTime`, crew channels animate, `PitStopProgress` releases at `stopEndTime`, AI command 15 applies the limiter, car drives out of the lane itself. |
| Numpad 0 | ABORT | Ungate + clear stop-active, leave the car where it is. |

## Why the gate is mandatory (FACT, traced 2026-08-05)

`PitStateUpdateMachine` (0x4A6E50) runs every frame; in state 3 with
`serviceReady==0` it falls through to `PitStopReadyCheck` (0x4A6200), which
would set `serviceReady=1` and start service immediately. "Activated but not
servicing" is unrepresentable in vanilla state, so the DLL patches the ready
check's head to `ret 4` while gated:

```
original 83 EC 18 56 8B      (sub esp,18h; push esi; mov esi,ecx)
gated     C2 04 00 90 90      (ret 4 ; nop ; nop)
```

The prologue is **re-verified against the live process before every gate**
— if it ever differs, the DLL refuses to patch rather than corrupt code.

While in Phase A the poll loop re-asserts the activated state at 20 Hz so
`PitStateUpdateMachine` cannot consume it (same arming pattern as the
proven `armed_entry` recipe).

## Preconditions

- Inject at **race start** while all cars are stationary on the grid
  (the DLL refuses Phase A if the target is moving faster than 1 m/s).
- Target = the first AI car (slot with `veh+0x32C8 == 1`); its identity is
  logged.
- The crew set must exist (built at load; player car always, AI cars only
  when `Pitcrew Detail` ≥ 3 — check the log for "WARNING - no crew set").

## Session gates

- `RACE_PHASE` gate is NOT enforced by the DLL (deliberate: the design is
  "teleport at race start", and the speed check is the honest guard).
- Phase B requires Phase A first (no service without activation).

## Files

- `dll/synthetic_pitstop.c` — source
- `dll/synthetic_pitstop.dll` — build
- `dll/synthetic_pitstop_log.txt` — runtime log (created in the game dir)

## Build

```
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o synthetic_pitstop.dll synthetic_pitstop.c -lkernel32 -luser32
```

## Inject / eject

- Inject: any loader into `F1 Challenge Online.exe` (32-bit).
- Self-eject (safe): `python tools/dll_unload_signal.py F1C_SYNTHPIT_UNLOAD`
  — the DLL restores the gate and frees itself. Never `FreeLibrary` from
  outside (crashed the game once, 2026-07-28).

## Evidence base (findings.jsonl, 2026-08-05)

- `pitcrew-activation-chain-complete-static` — full StateChangeNotify →
  AIStateMachineWithModeDispatch → sub_4A3390/sub_4A3320 chain
- `pitcrew-activation-direct-call-live` — direct sub_4A3390 call works live
- `pitcrew-organic-call-runs-activation` — organic path runs crew itself
- `pitcrew-organic-midrace-confirm` — organic call lever on a mid-lap car
- `pitcrew-visibility-timeline-expanded-organic` — crew 4→0→34 across a stop
- `pitstop-call-clears-calltobox-at-box` — callToBox clears at box entry
