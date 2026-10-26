# H23 - Simulated qualifying (skipped session): AI lap formula and a shift write (static)

Date: 2026-10-15. Agent: Haiku 5.5 (static, read-only). Owner question: how the skipped-session
AI grid and lap time are computed, and the minimal mod write to shift simulated AI times by X %.
No game touched; only write: this file. Paths relative to `f1c_ida_decompiled\`.
Tags: FACT = read at cited line; DERIVED = inferred from FACT; OPEN = not settled. Corrects H9 (C1).

## 0. Bottom line

- FACT: the simulated lap is ONE draw per car, `B + 0.25*B*x'*y - 0.25` (RVS L31). The base B is the GDB
  "Qualify Laptime" (sessions 3-5) or the live-data estimate. Ability enters only through x.
- FACT: AI strength (slider dword_7D01A0) enters the sim only as a fallback factor 1+0.15*(1-s/120)
  on B (sub_4BE760 L73-76). Max effect 6.25 % at s=70, 0 at s=120. It is not in the live branch and not
  in the per-lap draw.
- DERIVED: the simplest shift write is a multiplier on B, applied AFTER sub_4BE760 returns (B is
  overwritten inside it). Scaling B by (1+X) moves every simulated lap by about X % (section 4).

## 1. Call chain and gates (FACT)

| Step | Where | What |
|---|---|---|
| Reset | functions/PerformanceSetupStructInitializer_004be700.c:8 (body of sub_4BE700) | brain+834 := 0 |
| Caller of reset | functions/Delegation_e6f0_004be6f0.c:3 | calls the reset |
| Sim driver | functions/sub_46BF10_0046bf10.c:24 | `while (AIOvertake(...))` loop |
| Gate | functions/AIOvertake_004beb10.c:32-33 | `if (!brain+834) AIBehaviorInitialization(...)` |
| Init | functions/AIBehaviorInitialization_004bf0a0.c:35-36 | a4 = dword_7D01A0/120; call sub_4BE760(this, brain+864, entry, a4) |
| Base B | functions/sub_4BE760_004be760.c:19-20, 64-76 | B default 600 s; GDB key; strength term |
| Sets flag | functions/sub_4BE760_004be760.c:75 and :83 | brain+834 := 1 (both branches) |
| Draw | functions/AIBehaviorInitialization_004bf0a0.c:89 | RandomVariationScaler(this, v7, car, brain+844) |
| Store | functions/AIBehaviorInitialization_004bf0a0.c:93-103 | laps+1, lap clock, best lap 16540 = min |

Session type: brain+864 = session (AIOvertake_004beb10.c:14). Session 7 = race.

## 2. The formula (FACT unless marked)

### 2a. Base B (sub_4BE760)

- L20: B := 600.0 (default).
- L22-60 live branch: F = fastest car best lap (field 16540, cars with state 13000 == 1, >1 s, L30-32);
  T2 = slowest car within 1.15 F (L47-52). Note: the fastest is picked FIRST (L25-42), then slowest (L43-60).
  Needs two distinct cars and F < 9000 (L41, L61). A1, A2 = ability of F and T2 car (L33-36, L53-56).
- L80-89 live result: B = max(0.97 F, F - (T2-F)*(1-A1)/max(A1-A2, 0.01)).
  Strength is NOT used on this branch. (FACT)
- L62-77 fallback (no usable live laps): if the session is not 0-2, 6 and track entry exists, B = GDB value
  (L66-70): sessions 3-5 use key "Qualify Laptime", others "Race Laptime" (L66).
- L73-76: a4 stored at brain+840; B := B*(1 + 0.15000001*(1 - a4)). a4 = s/120 (AIBehaviorInitialization L35).
  Factor: 1.0625 at s=70, 1.0 at s=120, 0.9875 at s=130 (no clamp). Step 0.125 % per slider point.

### 2b. Per-car draw (RandomVariationScaler 0x4BEA10)

- L8-16 ability A: brain+864 == 7 ? car+17604 : car+17600 (FACT). Transform (findings.jsonl:41):
  0x44C0 = 1 - QualifyingAbility/22 ; 0x44C4 = 0.8464 - 0.038473*RaceAbility.
  So sessions 3-5 use QA; session 7 uses RA.
- L17: i = rand%40; x = clamp(0.99 + 0.01*i - A, 0, 1). Higher A = smaller x = faster lap.
- L27: x' = pow(x, 1.334) or 1.334^x. The decompile shows the call without a store; the asm
  (H11 section 49) is `fld v8; fld 1.334; call 0x5881F0; fstp v8`, so v8 := pow result. Operand order OPEN.
