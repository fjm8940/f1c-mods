# PIT_LINK — D8: doc 53 runtime states mapped to the brain registry (BATCH B item 6)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only; docs/53 is NOT edited here). Script:
`watch_race2b` (Imola) and `watch_race4fulld` (Monza), 20 Hz. Static: `sub_429960` (plan builder),
`AIPushLap 0x42A180`, `PitStopDecisionTireCompound 0x42A3D0`, `PitStopBlockingCondition 0x42AEE0`,
`ForcePitExit 0x42A110`, `PostPitCleanup 0x42A6F0`, `RacePhasePitEntryTrigger 0x429EB0`,
AIAbility 0x429470 command-15 block; registry v1.2.

## 1. Mapping: doc 53 state -> brain field / function

| doc 53 state / object | brain registry field | function that drives it | live evidence |
|---|---|---|---|
| `CallToBox 0->1` (request, PitStatus 1) | `call_to_box` 0x454C, `unnamed_4544` 0x4544 (command flag), `unnamed_4584` 0x4584 (strategy flag), `override_enable` 0x4548 | `AIPushLap`: `plan[cursor].lap <= lap_number` -> `ForcePitExit` -> `AIStateFlagSetter(1)`; also mean tyre < 0.9 -> `ForcePitExit`; `PitStopDecisionTireCompound` arms 0x4548/0x4584 | 67 call_to_box edges, all rise in **mode 0**; `unnamed_4544` 0/1 |
| route selector -> pit branch | `waypoint_set_selector` 0x4550, `track_part_from_pit_exit` 0x4450 | AIAbility command-15 block (0x4550 = 1); waypoint set by laneMode | static only |
| `PitEntryLatch` / PitStatus 2 | `driver_status` 0x4510 -> **4** (mode 3 never seen), `speed_limit` 0x4484 held | AIMainUpdate mode switch / AIAbility pit wait | 24/40 (Imola) and 12/40 (Monza) situations contain mode 4; mode 3/5: 0 |
| box stop + service (PitStatus 3, service 7-9.5 s, fuel/tread/wing) | no brain field — box/service belongs to the pit-crew subsystem; `post pit` brain side = mode 4 hold; `speed_limit` = limiter | AIAbility LABEL_57 (command-15 release) / pit-crew state machines (doc 53) | mode 4 present while `call_to_box` clears 1->0 (all 67 edges clear in mode 4) |
| release (PitStatus 4) | AIAbility command-15 block: 0x4544=0, maxSpeed via ParameterValueSetter(18), 0x4550=1, 0x4548=1, clears 0x443C/0x43F8/0x4424, driver_status 4/5; `time_of_next_exit` 0x4534 = now+180 | `ForcePitExit`/`AIStateFlagSetter`, `PostPitCleanup` | mode 4 -> 0, `pit_plan_cursor` +1 |
| pit exit / selector back to main | `driver_status` 0 -> 0, `track_part_from_pit_exit` 0x4450 | AIMainUpdate locator + mode switch | mode 4 -> 0 in every stop; cursor increment follows |
| stop schedule (how many/when) | `pit_plan_count` 0x4588, `pit_plan_cursor` 0x45E0, per-stop lap 0x4590+16i, speed 0x4594+16i, state bits 0x459C+16i (bit 3 = 8) | `sub_429960` at init: .rcd `PitStopStrategy` (1-4 stops, lap strings scaled by race-length fraction) or random fallback (3/4 stops, random gaps; D5); `AIPushLap` compares, `PitStopDecisionTireCompound` writes speed/state, `PostPitCleanup` +1 cursor | plan counts 2 and 3 (no 1/4); cursor 0->1 at every stop (22 transitions shown); per-stop state bit 8 |
| next compound | `next_chosen_tires` 0x458C (`_DWORD`) | `PitStopDecisionTireCompound` v18 (dry = current, wet bands 4/5/6) + `RandomWeightedTierSelection` | value **3** in all 69 289 samples of both dry races; no change at stops |
| pit request block | `PitStopBlockingCondition` 0x42AEE0 | last lap (`lap >= TotalRaceLaps`), session 8, byte 0x39FA, sub-object flags (front-wing condition) -> blocks the stop | - |
| forced entry | `RacePhasePitEntryTrigger` 0x429EB0 / `FuelMultiplierValue` | fuel multiplier < 1 -> `ForcePitExit`; `PitStopDecisionTireCompound` force-schedules on wing/stuck flags | - |

