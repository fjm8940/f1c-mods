# Engine faults: thresholds, messages, levels (owner reference, 2026-10-05)

Candidate: `CD4C3FC3` (AUDITFIX X1-X5), built twice byte-identically in TEMP; **not installed**.
Dev DLL remains `73F30551`. Prior installed DLL `73F30551` and engineer SDT `226E5402` are the
2026-10-05 handoff state. Source: `dll/engine_failure.c`, `dll/mf_engine_model.h`, `dll/mf_msg.h`,
`dll/mf_voice.h`; batch evidence in `ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md` section 8.

Evidence rule: **all thresholds, probabilities and delay ranges below are harness only** (source
rules and offline checks), unless a paragraph explicitly says "live" and names the recorded context.
Harness only does not prove a native game's exact timing or presentation. F2002 examples are
illustrative; the recorded owner water-leak pass used a 1996 car at Indy, practice. The repaired
candidate has had no live pass. Historical observations describe the prior installed build.

## How every fault talks (the levels)

| Level | What you get | Rule |
|---|---|---|
| 1 Diagnosis | dash text + engineer line + wheel lamp/HUD icon | condition must HOLD 1-3 s before it shows (some longer, listed) |
| 2 Instruction | HUD and speech have separate held/delayed schedules | advice fires only while its fault and narrated condition remain valid |
| 3 Critical | "ENGINE CRITICAL" / "PIT AND RETIRE" + critical line | engine dies soon unless you box |
| 4 Death | "ENGINE FAILURE" / "STOP THE CAR" + death line | practice/qual: DEAD01/02; race blow-up: vanilla RETIRE04 |

Radio timing (harness only): diagnosis/paired instruction/critical budgets limit ordinary calls;
normal dispatch gap is 15 s and death-priority gap is 5 s. Gasket death can queue GASKET01 plus
DEAD01/02; instruction-only coolant/oil calls also exist, so there is no universal one-line-per-level rule.
HUD instructions can hold 6-14 s (water: 8-14 s). Paired speech waits 30-60 s from its diagnosis
**request**, not confirmed audible playback. Queue contention can delay/drop a request; no playback
acknowledgement is available here. At the due time invalid advice is cancelled, not deferred. Repair,
clearance, stabilisation and stint end cancel timers and queued owned advice. Terminal engine state
immediately purges all non-death calls, including already queued ones.
The lamp stays on while any engine warning is on, and after a mod-killed engine until repair / stint end.
Manual panel buttons develop over a 300 s budget at race pace (driving harder can shorten it to ~200 s,
never more than 1.5x faster).

## Reference temperatures (harness only; F2002 illustration)

| | Normal | Warning |
|---|---|---|
| Water | ~77 C illustration; learned per car with a 50 C floor | WATER TEMP at max(normal + 12 C, **90 C**); COOL THE ENGINE at max(normal + 18 C, **95 C**) |
| Oil | ~95 C illustration (thermostat = optimum - 10 C) | OIL TEMP requires BOTH above optimum + 5 C AND above learned oil normal + 8 C |

Unknown cooling normal at onset uses max(current,85 C); a sub-85 C partially learned normal
already exceeded by the warning margin at >=85 C is conservatively reanchored to current. Unknown oil
normal captures onset oil and is bounded by car optimum. These fallback rules are harness only;
very low-normal cars and already-hot onsets need owner validation.

**So: 79-80 C water never warns any more.** The 79 C "WATER TEMP" seen on 2026-10-05 came from the low-coolant
rule, which now shows **COOLANT LOW** instead. A healthy car (warm or cold pit-out, 15 min) gives zero
warnings in the test harness.

## Faults

