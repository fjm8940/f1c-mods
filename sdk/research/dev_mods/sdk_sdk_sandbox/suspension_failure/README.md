# Player suspension failure (sandbox port)

Sandbox port of the live-proven legacy mod
`mods/extended_vanilla_features/patches/mechanical_failures/dll/suspension_failure.c`
(2026-08-08) plus the 2026-08-14 auto-reset fix.

## What it does

Gives the **player** a staged suspension failure by driving the game's OWN
bend input — the exact call the game makes on a wheel impact:

```
SteeringGeometryUpdate_00442060(0x7C7428, wheel, severity)   // game thread only
```

- Detour on `VehicleMainPhysicsUpdate_004441f0` (player per-frame physics
  tick). 12 stolen bytes, 6-byte push-imm32/ret patch — the prologue has no
  5-byte whole-instruction boundary, so `f1c_hook.h`'s 5-byte detour cannot
  be used at this site. This site is NOT the broker's main-update anchor
  (`0x0042C690`) or RestartGate (`0x004A0FD0`), so it does not duplicate a
  broker hook.
- Stages (applications x severity; game's own ±10° clamp bounds the result):
  FAILING 2×0.30, FAILURE 3×0.60, COLLAPSED 30×1.00.
- Radio eid 75 "Damaged suspension - avoid kerbs" once per arm.
- Controls: GUI only — shared block `F1C_SUSFAIL_CTL` (Suspension tab of the
  test panel, `development/mods/mechanical_failures/tools/mech_failure_test_gui.py`).
  In-game hotkeys (Num7/Num8) were removed 2026-09-27.

## Auto-reset (2026-08-14)

Damage, HUD text and arm state are cleared automatically when the player

1. leaves the session (`RACE_PHASE <= 4`),
2. retires in a race (`SESSION_TYPE == 7` and `FinishState >= 2`), or
3. returns to the pits in a non-race session (`SESSION_TYPE != 7` and
   `pittingStatus >= 2` — Test Day 0, Practice 1-4, Qualifying 5, Warmup).

Driver report 2026-08-14: "SUSP FAILURE" text remained on the HUD strip and
the bend damage survived ESC → pits. The mod now resets itself: corner
fields → 0, ReasonCode (only when ==4) → 0, radio pending dropped. Pure
data writes, worker-thread safe (same stores the GUI REPAIR command
performs); the steering-geometry CALL stays game-thread-only.

## Auto trigger (2026-09-28, ABI 2)

One seeded roll per live race/qualifying session, same scheduler pattern as the
gearbox/engine DLLs: default chance 15%, type split FAILING 45 / FAILURE 35 /
COLLAPSED 10 with the remaining 10% a clean no-fault session, wheels rear 50 /
all 30 / front 20, onset from lap 3, COLLAPSED downgraded inside the final 2
laps, never an auto-retirement. The pump arms only through the same
`arm_stage()` path the GUI uses and logs `AUTO:` lines next to the DLL.

- ABI 1 -> 2: ten int fields appended after `msg` (all old offsets unchanged;
  panel indices jump 33 -> 49). `_Static_assert(sizeof(SusCtl) == 236)` pins
  the layout.
- **ABI 3 (2026-09-28):** optional car-flavor scaling — the auto-roll chance
  is multiplied by `clamp(fieldMean / playerCounter, 0.50, 2.00)` using the
  player's frozen suspension lifetime seed (`veh+0x3D7C`, seeded from
  `engine.ini` `LifetimeAvg/Var` by the game) vs the live field mean over the
  24-vehicle array. Off by default; three int fields appended after the auto
  fields; `_Static_assert(sizeof(SusCtl) == 248)`. See
  `development/mods/mechanical_failures/docs/CAR_FLAVOR_WEIGHTING_2026-09-28.md`.
- **ABI 4 (2026-09-28, default source):** vanilla lifetime timer — our ticker
  drives the car's own game-seeded suspension counter (`veh+0x3D7C`) at the
  game's AI rate (1.0/game-second from green, session clock `dbl_8AFEA0`) and
  fires the family fault on expiry; the role byte is never flipped. Two int
  fields appended (`timer_enable`, `timer_remaining`);
  `_Static_assert(sizeof(SusCtl) == 256)`. Build record:
  `development/mods/mechanical_failures/docs/VANILLA_TIMER_MODE_2026-09-28.md`.
- Panel: Suspension tab -> Advanced -> "Auto trigger" (enable, timer/roll
  source, chance, onset lap, guard, live countdown/status). Build record:
  `development/mods/mechanical_failures/docs/SUSPENSION_AUTO_TRIGGER_2026-09-28.md`.

## GUI status instead of in-game text (2026-08-14)

All in-game HUD text-box messages are removed. Status travels through the
shared control block (`SusCtl.msg`) and is shown on the GUI's Suspension tab
("SUSP FAILING", "SUSP OFF", "SUSP REPAIRED", "SUSP WHEELS ...",
"NO LIVE SESSION", "AUTO-RESET (...): cleared"). The GUI injects this
sandbox build (`build/suspension_failure.dll`).

## Build

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure
```

**Merged build (2026-09-28):** this source is also compiled as one object of
`development/mods/mechanical_failures/dll/mechanical_failures.dll`
(`build_merged.bat`; entry point renamed to `DllMain_susp`, exit via
`mf_exit.h`). Standalone behaviour is unchanged; the header sits next to this
source so the canonical build above keeps working.

## Unload

Signal the clean self-eject event `F1C_SUSFAIL_UNLOAD` (GUI Unload button
or `python tools/f1c_mod.py unload F1C_SUSFAIL_UNLOAD`). Never eject via
external `FreeLibrary` — the DLL runs a worker thread.

## Evidence

- `findings.jsonl`: suspension bend located + live-proven (2026-07-29),
  control input found + live-proven (2026-08-08).
- Legacy doc: `mods/extended_vanilla_features/docs/SUSPENSION_DAMAGE.md`.
- Provenance of the mechanics: legacy `suspension_failure.c` (v4 = retuned
  stage 2, live-driven 2026-08-08).