The pit decision chain: **`sub_429960` builds the plan once** (or updates on session events) ->
**`AIPushLap` triggers per planned lap / worn tyres (mean 4-wheel < 0.9) -> `ForcePitExit` ->
`AIStateFlagSetter(1)`** -> `call_to_box` rises in mode 0 -> mode 4 (box) -> AIAbility command-15
release -> `PostPitCleanup` advances `pit_plan_cursor` -> mode 0.

## 2. Live pit-situation findings (80 situations)

- Mode coverage: **0 and 4 only** (40/40 situations include mode 0; 24 (Imola) / 12 (Monza) include
  mode 4). **Modes 3 and 5 never appear** — consistent with the doc-54 correction (mode 2 = one-tick
  reset; mode 3 = close-range pit-spot approach, mode 5 = race-start garage release) and with phase-2
  C1b Run D.
- `call_to_box`: 67 edges; rise always in mode 0, clear always in mode 4. In brain terms the request
  therefore covers entry + box approach, not just the request instant.
- `speed_limit 0x4484`: median 22.22 m/s (80 km/h) at Imola and 27.78 m/s (100 km/h) at Monza = the
  per-track pit limiter; lower distinct values (5.0-18) are traffic clamps in the same cut windows.
- `pit_plan_cursor`: 0 -> 1 exactly once per completed stop (22 shown), always after the mode-4
  interval; `pit_plan_count` = 2 (most drivers) or 3 (Sato, Monteiro, Barrichello, Karthikeyan, ...).
- `next_chosen_tires` = 3 for every sample in both dry races; no compound change across the stops.
- `unnamed_4544` takes 0/1 through the stop (pit command flag being set/cleared).

## 3. Doc 53 section 8 questions vs the brain logs

1. **Effect of AIW records 983-984**: not answerable from brain logs (they have no PitStatus or
   waypoint-record ids; waypoint pointers are per-session heap addresses and differ from the
   2026-08-30 base 0x1784438C). The brain side shows no pass-score field tied to those records.
2. **`nextChosenTires` type/purpose**: **answered in part** — registry v1.2 types 0x458C as `_DWORD`;
   live values are small ints (3), and doc 53's raw `2.802596928649634e-45` is the float reading of
   integer **5** (0x00000005), i.e. a compound id, not a float. A stop selecting a different
   compound was **not observed** in watch_race2b/4fulld (3 -> 3 everywhere); the wetness-band choice
   (4/5/6) is in `PitStopDecisionTireCompound`/`AIPushLap` and needs a wet pit capture.
3. **Physical job ordering inside state 3**: not answerable (no tread/wing fields in the brain ABI).
4. **Slot-5 `2 -> 0` return**: not answerable from the brain side; the brain registers
   `car_conditions 0x3D6C` and `driver_status`, but mode 2 is a one-tick reset (doc 54) and the
   observed pit windows stay `car_conditions == 0` (phase-2 C1b).
5. **Continuous setup/reparse or weather boundary**: no reparse evidence in brain logs; not
   answerable here.

## Limitations

- The brain logs do not carry PitStatus/PitEntryLatch/PitTargetFuel/tread fields; the mapping above
  links brain outputs to doc 53 states, it does not re-measure pit-crew internals.
- 45 s watcher situation caps chain stops into consecutive folders; entry/exit timestamps are
  approximate to the cut boundaries.
- SC/RF mods active on the Monza/Imola runs (no marker in logs); the pit sequences look normal
  (limiter held, cursor increments), no SC-pit anomaly claimed.