### Oil pressure (pump / bearing film)
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | gauge < 60 % of healthy OR bearing film breaking (gauge OR film; harness only) | OIL PRESSURE | "Oil pressure dropping." | engine |
| 2 | 30-60 s from diagnosis request; oil warning must still hold | SHORT SHIFT | "Oil pressure low, short shift." or "Low oil pressure, keep the revs down." | engine |
| 3 | bearings < 30 % | ENGINE CRITICAL | "Bearings are going, box now." | engine |
| 4 | bearings gone | ENGINE FAILURE | DEAD01/02 (race: RETIRE04 if it blows) | engine |
Harness only, current corrected gauge OR film rule: first oil-pressure condition about 15-18 s,
then 1-3 s HUD hold; terminal about 181-183 s. The earlier ~1 min claim described the
gauge-only crossing, not the now-eligible film warning. See FAULT_EXPECTATIONS_AND_VANILLA_OVERHEAT_2026-10-05.md.

### Oil leak
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | blue smoke visible (>= 2 % oil lost) -> marshals 5-20 s later | (flag) | vanilla marshals line, then "You're leaking oil." 15-20 s later | - |
| 2 | 30-60 s later | - | "Oil leak, take it easy." | - |
| 1b | oil hot | OIL TEMP + COOL THE OIL | "Oil temperature high, back off." | engine |
| 1c | pickup starving (gauge < 60 %) or film breaking | OIL PRESSURE | - | engine |
| 3 | sump < 25 % or bearings < 30 % | ENGINE CRITICAL | "Engine critical, box now." / bearings line | engine |
| 4 | seizure | ENGINE FAILURE | DEAD01/02 | engine |
Harness: death ~3-4 min. Note: no lamp until oil temp/pressure actually move (the smoke + marshals come first).

### Water leak
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | 1.0 L coolant lost (of 8 L), held 1-3 s then delayed 30-60 s | - | COOLANT01: "You're losing coolant." | - |
| 1b | water >= max(normal + 12 C, 90 C) | WATER TEMP | "Water temperature rising." | engine |
| 2 | water >= max(normal + 18 C, 95 C) | COOL THE ENGINE | "Water temp high, lift and coast." | engine |
| 2b | coolant < 20 % (1.6 L left) | COOLANT LOW + steam | - | engine |
| 3 | coolant < 20 % or head gasket nearly gone | PIT AND RETIRE | "Coolant low, box this lap." | engine |
| 4 | head gasket | ENGINE FAILURE | "Head gasket's gone." then DEAD01/02 | engine |
Hot-escape steam requires escaping coolant AND water above normal + 10 C AND at least 88 C.
Low-coolant/dry steam is a separate exception. All thresholds are harness only.
Recorded live, prior 73F30551 build, 1996 car / Indy / practice, 2026-10-05: water leak ended
in death at about 4.5 min (HANDOFF section 3); observed leak about 2 L/min at race pace.
Harness only: dry/gasket about 4-5 min. Candidate has not repeated this live pass.

### Radiator blocked (debris)
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | model WATER TEMP threshold, held 8-15 s + 3 s for diagnosis request | RADIATOR BLOCKED, BOX - DEBRIS | RAD01: "Debris in the radiator." | engine |
| 2 | 30-60 s from RAD01 request, only with active debris | - | RAD02: "Radiator blocked, find clean air." | engine |
| end | debris blows off (random, more likely at speed) or pit | RADIATOR CLEARED | "Radiator's clear now." | off |
Blockage: small 70 % / medium 85 % / big 95 % of the airflow (harness only). WATER01 is reserved
for water leak; debris uses RAD01 and cannot consume the leak diagnosis. RAD03 clearance cancels advice.

### Valve air (pneumatic valves)
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | valve return pressure < 70 % | LOSING REVS | "Losing valve pressure." | engine |
| 2 | actual valve float above the lowered ceiling; spoken pair also due | SHIFT NOW / SHIFT EARLIER | VALVE02 only while float still holds | engine |
| 3 | valvetrain nearly gone | ENGINE CRITICAL | "Engine critical, box now." | engine |
| 4 | valve failure | ENGINE FAILURE | DEAD01/02 / RETIRE04 | engine |
Harness: warning ~2 min (later than the 20-120 s target - DeepSeek reported, not retuned), death ~3 min.

