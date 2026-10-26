# Oil on track from engine failures - feasibility (2026-09-28)

**Owner question:** can engine issues leave *visible, physical* oil leaks -
oil you can see on the tarmac, and that makes the track slippery?

**Status:** research only. No code or game file changed. Evidence tiers:
FACT = live-proven or read from files; INFERENCE = reasoned, unproven.

## Answer in short

| Part | Possible? | Confidence | Effort |
|---|---|---|---|
| A. Visible oil trail on the track (dark marks where the car leaked) | **Probably yes** | medium | small spike (1 test session) |
| B. Oil makes the track slippery for the **player** | **Possibly** | low | real RE work (days) |
| C. Oil makes the track slippery for **AI cars** | **Unknown / hard** | low | larger RE work |
| D. Presentation (marshal oil flag line, AI lifting) | partly | - | small, after A |

## A. Visible trail - existing levers

- FACT: the SpecialFX reaction table holds ground-mark effects defined in
  `SeasonData\Circuits\terrain.ini`: `skid` (skidhard.tga, Max 2048),
  `softskid`, `wetskid` (Duration 0.40), `groove` / `wetgroove`
  (wetline.tga, Max = NumWaypoints), all `Particle=Plane+Deformable+SingleSided`
  = flat marks laid on the road.
- FACT (findings.jsonl `bluesmoke-rain-id13-crashes-emitter`, 2026-08-07):
  the effect browser drove **every** live entry through ParticleTrailEmitter
  (the call our oil leak / fireball already use) - skid x2, softskid,
  wetskid, groove, wetgroove all toggled cleanly; only RAIN (id 13) crashes.
- UNKNOWN: whether those emits actually render as ground marks where we put
  them, how long they persist, and whether they can be tinted. Tinting uses
  the renderer COLOR (desc+80 -> +96, proven on flames), but it is per
  descriptor: if colour is applied at render time, every car's skid marks
  would turn oil-coloured while the leak runs. `skidhard.tga` is already dark,
  so untinted marks may be good enough.
- Plan: spike = emit `skid` / `wetgroove` under the player every tick while
  the oil leak runs; check look, orientation, persistence, frame rate.
  Never write Max (the fuel_fire_lab pool-cap freeze).

## B/C. Real grip loss - what exists and what is missing

- FACT: surface grip comes from `terrain.ini` per material
  (`RoadDryGrip=1.00`, `RoadWetGrip=0.80`, rumble 0.95/0.60). These are
  **track-wide per material**, loaded at session start - editing them would
  make the whole circuit slippery, not a patch.
- FACT (docs/12-globals-reference.md): the tyre model's grip multipliers
  (`DAT_005AD1B4` DryGripMultiplier, `DAT_005AD230` ContactGripMultiplier,
  `DAT_007D020C` WetGripMultiplier) are **global** - every car, everywhere.
  Unusable for a local oil patch.
- INFERENCE: `TireGripCoefficient_004e88b0` reads four per-wheel records at a
  248-byte stride (`this+13772/14020/14268/14516`); one of the fields per
  wheel is likely the grip of the surface under that tyre. Not identified.
- Needed for B: find that per-wheel surface-grip field (x32dbg write
  breakpoint while a wheel crosses road -> kerb), then scale it down from the
  **game thread** when a wheel is inside a recorded oil patch. The
  suspension family already detours the player's per-frame physics
  (`VehicleMainPhysicsUpdate_004441f0`), so a player-only hook site exists.
  A worker-thread write would lose the race with the per-frame recompute.
- C (AI): AI cars run a different control path; whether their tyres read the
  same per-wheel field is unknown. Until proven, oil would only affect the
  player - the car that leaked it, which drives away from it.

## D. Presentation

- eid 130 marshal warning already plays on the oil leak. No oil-flag
  (yellow/red striped) mechanism is known in the game.

## Recommendation

1. Do spike A first (cheap, visible, low risk). If marks render well, ship
   the trail with the oil leak and overheating/blow-up.
2. Treat B as a separate research task with its own spec; C only after B.

## Spike A - BUILT (2026-09-28), awaiting owner test

- Engine ABI 6 -> 7 (200 -> 216 B): `trail_id` (default 6 SKID),
  `cmd_trail_toggle`, `trail_on`, `trail_max` (-1 = descriptor missing).
- `trail_pump()` (engine_failure.c): while the oil leak runs, or while the
  panel test is ON, emits 2 particles/tick (40/s) of the chosen reaction at
  the car position, 0.30 m below the body reference, zero velocity, rate 1.0.
  Id 13 (RAIN) refused; Max never written; only in live sessions.
- Simple panel, ENGINE card: `Oil trail test: SKID / SOFT / WET / GROOVE /
  WETLINE` + `TRAIL: on/off`. Old panel ENG_F updated; tests OK.
- Log line `TRAIL: reaction id N found/MISSING (Max=..)` per chosen id.
- Before-state: `workspace_organization_audit/OIL_TRAIL_SPIKE_BEFORE_2026-09-28/`.
  Patch: `trials/realism_2026-09-28/patch_oil_trail.py`.
- Not installed (game running). Not live-validated.

Owner test: turn the trail ON, drive a lap, look behind (replay/TV cam) -
for each type note: visible? on the ground or floating? how long it stays?
oil-like? any stutter?

## Spike A - live result: GAME FROZE (2026-09-28)

- FACT: installed build `f3d3a330…` (18:27:30). engine log, last lines:
  `TRAIL test ON (id 6)` / `TRAIL: reaction id 6 found (Max=2048)` - then
  nothing; owner: "froze the game"; process gone by 18:29.
- INFERENCE (unproven): skid marks are Deformable plane *trails*; our emits
  used zero velocity (the 2026-08-07 browser, which did not freeze, used
  jittered non-zero values) and one fixed point per tick, so a degenerate
  direction / strip joint is the likeliest hang. Rate 40/s is a second
  suspect.
- Action: kill switch - `trail_pump()` never emits (oil leak included);
  `trail_id` default 0. Built + installed `d99c9022…`. Panel row labelled
  DISABLED. Before-state: `workspace_organization_audit/OIL_TRAIL_FREEZE_BEFORE_2026-09-28/`.
- Next, if the owner wants to continue: test ONE particle per second with the
  car's own velocity direction, starting with WETGROOVE/GROOVE, on a
  throwaway session - or park spike A. Do not re-enable SKID blindly.
