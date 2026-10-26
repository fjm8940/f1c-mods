# RANDOMNESS — D5: RandLCG census, RandomVariationScaler correction, mistake paths (BATCH B item 3)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d5_random.py`
(`skipdata`), every direct `call 0x5884ED`; containing function from `functions/*.c` starts;
wrapper callers likewise. No live game use.

## 1. Census summary

59 direct `RandLCGRandomNumberGenerator` (0x5884ED) call sites in the exe. **25 are AI-relevant**
(the mapped AI set + the controls below); 34 are sound/render/particle/cloud/telemetry/CRT/file
helpers (not AI). Wrapper callers:

| wrapper | callers (containing function) |
|---|---|
| `RandomVariationScaler 0x4BEA10` | `EngineFailureCheck` 0x4BECB0, `AIBehaviorInitialization` 0x4BF1FF |
| `RandomWeightedTierSelection 0x429FD0` | `AIMainUpdate` 0x425603, `sub_429960` 0x429E89, `AISteeringRacingLineCorrection` 0x42B386 |
| `AIPathPerformanceFunction 0x4BFA00` | `AIOpponentSelection` 0x4BF7E7 |
| `AIOpponentSelection 0x4BF790` | `AIDriverCountSetup` 0x4BFE80, `GameFilterDataLoader` 0x4C0EBD |

## 2. AI-relevant call sites

| function | call VA | C line | randomized value / distribution | cadence | target field | gameplay effect | tier |
|---|---|---|---|---|---|---|---|
| `AIMainUpdate` | 0x424F74 | L80 | `rand % 4` → start-state code {7,3,8,8} | start phase only | 0x4068 `reason_code` if unset | start-stall/recovery variant marker | DERIVED |
| `AIMainUpdate` | 0x424FA9 | L89 | `rand & 0x14` (+ db + 10.0) | start phase only | 0x4170 | start-phase timer jitter | DERIVED |
| `AIMainUpdate` | 0x424FA9/0x42517B | L123/L125 | `rand % 100 * 0.02 or 0.01` (+ time) | start phase, alternating arms | 0x4564 | launch reaction-time jitter (D7) | DERIVED |
| `AIMainUpdate` | 0x425293 | L202 | `rand % (lap/fuel budget)` per lap entry | per matching lap | 0x416C + engine-life slots 0x3D70 (5 slots) | AI engine-failure scripting (→ `EngineFailureCheck`/`AICollision` incident codes) | DERIVED |
| `AIAbility` 0x429470 | 0x42958B | L54 | `rand % 100` vs `(0x44D4 - 0x450C*0.5)*100` | every 4 s in race | 0x453C pace | random direction of the traffic pace gate (D4) | OBSERVED (D4) |
| `sub_424080` (.rcd/default init) | 0x42411B | L62 | `rand % 3` | once per driver init | 0x44FC `unnamed_44fc` | per-driver random variant (GUESS: style/compound seed) | DERIVED |
| `sub_424080` | 0x424480 | L165 | `rand % 200 * 0.01` added to STARTSDRY/WET blend | once | 0x44B8 `throttle_cap_race_start` | start throttle cap jitter 0..2.0 before `*0.1+1.0` | DERIVED |
| `sub_424080` | 0x4246EF | L182 | `rand % 10000 < StartStalls*100` | once | 0x4144/0x4148 start-state pair | start-stall scripted event | DERIVED |
| `sub_424080` | 0x42470F | L184 | `rand % 10` | once (on stall hit) | 0x4148 state (5 or 4) | stall vs stall-DNS variant | DERIVED |
| `sub_429960` (pit strategy) | 0x429CA2 | L177 | `rand % 2 + 3` | once per strategy build | `this[4450]` pit plan count | 3/4 scheduled stops (sessions 0/1/2/6) | DERIVED |
| `sub_429960` | 0x429D5F | L186 | `rand % 5 + 3` cumulative | once | per-stop lap offsets | random stop laps | DERIVED |
| `sub_429960` | 0x429D93 | L212 | `rand % 21 + 3` | once | `this[4450]` | 3/4+ scheduled stops (sessions 3/4/5) | DERIVED |
| `RandomWeightedTierSelection` | 0x42A007/A055/A0A1 | L17/L41/L50 | `rand & 1` / `rand % 2` tie-breaks inside wetness bands | per tyre-choice call | tyre compound tier | wet/dry compound selection (4/5/6) | DERIVED |
| `CarCrashDamageProcessing` | 0x49607C/A9/179 | L24/L28/L65 | `rand % 2`, `rand % 8`, `rand % 3` | per damage event | sub-object +5496/5504/5508 | crash damage particle/part variant | DERIVED |
| `RandomVariationScaler` | 0x4BEA38/B0 | L17/L28 | `rand % 40`, `rand % 50` | per call | return value | per-driver variation factor (see section 3) | DERIVED |
| `AIOpponentSelection` | 0x4BF816 | L43 | `2*rand % N` | once per opponent build | sim lap timing fields | simulated-lap randomization | DERIVED |
| `AIPathPerformanceFunction` | 0x4BFAF0/0x4BFC44 | L100/L195 | `2*rand % N`, `rand % 2` | per call | path performance values | sim performance variation | DERIVED |
| `FuelConsumptionAndWear` | 0x4EE0D7 | L267 | `rand % 4` | on a failure branch | wheel/failure slot +3929 array | random wheel mechanical issue (reason 4) | DERIVED |
| `PitExit` | 0x41C20F | L100 | `rand % 2` | per pit exit | pit-exit variant id | alternates pit-exit roll-out variant | GUESS |

