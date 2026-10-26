# Failure-synced crew animation and wheel-gun audio: definition (2026-09-28)

Pipeline stage: **define / investigate**. No code changed. Base for any work: `release_candidates/pitstop_failures-v1.0.4-rc.1/source` (not `live/`).

## Owner feedback on 1.0.4 open items (2026-09-28)

1. AI-stop audio, fault stutter, passing volume: owner reports it "works and plays correctly" (human validation, owner report).
2. Log-drain copy cost / per-burst PCM allocation (R3/R5): owner unsure; stays open, low severity.
3. Other antivirus products: no alarm on the owner's machine (single-machine report, not broad coverage).
4. YouTube posts: already posted.

## Objective (owner intent)

Every crew animation reflects the failure actually happening:

- Wheel problems (slow wheel, wheel gun/nut, cross-thread) stop or hold the affected corner's crew instead of finishing natively and then waiting.
- Fuel rig problems (slow/interrupted/no flow, spill): refueller, hose and nozzle stay on the car for the extended refill.
- Wheel-gun sound plays only while the gun is actually working on the nut; no bursts during the delay.

## Current behaviour (FACT, static source)

- Failures extend only the release: `c.target = baseline + extra_job_seconds`, applied with `f1c_pit_hold_after_service` (`live_client.cpp` ~1140-1275). Crew animations run their native script and finish; car then sits held. This is the mismatch the owner sees.
- Gun audio already follows each corner's WHEELGUN channel window, read once at commit (`pit_gun_sync.h`). A faulty gun stutters for its native dwell (`live_client.cpp` ~737-760).
- Known lollipop desync: on held stops the board may turn before release (trial README).

## Prior art (FACT unless noted)

- Crew layout: `vehicle+0x32D4` crew, 120-byte actors named at `+100`, channels (28 B) at `+112/+116`; each channel = `{start_abs, end_abs, 1/len, baseStart, baseEnd, ?, event}` on the stop clock (`docs/HANDOFF_ANIMATION_MAPPING.md`).
- Per corner six actors `FMAN, GMAN, NMAN, WHEELGUN, WHEELOFF, WHEELON`; jacks, lollipop, refueller `ZMAN/HOSE/NOZZLE`.
- 2026-07-29 Frida freeze tests froze the lollipop and one front corner selectively (hook-based, dev tooling only).

## Candidate mechanism (ASSUMPTION, unverified)

Delay or hold an actor by moving its channel window forward (`start_abs`/`end_abs` += delay) instead of hooking. Pausing mid-motion = keep advancing the window each tick. Unknowns to test first:

1. Does the game read the window each frame or cache it at stop start (`0x4A6340`)?
2. Channels written from the plugin thread vs the sim thread: race safety.
3. Does the game use channel ends to decide service completion or release (could end the stop early/late)?
4. Which clip frame is a sensible "stuck" pose per failure (gun on nut, wheel off, nozzle in).
5. AI and player stops both, Normal and Left pits.

Gun audio would then be scheduled from the shifted windows, so bursts land only on real contact.

## Proposed order

1. Read-only probe: log all 34 actors' windows through one normal and one faulted stop.
2. Write probe (dev build, one corner): shift one WHEELGUN/NMAN pair; owner watches.
3. Per-failure choreography table, then implementation, tests, owner trial.

## Pending

Owner decisions on scope and choreography (asked 2026-09-28). No validation performed.

## Owner decisions (2026-09-28)

1. Wheel choreography depends on failure type (draft table below, awaiting owner review).
2. Jacks and lollipop stay up/held while any problem is ongoing: jacks cannot lower and the lollipop must not swing until the car is ready.
3. Engine stall: unchanged.
4. Scope: player and AI stops.

## Draft choreography per subtype (PROPOSAL, owner review pending)

Affected corner = FMAN/GMAN/NMAN/WHEELGUN/WHEELOFF/WHEELON of the chosen corner. Guns sound only while at the nut.

| Failure / resolution | Affected corner | Gun audio |
|---|---|---|
| slow_wheel minor | old wheel off normally; new-wheel fitter (NMAN/WHEELON) arrives late by the delay | normal bursts, second burst shifted |
| slow_wheel awkward | new wheel held at the hub (fumbling) for the delay, then fitted | tightening burst only after fit |
| wheelgun quick_fix | gun holds at the nut for the delay | stutter, then burst |
| wheelgun backup_gun | gun pulled back and returns after the delay | silence during swap, then burst |
| wheelgun irreparable_nut | gun holds at nut until retirement | stutter, then silence |
| cross_thread corrected | gun holds at nut (back off and re-seat) | stutter, pause, burst |
| cross_thread lengthy_rework | as corrected, longer hold | repeated stutters |
| cross_thread irreparable | as irreparable_nut | as irreparable_nut |
| fuel_rig slow_flow | ZMAN/HOSE/NOZZLE stay on car for the extra time | none |
| fuel_rig temporary_no_flow / interrupted_flow | nozzle stays in, refuel resumes | none |
| fuel_rig spill | nozzle stays in for the delay | none |
| stall | unchanged | unchanged |

All faults: jacks, TMAN/RMAN and lollipop hold until the delayed job finishes; other corners finish normally.

## Fuel correction (owner, 2026-09-28)

Owner expects the game itself to keep the fuel rig on the car for as long as fuelling runs. Consistent with source (FACT): `fuel_effect.h` does not hold the release for slow/no-flow/interrupted; it changes the native fuel flow (`fuel_start`/`fuel_start_time`, 12 L/s), so the game's own refuel lasts longer. Fuel rows therefore need no animation work, pending probe confirmation that the ZMAN/HOSE/NOZZLE windows follow the refuel duration.
Possible exception (INFERENCE): spill cuts the fuel target, then holds the car ~3 s for cleanup (`cleanup_until`), so the rig may leave while the car is still held. Probe will show; owner to judge whether that looks wrong.

## Owner acceptance sequence (2026-09-28)

For every failure: work starts; the issue arises (HUD + radio); the issue is worked on with a per-failure animation; the issue is fixed and the work completes: HUD + radio only when the work is done; lollipop and jacks release when everything is ready; normal exit. The car stays on the jacks while the failure is ongoing. Trial progress: `trials/failure-anim-probe-20260928/README.md` (r7 implements this for held wheel failures).
