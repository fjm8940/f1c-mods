# H16 - HDV power, gearing, drag and downforce: 2001 Ferrari vs Jordan vs Minardi

Date: 2026-10-15. Method: static, read-only. Game data read as values only. Nothing in the game install, `findings.jsonl` or other corpus files was modified. This report is the only write.

Tags: FACT = read directly from file or decompile. DERIVED = computed or inferred from FACTs. OPEN = not established.

## 0. Cold start and limits

- Root `AGENTS.md`, `README.md`, `governance\README.md`, `PIPELINE.md` and `PRODUCT_STATUS.json` read. Product is F1C SDK / adaptive AI research; no stage change proposed.
- `python tools/f1c_mod.py start --json`: `freshness.stale = true` (newest checkpoint 2026-10-13). Checkpoint objectives not acted on (this is a research task).
- Prior art: `f1c_mod.py evidence-query findings "RPMTorque"` (1 hit) and `"gear ratio"` (12 hits) read. Used below.
- Workspace validator: run from `tools/workspace/check-workspace-structure.ps1`. Result: 2 errors, both pre-existing and unrelated to this task: unexpected top-level directories `.claude` and `plans`. Not fixed (read-only scope). Needs owner decision.
- Brief item: `sub_4F1510` is named `ScaleAudioPitch_004f1510` in the corpus. Its body is a physics rescale, not audio. The name is misleading; the function is the same one.
- Not written: a `findings.jsonl` entry (AGENTS.md requires one for proven results). Excluded by the read-only scope. Pending follow-up, see section 7.

## 1. Bottom line

- Ferrari and Jordan are near-identical on straight-line numbers (within about 1 percent on most keys). Jordan has slightly more torque at 13,000-16,000 rpm and higher engine inertia.
- Minardi is the outlier. Peak power is about 12.7 percent lower (559 vs 640 kW), it has 6 forward gears instead of 7, a higher body drag (+3.0 percent), a lower rev limit (16,400 vs 17,700 rpm), and lower wing lift magnitude.
- Mass is not a differentiator (601.7 to 601.9 kg). Tyres are a negligible factor: Michelin (Minardi) is about 0.2 percent lower in dry lateral grip than Bridgestone.
- AI: no AI-only override of mass, aero, power or gearing was found. The same HDV and engine parsers run for every car. AI differences are runtime (section 5).
- `sub_4F1510` touches engine power (bit 1), gearing (bit 2), fuel-weight limit (bit 4) and one ungated 4-entry block. It does not touch mass, drag, lift or the wing tables (section 6).

## 2. Key keys, sources and parser offsets

