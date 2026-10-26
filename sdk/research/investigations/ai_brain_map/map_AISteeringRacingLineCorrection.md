# map_AISteeringRacingLineCorrection — section-3 name `AISteeringRacingLineCorrection`, declared `sub_42B0E0`

## Identity

- Address: `0x42B0E0`; file `functions/AISteeringRacingLineCorrection_0042b0e0.c`; 263 lines.
- Signature: `void __thiscall sub_42B0E0(float *this)`; `this` = F1CVehicle (`this[N]` byte offset = `N * 4`;
  `this + 3` = vehicle + 0xC).
- Caller: `AISteeringCorrection` (`0x426CF0`) L79; grep `functions/` for `AISteeringRacingLineCorrection(` ->
  `functions/AISteeringCorrection_00426cf0.c` only; `inspect` agrees (callers: 1).
- Callees (from `offsets_AISteeringRacingLineCorrection.csv` CALL rows): `DistanceProjectionCalc`,
  `DistanceComparison`, `TrackParameterMinimumClamp`, `InterruptDispatch`, `RandomWeightedTierSelection`,
  `LoopWithConditionalSubCall`, `SpeedValueSetterWithGlobalMirror`, `VehicleClimbAdjust`,
  `SingleFunctionDispatch_10e0`, `F_3DTransformWithMatrixMultiplication`, `EulerToRotationMatrix`,
  `VehicleDynamicsStateReset`, `AIVehiclePathPositionTracking`, `ConditionalGate`, `sub_403CD0` (`sqrt`, `atan2`,
  `fabs` intrinsics and `LODWORD` macro rows are not calls).
- Prior art: `S1-placement-native-abi` (CONFIRMED) — this is the native caller that passes ECX = vehicle+12 to
  `F_3DTransformWithMatrixMultiplication` (0x4EFB20, get pose), `AIVehiclePathPositionTracking` (0x4EF7C0, set pose)
  and `VehicleDynamicsStateReset` (0x4F2320); reset-then-set in the L252-L259 branch.
- Prior art: `aiw-lateral-sign-positive-is-right-2026-10-02` (DERIVED) cites L210-L221: positive steer command
  0x442C turns toward +lateral; this function and AIAbility write/read 0x4420/0x4424.
- Names marked (inspect) come from the navigation-only inspect view; only CONFIRMED/FACT tags are copied.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4060 | 16480 | _DWORD | L54 | position (inspect CONFIRMED) |
| 0x4440 | 17472 | float | L86 | Speed (doc 54) |
| 0x4434 | 17460 | float | L100 | throttle candidate (doc 54) |
| 0x3AA8 | 15016 | _DWORD | L105 | ? pointer (`+0x12C` checked) |
| 0x39A8 | 14760 | float | L112 | ? requested RPM (compared with 0x39A0) |
| 0x39A0 | 14752 | float | L112 | EngineRPM (inspect CONFIRMED) |
| 0x4460 | 17504 | float | L115 | ? session-time stamp (vs `dbl_8AFEA0`) |
| 0x4448 | 17480 | float | L119 | ? time margin |
| 0x45E8 | 17896 | float | L127 | ? scale |
| 0x3984 | 14724 | float | L127 | ? speed/fuel threshold (inspect) |
| 0x3988 | 14728 | float | L128, L129 | ? speed cap |
| 0x34D4 | 13524 | float | L211 | ? orientation component (v21 * ...) |
| 0x34C8 | 13512 | float | L211 | ? orientation component |
| 0x34BC | 13500 | float | L211 | ? orientation component |
| 0x34DC | 13532 | float | L212 | ? orientation component |
| 0x34D0 | 13520 | float | L212 | ? orientation component |
| 0x34C4 | 13508 | float | L212 | ? orientation component |
| 0x44B4 | 17588 | float | L213 | steering lock divisor (doc 54 live 0.4014) |
| 0x442C | 17452 | float | L214 | steering command (doc 54) |

