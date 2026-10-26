# AI Organic Pit Stop — full field map (live trace, Monza 1999)

Whole-struct capture at 20 Hz, 75 s, on Olivier Panis (slot 1). Organic pit
call fired externally (veh+0x4590 = current lap + veh+0x413C = 0 re-arm).
Raw: `panis_trace.bin` (8-byte double timestamp + 0x48C0 bytes per frame).

## Sequence timeline (all times = seconds after trace start)

| t | event | fields |
|---|---|---|
| 23.8 | **pit call accepted** | pit=1, callToBox(0x454C)=1, stopIndex, 0x413C→FLT_MAX (disarms), 0x3454 stamped, 0x1064/0x42E4 flicker |
| 24.0-25.3 | in-lap braking | gearbox downshifts 6→5→4→3 (0x39FC/0x3A00), 0x4468 3→2, 0x42F0=0x10 |
| 34.3 | lane set selected | laneMode(0x4550)=1, 0x443C=1 (in-lap flag), 0x4174→player ptr |
| 38.6 | **XPITIN entry** | pit=2, 0x344C=1, limiter(0x4538)=22.222, 0x1068\|=0x100, 0x3454 stamped |
| 48.7 | **box arrival** | pit=3, serviceReady(0x345C)=1, stopStart(0x346C), stopEnd(0x3470)=now+8, fuelAtStart(0x347C), targetFuel(0x3484), refuelFlag(0x348C)=1, tyreChangeFlag(0x3490)=1, tyre-job window (0x349C/0x34A0), dS(0x4510)=4, **AIControl(0x45F4)=0** (game takes the car), callToBox→0, 0x4548=1, 0x4468→0, 0x4174→NULL |
| 56.8 | **release** | pit=4, dS→0, **AIControl→1**, duration(0x3474)=8.53, stopIndex(0x45E0)=1, 0x3454 stamped |
| 61.8 | **exit complete** | pit=0, 0x344C→0, limiter→99999, 0x1068 bit8 cleared, 0x4548→0 |
| 62.5 | lap complete | lap(0x403C)=3, PitState(0x4040) 2→0, 0x4458 2→3, 0x413C re-arms (next stop decision) |
| 66.5 | lane set released | laneMode→0 |

## New facts

- **veh+0x3454 is the pit-machine last-TRANSITION timestamp** - it gets
  re-stamped at EVERY pit state change (call, entry, box, release, exit),
  not just at entry. The old value goes to 0x3458 (written once at the call).
- **veh+0x4040 = 2 is the NORMAL racing value for AI cars** (constant 2 the
  whole trace on a healthy racing car). It drops to 0 only after a pit
  sequence. The synthretire recipe's "PitState=2" is therefore REDUNDANT on
  AI cars - the freeze's active ingredients are FinishState=2 + AIControl=0.
- **0x4544 flickers 1↔0 during the in-lap** (34.3-43.7 s) - a pit-request
  indicator, not a latch.
- **0x443C = 1 during the in-lap** (34.3-45.8 s), in sync with laneMode.
- **0x4174 = the AI's target-car pointer** (points at other vehicles' bases,
  NULL while boxed).
- **Gearbox**: 0x39FC/0x3A00 = current gear (0 = between shifts),
  0x3A14 = target gear.
- **0x4468 = pit-phase enum**: 3 racing → 2 in-lap → 1 approach → 0 boxed →
  1 release → 2 post.
- **0x42F0 / 0x430C / 0x42E4** = AI state machines that flicker at the call
  and the entry crossing (0x430C 36-41 s, 0x42E4 ends at 0xFFFFFFFF at the
  lane-set moment).
- **0x453C** = the AIAbility governor, decays steadily ~-0.01/4 s (the
  known "recalc every frame" field).
- **0x4020** = a steadily rising session/mileage counter.

## The organic call recipe (FACT, re-verified)

```
veh+0x4590 = current lap (veh+0x403C)   # first stop scheduled this lap
veh+0x413C = 0                          # decision-timer re-arm
```
Call → pit=1/callToBox=1 in <1 s → 15 s in-lap → real XPITIN (six-field
signature) → box (AIControl drops to 0 - the game's own park) → crew service
(8.5 s measured) → release → clean lane exit. No forged flags, no teleport.

## Track-agnostic pit-start validation (2026-08-04)

	ools\validate_lane_start.py (also copied to tools/) parsed ALL 210 .aiw
files: the pit lane (branch-1) entrance = min-index lane waypoint; the stall
reference = lane node nearest team-0 PitPos. Result: 209/209 real tracks OK,
0 EDGE, 0 FAIL (1 NO-LANE = 'Blank' template). The lane always starts before
the stalls, hence before xpitin. Recipe: place safety car at the lane
entrance node, yaw = bearing to the next lane node, organic call
(0x4590=lap + 0x413C=0) - he crosses the entry line on every track.