### Over-rev
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | revs > 104 % of the limit (2 s grace after a mod lowers the limit) | OVER-REV | "Over-rev, watch the downshifts." (max once per 2 min) | - |
| 2 | an over-rev that measurably damaged the rods/valves | - | "That over-rev hurt the engine." | - |
| 4 | above this car's torque-zero RPM for > 0.3 s (fallback if unavailable; not a universal 115 %) | ENGINE FAILURE (blow-up, fire) | DEAD01/02 / RETIRE04 | engine |
Prior session reports a live blow-up chain (HANDOFF); the per-car threshold and candidate
behaviour above are harness only until an owner pass records car and RPM.

### Exhaust crack
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | HUD: first pop + 3-8 s; voice: 3 pops + 5-20 s | EXHAUST | EXH01: "Exhaust is cracked." | engine |
| 2 | 30-60 s later / rear heat | WATCH THE REAR | "Exhaust problem, keep pushing." | engine |
Renamed from EXHAUST TEMP: the model has no exhaust temperature rise for a crack.

### Blow-by (piston rings)
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | blow-by index > 0.3 held 45-90 s | ENGINE PROBLEM | "Engine's losing compression." | engine |
| 2 | power loss | EASE OFF (HUD only) | - | engine |
| 3 | rings < 30 % | ENGINE CRITICAL | "Engine critical, box now." | engine |
| 4 | rings < 15 % | ENGINE FAILURE | DEAD01/02 | engine |

### Fuel pressure
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | fuel delivery < 90 %, held 1.5-3 s | FUEL PRESSURE | "Fuel pressure dropping." | fuel |
| 2 | 30-60 s later | - | "Fuel pressure low, keep it smooth." | fuel |
| 3/4 | starvation | FUEL STARVATION / ENGINE FAILURE | "Engine critical, box now." / DEAD | - |
Harness: warning ~1-1.5 min, end ~7-10 min.

### Fuel leak
| Level | Trigger | Dash | Voice | Lamp |
|---|---|---|---|---|
| 1 | >= 0.1 L actually lost, then held 20-40 s | FUEL LEAK | "You've got a fuel leak." | fuel |
| 3 | critical | BOX FOR FUEL | "Fuel leak, box now." | fuel |
| 4 | fire | ENGINE FAILURE | DEAD01/02 | engine |
Harness only: warning ~4 min at medium race pace (was ~18 min with the old 0.5 L gate); fire ~30 min.
Empty fuel can stop the engine natively before a critical/fire call; practice has no race-distance target.
These stages do not guarantee a warning before every possible native end.

### Impacts (player)
| Impulse (game's own) | Chance of an engine fault | Severity | Voice |
|---|---|---|---|
| < 2000 (taps, brushes ~50 km/h side) | 0 % | - | - |
| 2000-5000 | 0 -> 25 % | small | HIT01: "Big hit, checking the data." if probe rolls no fault; no extra 5000 gate |
| 5000-8000 (real crash, wing off ~5500) | 25 -> 70 % | medium | fault: "Damage after that hit, watch the temps." |
| > 8000 (nose-on 60-110 km/h) | 70 -> 85 % | big | same |
Fault type when it happens: water leak 47 % / exhaust 29 % / oil leak 24 %. Pit returns never count.

## Things that may still look odd (for the owner's pass)
1. Oil leak: no lamp in the first phase (smoke + marshals + voice only) - intended "nobody sees it on the dash yet".
2. Valve warning ~2 min after onset (harness) - slower than the other faults.
3. Fuel leak warning ~4 min, fire ~30 min - a slow fault by design.
4. COOLANT01 follows 1 L loss + its hold and instruction delay; its order relative to HUD/other speech is not guaranteed.
5. HUD-only texts: EASE OFF, SAVE FUEL, ENGINE SAVE OFF.

Candidate verification: model 67631, msg 820, voice 406, lamps 212/0, AI pulloff 81/0,
AI react 40, patch8 132/0; thread pause test PASS; tool safety 5/5; audit regression 47/0.
These are harness only. Native lamp ABI, cold/hot-arm gameplay, repaired radio order, queue
contention, multi-mod compatibility and audio acceptance remain pending human validation.