Source files: `F1 Challenge V10\SeasonData\Vehicles\<Team>\2001_*\` (`2001_Ferrari.hdv`, `2001_Jordan.hdv`, `2001_Minardi.hdv`, `engine.ini`, `.veh`). Tyre files: `SeasonData\Vehicles\2001_Bridgestone.tbc`, `2001_Michelin.tbc`.

Offsets are relative to the HDV parser object, not the 0x48C0 race struct (CONFIG_SCHEMA.md, `TheSvmHdvVehicleFileParser_004f4270`, line 343 onward). Pinned offsets are in parentheses.

| Physics role | Key (file, raw line in Ferrari HDV) | Parser offset | Consumer status |
|---|---|---|---|
| Mass | `[GENERAL] Mass` (line 39) | 0x40 | OPEN (not traced) |
| CG height | `[GENERAL] CGHeight` (46) | 0x60 | OPEN |
| Inertia (3 axes) | `[GENERAL] Inertia` | 0x48/0x50/0x58 | OPEN |
| Fuel range / setting | `[GENERAL] FuelRange`, `FuelSetting=64` (all 3) | n/a | OPEN (range units unknown) |
| Tyre brand | `[GENERAL] TireBrand` (59) | n/a | FACT (selects .tbc) |
| Front wing drag | `[FRONTWING] FWDragParams` (85) | 0x1F8/0x200/0x208 | OPEN formula |
| Front wing lift | `[FRONTWING] FWLiftParams` (86), `FWLiftHeight` | 0x210/0x218/0x220 | OPEN formula |
| Rear wing drag | `[REARWING] RWDragParams` (101) | 0x2FC/0x304/0x30C | OPEN formula |
| Rear wing lift | `[REARWING] RWLiftParams` (102) | see CONFIG_SCHEMA | OPEN formula |
| Body drag | `[BODYAERO] BodyDragBase` (114), `BodyDragHeightAvg/Diff` | 0x3F4 / 0x3FC / 0x404 | OPEN formula |
| Radiator and brake-duct drag | `[BODYAERO] RadiatorDrag`, `BrakeDuctDrag` (identical, all 3) | 0x518 / 0x528 | OPEN |
| Diffuser | `[DIFFUSER] DiffuserBase`, `DiffuserFrontHeight`, `DiffuserSideways` | 0x4D8-0x510 | OPEN |
| Driveline gears | `[DRIVELINE] ForwardGears` (249), `Gear1..7Setting` (250-256), `FinalDriveSetting` (247), `GearFile=gear_ratios.ini` | ForwardGears 0x13C4 | Ratio values OPEN (file not in install) |
| Clutch | `[DRIVELINE] ClutchTorque` (235), `ClutchFriction`, `ClutchInertia` | 0x137C / 0x138C / 0x1374 | FACT (loaded at 0x4E6CAB, finding 212) |
| Engine torque curve | `engine.ini RPMTorque=(rpm, coast, power)`, 43 rows (F, J) / 41 rows (M), lines 1-43 | EngineSectionParser | DERIVED: power column feeds player table (see 3) |
| Engine inertia | `engine.ini EngineInertia` (46 F/J, 44 M) | 0x684 | DERIVED: player object +7276 (finding player-engine-data-addresses) |
| Rev limit | `engine.ini RevLimitRange=(base,step,n)`, `RevLimitSetting=5` (51 F/J, 49 M) | via 4F3C10 | DERIVED: limit = base + 5 x step |
| Launch | `engine.ini LaunchRPMLogic`, `LaunchEfficiency` | 0x6A4 (efficiency) | FACT for AI use (engine.ini comment) |
| Fuel burn | `engine.ini FuelConsumption` (44 F/J, 42 M) | 0x674 | Two consumers, see 5c |
| Wear gate | `engine.ini` none; code gate `veh+0x32C8` | n/a | FACT: AI cars read 1 (docs/46 section 4.2) |
| Tyre dry grip | `.tbc` `[COMPOUND] DryLatLong`, `Radius`, `Width`, `LoadSens`, `SpringBase`, `Heating`, `WearRate` | n/a | FACT (values in section 3) |
| Global downforce | `sub_4F8440` (SuspensionForceComputation...004f8440.c) | n/a | FACT formula (below) |
| AI-prefixed HDV keys | `AIMinPassesPerTick`, `AIRotationThreshold`, `AIEvenSuspension`, `AISpringRate`, `AIDamperSlow/Fast`, `AIDownforceZArm`, `AIDownforceBias`, `AITorqueStab` (`[GENERAL]`, Ferrari HDV lines 71-80) | e.g. AIDownforceBias 0x124, AIDownforceZArm 0x11C | Readers not found (5a) |

Global downforce (FACT, `SuspensionForceComputationWithPerCornerVirtualDi_004f8440.c` around the `v14` assignment): downforce term = (speed^2 x `dword_5D67A8` + `dword_5D67AC` x v15 + `dword_5D67A4`) x `flt_7CF2F0`, gated by `sub_4C7BD0`. Constants are global, not per car. Same formula as docs/12-globals-reference.md:2693 (`AeroDownforceConstant`, "both"). Constant values not read in this pass (OPEN).

## 3. Three-car values

HDV values read directly with grep (cross-checked against a key diff). Power is computed as `T x rpm x 2pi/60`, with T assumed to be N m (DERIVED: docs/46 section B.1 uses "Nm"; magnitude is plausible). Unit comment not present in the file.

| Key | Ferrari 2001_F2001 | Jordan 2001_EJ11 | Minardi 2001_PS01 | Minardi vs Ferrari |
|---|---|---|---|---|
| Mass (kg) | 601.7 | 601.8 | 601.9 | +0.03% |
| CGHeight (m) | 0.2422 | 0.2450 | 0.2469 | +1.9% (higher CG) |
| Inertia (x, y, z) | 507.4 / 578.1 / 117.3 | 509.3 / 579.3 / 117.8 | 511.6 / 581.4 / 118.2 | +0.8% / +0.6% / +0.8% |
| CGRearSetting (range) | 4 of 8 (0.555 + 4 x 0.005 = 0.575, DERIVED) | 4 of 8 (0.575) | 3 of 7 (0.560 + 3 x 0.005 = 0.575) | equal bias |
| Peak torque (N m, power col.) | 371.7 at 15000 rpm | 375.8 at 15000 rpm | 348.6 at 14500 rpm | -6.2% |
| Torque at 16000 rpm | 370.0 | 374.5 | 331.8 | -10.3% |
| Peak power (kW, computed) | 640 at 17000 | 639 at 17000 | 559 at 15500 | -12.7% |
| Power at 16000 rpm (kW) | 620 | 627 | 556 | -10.3% |
| Rev limit (rpm, DERIVED: base + 5 x step) | 17200 -> 17700 | 17000 -> 17500 | 15900 -> 16400 | -7.3% |
| Power at rev limit (kW, DERIVED) | 624 | 618 | 534 | -14.4% |
| EngineInertia | 0.0509 | 0.0535 | 0.0525 | +3.1% (Minardi) |
| FuelConsumption | 3.009e-5 | 3.052e-5 | 3.090e-5 | +2.7% |
| LaunchRPMLogic (rpm) | 10008.9 / 13507.0 | 9505.5 / 13410.4 | 8930.4 / 12512.9 | lower |
| LaunchEfficiency | 0.969 | 0.971 | 0.963 | ~ |
| ForwardGears | 7 | 7 | 6 | -1 gear |
| Gear1..7Setting (index) | 7, 25, 37, 48, 57, 65, 69 | 7, 25, 37, 48, 57, 65, 69 | 7, 25, 38, 50, 60, 69 | 3-6 at higher index |
| FinalDriveSetting / ReverseSetting | 1 / 7 | 1 / 7 | 1 / 7 | equal |
| ClutchTorque | 1015.1 | 1012.4 | 1007.1 | -0.8% |
| BrakeTorque (front-left) | 3567.5 | 3522.5 | 3472.5 | -2.7% |
| BodyDragBase | 0.3811 | 0.3842 | 0.3924 | +3.0% (drag) |
| FWDragParams (a, b, c) | 0.0147, 0.00195, 0 | 0.0150, 0.00203, 0 | 0.0154, 0.00210, 0 | a +4.8% |
| RWDragParams (a, b, c) | 0.0627, 0.00733, 0 | 0.0630, 0.00737, 0 | 0.0637, 0.00744, 0 | a +1.6% |
| FWLiftParams (a, b, c) | -0.2659, -0.00947, 1.2e-5 | -0.2644, -0.00936, 1.2e-5 | -0.2624, -0.00930, 1.2e-5 | a magnitude -1.3% |
| RWLiftParams (a, b, c) | -0.1888, -0.01420, 4.6e-5 | -0.1862, -0.01408, 4.6e-5 | -0.1837, -0.01397, 4.6e-5 | a magnitude -2.7% |
| FWLiftHeight | 1.032 | 1.034 | 1.036 | ~ |
| DiffuserBase | -0.6952 (-7.0, 129.0) | -0.6897 | -0.6840 | magnitude -1.6% |
| TireBrand | 2001_Bridgestone | 2001_Bridgestone | 2001_Michelin | differs |
| TireCompoundSetting | 1 | 1 | 1 | equal |
| AI keys (section 2) | identical | identical | identical | none |

Notes:
- Lift is negative downforce (HDV header, FACT). "a magnitude" comparisons only; the full lift formula (and ride-height argument) is OPEN.
- Gear settings are indices into a shared ratio list (FACT: finding f1c-gear-ratio-lists-sort-descending). The list sorts descending, so higher index = numerically smaller ratio = taller gear (DERIVED). Minardi's gears 3-6 sit at higher indices, so they are taller. Its 6th gear (69) equals Ferrari's 7th. Actual ratio values are OPEN: `gear_ratios.ini` is not in the install tree (search over the whole game root returned nothing).
- Cross-check: a live ladder from finding 212 (player object, car not identified) reads forward ratios 16.39, 13.24, 10.85, 9.07, 7.76, 6.98 (6 entries). Not mapped to a team.
- Tyres (`.tbc`, `[COMPOUND]`, hard compound, FACT): dry lateral/longitudinal grip Bridgestone front (1.9076, 2.0960), Michelin front (1.9038, 2.0991), i.e. -0.2 percent lateral. Rear: Bridgestone (1.9702, 2.1813), Michelin (1.9662, 2.1846), i.e. -0.2 percent lateral. Radius 0.325, Width, SpringBase, LoadSens (-4.09e-5, 0.395, 13300), Heating and WearRate identical. Tyre brand is not a meaningful performance difference.
- Doc conflict: docs/46 section B.1 (line 201) describes a torque table with 44 rows, peak 375.0 Nm at 15500 rpm and rev limit 17400 + 5 x 100. The shipped `2001_F2001\engine.ini` has 43 rows, peak 371.7 at 15000 rpm, rev limit 17200 + 5 x 100. Do not use the docs/46 numbers for 2001 cars (stale or different car, OPEN which).

Car differences in one line each (DERIVED):
- Power: Minardi is the only car with a real power deficit (about -10 to -13 percent). Ferrari and Jordan are equal at peak power (639-640 kW) with Jordan slightly stronger at 13,000-16,000 rpm (+1.1 to +1.2 percent torque).
- Aero: Ferrari has the lowest drag and the largest lift magnitude; Minardi the highest drag and lowest magnitude. The differences are around 1-5 percent. Whether this matters for lap time depends on the unknown lift and drag formulas (OPEN).
- Mass: not a difference (0.03 percent).
- Tyres: not a difference (about 0.2 percent).
- Gearing: Minardi has one fewer gear; the rest is ordering-based (DERIVED).

## 4. What sets acceleration and downforce (mechanism)

- Acceleration: engine torque (engine.ini, lines 1-43) x gear ratio (index into the shared list, values OPEN) x final drive (index, value OPEN) / tyre radius (0.325 m) -> wheel force, limited by tyre grip (DryLatLong / LoadSens) and by drag. Mass sets inertia and the load term. Launch uses LaunchRPMLogic even without launch control (engine.ini comment, FACT).
- Downforce: global speed^2 term (section 2) plus per-car wing and body lift tables (FWLiftParams, RWLiftParams, BodyFore, DiffuserBase). Downforce feeds tyre load through the load/grip path (OPEN quantitatively; `TireBrushModelForceSolver` is called at the end of `sub_4F1510`, line 69, so the tyre solver is re-run after the scale).
- Drag: wing, body and radiator/duct drag tuples. The drag equation is not recovered here (OPEN).
- Fuel: 64 in all three, so the fuel setting is equal. Fuel weight is a mass-side term whose scalar is touched by bit 4 of `sub_4F1510` (section 6).

## 5. AI vs player

a. FACT: one HDV per car, parsed by the same routine for every vehicle. Call sites: `functions/TireModelInit_0043e050.c:37-38`, `functions/LargeConfigFunctionWithPhysicsConstants_004e45b0.c:37-38`, `functions/ValidateVehicleConfigurationFile_00433ed0.c:21-24`. No AI branch in these call sites. The `mode` argument (0/1/2) meanings are OPEN.
- FACT: the AI-prefixed HDV keys have no decompiled reader by name (grep of `functions/*.c` for AIDownforce, AISpringRate, AIDamperSlow, AITorqueStab, AIMinPasses returned nothing). Their effect is OPEN. Their values are identical across the three cars.
- FACT: `[ENGINE]` in the HDV holds only `Normal=Engine` and `RestrictorPlate=Engine`. Power data is in each team's `engine.ini`, so there is no per-car engine override inside the HDV.

b. FACT (H2 section 6; H6): `sub_4F1510` is called from `AIDriverStructInit` for each AI car at init, outside the `LOBYTE(a2)` guard. Whether the player car goes through it is OPEN.

c. FACT/DERIVED: fuel burn has two consumers. (1) `FuelConsumptionAndWear` (0x004ED960) runs only for cars with `veh+0x32C8 == 1`, which are the AI cars (docs/46 section 4.2, line 136). (2) The physics fuel model (player object `this+428`, coefficient, DERIVED from finding player-engine-data-addresses). Which applies to which car is partly OPEN.

d. FACT (docs/46 section 4.3): Model A failure countdown runs for AI cars only.

e. FACT (H2 section 3c): the AI fuel multiplier `flt_7D01E8` (default 0.95) is applied to `this+14724` at vehicle init. OPEN whether this applies to the player.

Verdict: no AI-only HDV override of mass, drag, lift, power or gearing was found. AI differs at runtime through the strength scale (b), fuel multiplier (e) and AI-gated wear and fuel paths (c, d).

## 6. What sub_4F1510 (ScaleAudioPitch) touches

Source: `functions/ScaleAudioPitch_004f1510.c`. `this` is the vehicle physics struct. The AI curve value is `a2`.

- Scale: `v14 = a2 / this[15348]` (line 17). Baseline stored at line 68 (`this[15348] = a2`). Baseline paths in H2 section 3b (OPEN which runs last).
- Ungated (lines 17-27): the 4-entry block at `this+13852`, stride 248 bytes, each multiplied by `v14` regardless of the bitfield (FACT, H2 section 5). Identity OPEN; the 248-byte stride suggests a per-wheel block (DERIVED, not established).
- Bit 1 (line 28, `byte_7D01C4 & 1`): block at `this+14756` (pointer), count `this+14768`. Entries are (coast, power-coast) pairs, the same layout as the player table at `+7420` (finding player-engine-data-addresses). The loop (lines 31-36) rewrites only the second element as `(c + inc) x v14 - c`. Net effect: full-throttle torque is scaled by `v14`, coast is untouched (DERIVED from the code). Identity with the player table is DERIVED by layout, not proven (OPEN). The brief's "torque table" label is consistent with this.
- Bit 2 (line 38, `& 2`): block at `this+14800`, count `this+14840`. Entries are multiplied by `1/v14`. The 9th element uses stride 918 floats (line 52). Label "gearing" is DERIVED: the gear table at `veh+0x39D8` (finding 212, CONFIRMED) is in the same neighbourhood, but base equality is OPEN. At strength 100 (baseline 1.0, Mid 1.0), v14 = 0.995, so ratios x1.005 (DERIVED, from H2 section 5).
- Bit 4 (line 63, `& 4`): `this+14724 *= v14`. Consumers: `functions/FuelWeightLimitCheck_0042ad50.c:4` and `functions/FuelWeightComputation_0049ba50.c:7` (FACT). This is the fuel-weight limit scalar, not the fuel burn coefficient (`this+428`), which sub_4F1510 does not touch. The brief's "fuel phys+14724" label is correct for the weight limit, not for burn rate.
- Not touched by the code shown: mass, CG, inertia, drag and lift tables, wing and diffuser tables. Caveat: `sub_5881F0` tail-calls `sub_588212` (not analysed, OPEN), and `TireBrushModelForceSolver(this,0)` runs after the scale (line 69), so tyre forces can change indirectly.

Answer to question 4: the scale touches engine power (bit 1, torque only), gear ratios (bit 2, DERIVED), the fuel-weight limit (bit 4) and an ungated 4-entry block (identity OPEN). It touches none of the mass, drag or downforce values in section 3.

## 7. Open items and follow-ups

1. Find `gear_ratios.ini` (GearFile) and the final-drive list. Without these, per-car top speed and acceleration cannot be computed. Read-only runtime read of the shared list is the route.
2. Recover the drag and lift formulas (FWDrag/RWDrag/BodyDrag a, b, c; FWLift/RWLift; ride-height argument FWLiftHeight). Read the `sub_4F8440` constants (`dword_5D67A4/A8/AC`, `flt_7CF2F0`) and gate `sub_4C7BD0`.
3. Name the blocks at `this+14756` (bit 1), `this+14800` (bit 2, check base vs `veh+0x39D8`) and `this+13852` (ungated).
4. Confirm the torque column unit (N m) with one runtime read.
5. Reconcile docs/46 section B.1 with the shipped Ferrari engine.ini (44 vs 43 rows, 375.0 vs 371.7 Nm, 17400 vs 17200). Flag as stale or different car.
6. Per-track `.svm` setups (FWSetting, RWSetting, Gear1..7, FuelSetting, RevLimitSetting) set wings and gears per track. Which setup the AI uses, and how `FWSetting` indexes the wing tables, are OPEN. Not inspected in depth (Australia.race.svm read only).
7. Confirm whether `sub_4F1510` runs for the player car.
8. Second Jordan car (`2001_jordan12.veh`) points to the same HDV (FACT); not separately compared.
9. Append proven results to `f1c_ida_decompiled\findings.jsonl` (AGENTS.md): pending owner go-ahead, since this task was read-only apart from this file.
10. Workspace validator: 2 pre-existing errors (`.claude`, `plans` top-level dirs). Owner decision.

## 8. Changed files and verification

- Changed: this report only. No game files, no findings, no state.
- Verification: HDV values read by grep, cross-checked with a sectioned key diff across the three files. Power values computed from the RPMTorque table with awk; rev-limit power interpolated linearly between rows. Gear and drag values are index or raw parameters; no physics re-derivation.
- Limitations: static read only; no live run; formulas OPEN as above; DERIVED items are labelled and need the runtime checks in section 7.