- L28-30: y = min(1, 0.5 + 0.01*(rand%50)).
- L31: return 0.25*B*(y*x') - 0.25 + B.
- RNG argument in L17/L28 is a decompiler artefact (a1 overwritten). OPEN.

### 2c. Bookkeeping (AIBehaviorInitialization)

- L81-84: car eligible if laps (16444) < brain+860 (TotalRaceLaps, AIOvertake L11) and state not 2/3.
- L93: laps += 1. L94: v14 = (K - v13)*(lap/K), K = flt_5EA3FC (clock delta, not a lap time).
- L101: cumulative time 16536. L102-103: best lap 16540 = min(current, lap). L105: AIDriverSorting.
- L107-111: stop car when laps >= brain+860 or when the fill flag a3 is set.
- L123: brain+852 += v23 (lap of the last non-stopped car).

## 3. Corrections and what the sim really draws

- C1 (FACT, important). H9 section 2 Step 3 and section 4 say the sim best lap is the minimum over
  TotalRaceLaps draws. The gate in section 1 shows AIBehaviorInitialization runs once per reset (brain+834
  is set to 1 at L75/L83 and cleared only in sub_4BE700). Each run gives ONE draw per car (L89). DERIVED:
  the sim best lap = that single draw. OPEN: whether sub_46BF10 or Delegation_e6f0 is re-entered per lap
  in a skipped session. Until resolved, treat "min over 53 draws" as unsupported.
- C2. findings.jsonl:1573 says sim time = base*(1+0.15*(1-abilityNorm)). Wrong: the 0.15 term uses
  a4 = strength/120 (AIBehaviorInitialization L35). Ability enters only the draw.
- C3. findings.jsonl:1582 strength-physics claim is race-only (sub_4F1510); it does not reach the sim.

## 4. Mod write to shift simulated AI times by X %

Recommended (DERIVED):
1. Hook inside AIBehaviorInitialization immediately after the sub_4BE760 call (after L36, before the loop
   at L71). Multiply the float at brain+844 by (1+X). Gate on brain+864 in {3,4,5} (qualifying) so race
   sims are untouched.
2. Why after L36: sub_4BE760 rewrites brain+844 on every call (L20, L70, L76, L86). A write before the sim
   starts is overwritten. Write must come after that call.
3. Effect: lap' = (1+X)*B*(1+0.25*x'*y) - 0.25 (approx). Shift is X % of B, with the same spread. Clock
   (v14, L94, L101) follows the lap, so grid gaps follow too.
4. Equivalent but per-draw: multiply RVS return (0x4BEA10 L31) by (1+X). Only caller is L89 (FACT).

Alternatives, rejected or secondary:
- Strength slider dword_7D01A0: only the fallback factor (max 6.25 %), and changes race physics
  (sub_4F1510). Not suitable for a sim-only shift. Reject.
- Per-car ability (car+17600 / +17604): sensitivity (DERIVED, unclamped, B=90, E[y] approx 0.745):
  dlap/dA ~ -16.8 s per unit A (-0.19 % of B per 0.01 A). One QA point = dA -0.0455 = +0.76 s (+0.85 %).
  Clamping at x=0 cuts this. Usable only as a per-car spread lever, not a clean percentage.
Mapping strength to X is a design choice (not in code).

## 5. Open items

- O1. Draw count per sim (C1): re-entry of sub_46BF10 / Delegation_e6f0 per lap. Decides whether any "min over
  draws" exists. Blocks the grid-spread estimate.
- O2. pow operand order (2b L27). Sets the size of x' and the per-draw spread. Both readings in H9/H11.
- O3. Whether 16540 of cars is cleared before a skipped session (gates the live branch, L30-32, L41).
- O4. GDB 'Qualify Laptime' units (sub_58850F). Needed to predict absolute times.
- O5. Receiver of AIOvertake L33 call (a3 passed as this); confirm it is the brain holding +844.
- O6. Live check (owner): skip a quali after a real one; compare sim times with the GDB base.

## 6. Proposed findings (NOT written: read-only task)

- {"id":"h23-sim-one-draw-per-car-2026-10-15","tier":"DERIVED","claim":"Skipped-quali sim gives one RVS draw per car per AIBehaviorInitialization run (brain+834 gate reset only in sub_4BE700); H9 min-over-laps claim unsupported","evidence":"AIOvertake_004beb10.c:32-33; PerformanceSetupStructInitializer_004be700.c:8; AIBehaviorInitialization_004bf0a0.c:89,107-111"}
- {"id":"h23-sim-strength-factor-2026-10-15","tier":"FACT","claim":"Strength dword_7D01A0 enters sim only as B*(1+0.15*(1-s/120)) in fallback branch; not in live branch or RVS","evidence":"AIBehaviorInitialization_004bf0a0.c:35; sub_4BE760_004be760.c:73-76"}
- {"id":"h23-sim-shift-write-2026-10-15","tier":"DERIVED","claim":"Shift simulated AI times by X % by multiplying brain+844 after sub_4BE760 returns, gated on brain+864 in {3,4,5}","evidence":"AIBehaviorInitialization_004bf0a0.c:36-89; sub_4BE760_004be760.c:20,70,76,86"}

## 7. Verification and limits

- Read only: the functions/ files above, findings.jsonl 41, 776, 1573, 1582, H9 and H11. No live run.
- Not verified: pow convention, GDB units, sub_46BF10 caller graph.
- Pending: owner decision on appending section 6 to findings.jsonl; this file is not yet indexed.