Other-car reads (found via `dword_7C54F0[v3]`): `+16480` (L57, position-like index), then the same car's
`+13560..+13568` velocity components `v7[3390..3392]` (L89, same 0x34F8..0x3500 block as doc 54 step 1).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4434 | 17460 | float | L92, L101 | throttle candidate (0.0; cap 0.9) (doc 54) |
| 0x442C | 17452 | float | L121, L210, L217, L221, L237, L244, L248 | steering command (doc 54) |
| 0x4354 | 17236 | _DWORD | L134 | current waypoint ptr = 0 (doc 54) |
| 0x4358 | 17240 | _DWORD | L135 | look-ahead waypoint ptr = 0 (doc 54) |
| 0x4460 | 17504 | float | L136 | ? session-time stamp = `dbl_8AFEA0` |
| 0x4510 | 17680 | _DWORD | L137 | driverStatus = 5 (inspect CONFIRMED) |
| 0x4550 | 17744 | _DWORD | L138 | laneMode / waypoint-set selector = 1 (prior art) |
| 0x4538 | 17720 | float | L139 | maxSpeed = `flt_8ACFB4` (inspect CONFIRMED) |
| 0x45F4 | 17908 | _BYTE | L256 | ActiveFlag = 0 (inspect FACT) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/AISteeringRacingLineCorrection_0042b0e0.c`.

| value | lines | note |
|---|---|---|
| 3 | L51 | RacePhase compare (`dword_7C56A8`) |
| 1 | L103, L108 | RacePhase compare |
| -1 | L11, L214, L219, L241, L246 | return codes / clamps |
| 4120 | L54, L57 | car index field index |
| 16480 | L57 | other-car index field |
| 0.5 | L65, L67, L206, L209 | wrap half-length; midpoint scale |
| -200.0 | L74 | gap floor |
| 15.0 | L77, L84, L235 | speed clamp floor / angle limit |
| 5.0, 0.40000001 | L90 | session-time estimate |
| 0.2 | L89 | velocity magnitude gate |
| 0.89999998 | L100, L101 | throttle cap |
| 3754, 300 | L105, L106 | pointer index / field |
| 3688, 3690 | L112 | engine/RPM indices |
| 10.0 | L115 | time gap |
| 8.5 | L117 | speed-to-interrupt scale |
| 20.0 | L119 | timer scale |
| 1.5 | L127 | speed target scale |
| 4420, 4436 | L137, L138 | state indices |
| 30.0 | L103 | RacePhase 3 gate (`+30.0`) |
| 104, 2, 80.0, 35.0 | L162-L192 | nearest-node scan bounds |
| 3381, 3378, 3375, 3383, 3380, 3377 | L211, L212, L238, L239 | orientation indices |
| 4397 | L213, L240 | steer divisor |
| 40.0 | L227 | near-distance gate |
| 0.0 | many | resets / compares |

## Logic

NOTE: source uses scaled `this[N]` / `vN[N]` indexing (Hex-Rays prints element index for `float *`). Logic keeps
source-literal indices; byte offsets are in the tables above. No hex tokens are used on scaled lines.

```
if (RacePhase == 3) {                                                       [L51]
    v3 = 0; v26 = this[4120] - 1                                            [L53,L54]
    while (v3 < VehicleCount) {                                             [L55]
        if (other[v3][16480] == this[4120] - 1) break                        [L57-L60]
        ++v3
    }
LABEL_18:
    if (v3 != VehicleCount) {
        v36 = DistanceProjectionCalc(this + 3)                              [L62]
        v4 = DistanceProjectionCalc(other[v3] + 12) - v36                   [L63]
        wrap v4 into [-0.5*flt_5EA3FC, +0.5*flt_5EA3FC]                     [L64-L73]
        if (v4 > -200.0) {
            TrackParameterMinimumClamp(this, max(2*v4, 15.0))               [L74-L85]
            if (this[4368] < 1.0) {
                v7 = other[v3]
                if (|v7[3390..3392]| < 0.2 && ((v26+5.0)*0.4 + flt_8ACF94) > dbl_8AFEA0)
                    this[4365] = 0.0                                        [L86-L94]
            }
        }
    }
    if (v3 == VehicleCount) TrackParameterMinimumClamp(this, 49.0)          [L98,L99]
    if (this[4365] > 0.89999998) this[4365] = 0.89999998                    [L100,L101]
}
if (RacePhase == 1 || (RacePhase == 3 && flt_8ACF94 + 30.0 <= dbl_8AFEA0)) { [L103]
    v8 = this[3754]
    if (v8 && *(v8 + 300)) {                                                [L106]
        if (RacePhase == 1 && DistanceComparison(this + 3) && this[3690] > this[3688]) {  [L108-L113]
            v27 = dbl_8AFEA0
            if (|dbl_8AFEA0 - this[4376]| > 10.0) {                          [L115]
                v9 = this_int[1] * 8.5; InterruptDispatch(v9)                [L117,L118]
                if (v27 <= v9 + 1.0 + this[4370]*20.0 + flt_8ACF90) this[4363] = 0.0   [L119-L122]
                else {
                    v10 = RandomWeightedTierSelection(this); LoopWithConditionalSubCall(v10, 1)  [L125,L126]
                    SpeedValueSetterWithGlobalMirror(min(this[4474]*1.5 + this[3681], this[3682]))  [L127-L131]
                    VehicleClimbAdjust(0)                                    [L132]
                    this[4309] = 0.0; this[4310] = 0.0; this[4376] = dbl_8AFEA0   [L134-L136]
                    state[4420] = 5; state[4436] = 1; this[4430] = flt_8ACFB4    [L137-L139]
                }
            }
        }
    } else {
        SingleFunctionDispatch_10e0(*(_DWORD *)this, v42, v44)              [L148]
        F_3DTransformWithMatrixMultiplication(&v37, v47)                    [L149]
        v33..v35 = v42[] - v37..; v36 = |delta|                             [L150-L155]
        TrackParameterMinimumClamp(this, v36*0.40000001 + 3.0)              [L156,L157]
        if (byte_7CEC18) {
            scan up to min(VehicleCount+2, 104) nodes via sub_403CD0, nearest v25/v1  [L158-L181]
            if (v25 < 80.0) {
                TrackParameterMinimumClamp(this, max(v25 - 20.0, 35.0))     [L182-L193]
                if (v25 < 35.0) {
                    midpoint of nodes v1-1/v1-2 (clamped to 0) - v37..       [L194-L209]
                    this[4363] = atan2(-(v21*this[3381] + v31*this[3378] + v30*this[3375]),
                                        -(v21*this[3383] + v31*this[3380] + v30*this[3377])) / this[4397]   [L210-L213]
                    clamp this[4363] to [-1.0, 1.0]                          [L214-L222]
                }
            }
        }
        if (v36 < 40.0) {
            EulerToRotationMatrix(v44, v43)                                 [L229]
            v22 = -(v43[8]*v35 + v43[5]*v34 + v43[2]*v33)  (forward test)    [L230-L232]
            if (v22 >= 0.0) {
                if (v22 < 15.0) this[4363] = atan2(... as above) / this[4397], clamp  [L235-L249]
            } else {
                VehicleDynamicsStateReset(this + 3)                         [L254]
                AIVehiclePathPositionTracking(v42, v44)                     [L255]
                *(this + 17908) = 0                                         [L256]
                if (RacePhase == 1) ConditionalGate(this + 3)               [L257,L258]
            }
        }
    }
}
```

## Meaning (GUESS)

- GUESS Re-joins the racing line when the car is off it: in RacePhase 3 it finds the car with `+16480 ==
  this[4120]-1` (position order?), measures the track-projection gap between the two cars, wraps it by track
  length `flt_5EA3FC` and clamps a speed parameter from `2*gap` (floor 15) [L51-L99].
- GUESS If the searched car does not exist, raises the speed clamp to 49.0; caps the throttle candidate `this[4365]`
  (0x4434) at 0.9 [L98-L101].
- GUESS In RacePhase 1 (or late RacePhase 3) with a valid pointer 0x3AA8 (+0x12C non-zero) it can trigger a driver
  reset: interrupt dispatch, weighted tier re-selection, speed setter, climb adjust, waypoint/lane state reset
  (0x4354/0x4358/0x4510=5/0x4550=1/0x4538=pit speed) [L103-L145].
- GUESS Without that pointer it falls back to a track-node projection: gets the car pose, finds the nearest of up
  to min(VehicleCount+2,104) nodes, lowers the speed clamp from `nearest - 20` (floor 35), and when very close
  (<35) steers toward the midpoint of the two nodes behind the nearest one, writing steer 0x442C via `atan2` over
  the orientation block and dividing by 0x44B4 [L148-L249].
- GUESS If the pose delta is < 40 and the forward test is negative (target behind the car), it hard-resets vehicle
  dynamics and re-places the car with `AIVehiclePathPositionTracking` (CONFIRMED ECX = vehicle+12 via
  `S1-placement-native-abi`), clears ActiveFlag 0x45F4 and gates on RacePhase 1 [L227-L259].
- GUESS The steering sign is the same convention as the rest of AISteering: positive command turns toward
  +lateral (`aiw-lateral-sign-positive-is-right-2026-10-02`, citing L210-L221) [L210-L249].
- GUESS So this is a recovery/re-placement service: pace clamp by gap to the car ahead, node-projection steering,
  and a last-resort physics reset — not a normal per-tick steering term (the caller AISteeringCorrection runs after
  the steering law) [caller].

## Questions

- What are `flt_8ACF90`, `flt_8ACF94`, `flt_8ACFB4`, `dbl_8AFEA0`, `flt_5EA3FC` exactly (session timing, pit speed,
  track length)? The L74-L99 branch uses them as race timing.
- 0x3AA8 points to what object, and what is its `+0x12C` field? (inspect skeleton says opponent; unverified.)
- Two `atan2` sites use different displacement vectors: `v42/v44` (pose native out) vs midpoint of two track nodes.
  Which situation picks each?
- 0x39A0 EngineRPM vs 0x39A8 requested RPM: is the L112 test a downshift-prevention trigger?
- When the L252 branch resets dynamics, does the caller AISteeringCorrection overwrite its outputs (throttle 0x4434,
  steer 0x442C) later in the same tick?