Not AI-relevant (34): sound (`SoundEventDispatcher`, `SoundEffectTrigger*`, `ScaleAudioPitch`,
`RandomParameterDispatch`), particles/clouds, `FileScanner`, `TelemetryDataDump`, `FileScanner`,
`SingletonInit`, CRT/`ShortComputation`, `RandomPasswordKeyGenerator`, `SuspensionAndWheelPhysics`
(1 site, physical noise), etc.

## 3. REVIEW_15 answer — RandomVariationScaler corrected formula


```
004BEA9D fld  dword [esp+8]      ; v8  (clamped 0..1)
004BEAA1 fld  qword [0x5acdd0]   ; 1.334  (double)
004BEAA7 call 0x5881F0
004BEAAC fstp dword [esp+8]      ; <-- the x87 return IS stored back into v8
```

- `sub_5881F0` is the CRT `pow` helper (25-byte public wrapper over the `_CIpow` core
  `sub_588212`): `fxch st(1)` / `fstp` + `fst` of the two inputs, `fyl2x`, `fscale`, `f2xm1`
  family, with the standard 0^y / y==0 / negative-base edge cases. The helper convention is
  **entry st0 = base x, entry st1 = exponent y** (the value it decomposes and zero-tests is the
  one stored last by the caller); the caller-side mirror is `RandomOffsetCalculator 0x4F6850`
  (`fld a2; fld obj+0x580; call` = `pow(objConst, a2)` used as a configurable curve).
- Hex-Rays shows `sub_5881F0(v8);` as a standalone statement and keeps `v8` unchanged; that is
  wrong. The corrected model:

```
v8 = clamp((rand%40)*0.01 - 0.01 + 1.0 - ability_norm, 0.0, 1.0)
v8 = pow(1.334, v8)                      # not pow(v8, 1.334); base = last-loaded operand
v5 = min(1.0, (rand%50)*0.01 + 0.5)
return 0.25*a4*(v5*v8) - 0.25 + a4
```

- Same class as DROPPED_ST0_SCAN, but here the store (`fstp [esp+8]`) is visible right after the
  call, so the fix is unambiguous. `EngineFailureCheck` (0x4BECB0) uses this to seed the AI
  engine-failure timer; `AIBehaviorInitialization` (0x4BF1FF) uses it for simulated-lap
  performance (IR L1969-1977).

## 4. Mistake paths — does the AI make deliberate random mistakes?

- **Green-flag driving functions have no RandLCG call at all**: `AIRacingLine`, `AISteering`,
  `sub_426100` (steering law), `AISteeringCorrection`, `AIRacingLinePathFollowing`,
  `PhysicsGearShiftLogicWithThresholds`, `AIOvertaking`, `Overtaking` — zero direct or wrapper
  random calls. This matches the deterministic replays: D1 Monza/wet 100 % within ±2 %,
  D11 wet step replay median 1.000 — the planner is a pure function of logged state.
- Random **events** are real and configurable: `.rcd` tokens `StartStalls` (0x44BC), `Crash`
  (0x44D0), `CompletedLaps%` (0x44DC) — both consumed by `AICollision 0x4BEF60` L30/L37 with the
  precomputed random table `flt_8AAE74[dword_8AAE70++]` (table filled by
  `RandomNoiseTableInitialization` at init), `RaceEvents` (scripted ENGINE/BRAKES/GEARBOX/
  SUSPENSION/STALL/STALLDNS via the 0x4144/0x4148 pairs and engine-life slots), plus random pace
  gate, pit plan, tyre tier, start reaction and damage variants (table above).
- No "random lift / late brake / line error" path was found. `AISteeringCorrection`'s slip cut and
  the braking profile are deterministic; the only per-tick randomness reaching AI control is the
  4 s pace gate (D4) and the start-phase timers.
- **VERDICT: deliberate random *events* (starts, pace drift, strategy, incident scripting,
  damage), yes; deliberate random *driving mistakes* in normal running, no evidence (effectively
  no).** All random paths are `.rcd`-configurable or event-scripted, not noise added to the
  racing line.

## Limitations

- Tier DERIVED = static code path + field mapping; no dedicated live trace was run for the event
  paths (offline-only batch). The D4 live replay covers the pace-gate random site.
- `RandomWeightedTierSelection` band semantics (flt_8AFEE8 = wetness) are from the code constants
  0.15/0.2/0.5/0.55/0.8/0.85; compound names not resolved here (D8 material).
- 0x44FC and 0x4170 meanings remain GUESS; they are assigned random values but their consumers are
  not in the mapped set (registry note only).
