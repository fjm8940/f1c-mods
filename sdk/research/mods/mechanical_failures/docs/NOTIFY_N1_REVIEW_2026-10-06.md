# N1 review - Claude, 2026-10-06 (DeepSeek N run)

Reviewed: `docs/NOTIFY_INVENTORY_2026-10-06.md` (SHA-256 5DDD769D..., 326 lines, 81 rows).
Checks: sources unchanged by N1 (engine_failure.c 15:36, mf_engine_model.h 14:53 = Claude's last edits);
before-copy folder holds the handoff log only (correct - no code changed). Note: the N1 chat summary said
"gates all match"; section 8 of the inventory says gates were NOT re-run in N1 (baseline quoted). From N2 on,
run every gate and paste the real numbers.

## Verdict: inventory ACCEPTED. Proceed to N2, then N3 with the decisions below.

## Decisions for N3 (approved unless marked otherwise)
1. **R7 hold-until-eligible** (all MFV instruction slots): APPROVED as in the prompt (max 120 s, same
   fault + onset, drop log line).
2. **H10 low-coolant steam < 100 C** (N79): APPROVED - apply MF_WATER_STEAM_MIN_K to that path too.
3. **H12/H13a clear lines** (N29 RADIATOR CLEARED, N58 MF_RAD03, stint_clear :5554): APPROVED - a clear line
   only if that onset's diagnosis was shown/said (W_RADBLOCK or W_WATER blink on, or the RAD01/WATER01 pre
   line went out). stint_clear and any session/pit clear: NO radio, no HUD.
4. **H13b REPAIR01 / ENGINE REPAIRED** (N30, N59): APPROVED - only if a diagnosis of the current onset was
   shown/said; a panel REPAIR on a healthy or undiagnosed engine is silent (log only).
5. **H13c per-line limit**: APPROVED - every MFV line and every HUD diagnosis text at most once per 120 s
   game time across re-arms, except death lines and a genuinely new fault TYPE.
6. **H15 exhaust**: APPROVED wording - HUD diagnosis `EXHAUST CRACKED` (replaces "EXHAUST"), instruction
   `SHORT SHIFT` (replaces "WATCH THE REAR"). MF_EXH01 "Exhaust is cracked." stays. MF_EXH02 ("keep pushing")
   is NOT said any more (no fallback line); SDT untouched; owner decides a new recording later.
7. **Blow-by R1 (N08, N17)**: APPROVED observable = the visible blow-by smoke (R2 visible effect: the model's
   blow-by smoke bank is being written, smoke_bank1 < 0 from the rings path) for >= 5 s game time, AND the
   existing k > 0.3. Diagnosis text stays "ENGINE PROBLEM", instruction "EASE OFF" (R10 ok: the model rewards
   less load). No new numeric threshold.
8. **R6 criticals (N20-N24, N48)**: the health thresholds stay (R2 allows near-terminal internal state for
   criticals). NEW ordering rule, APPROVED: a critical (HUD red line and its radio) never fires before that
   onset's diagnosis was shown/said (fixes H14: MF_CRIT01 at 60 s before BLOWBY01 at 95 s). If the
   diagnosis is not yet out when the critical condition becomes true, show the diagnosis first, then the
   critical >= 5 s later.
9. **R5 dev-panel arm paths** (N32, N33, N35, N81: stage-one/terminal/seize/legacy overheat buttons): OUT OF
   SCOPE - test tools. Do not change. List them as "dev-only" in N4.
10. **R4 niggle SENSOR FAULT (N31)**: KEEP (owner-approved feature, already silent with Failure Rate "none").
11. **H16 logging**: APPROVED - add fuel lost by the leak (litres, 2 decimals) to the "MODEL 5s" line.
    Also APPROVED, scope extended to `tools/mech_test_panel.py`: the panel's dead-engine label must show the
    model's death reason string (e.g. "over-rev", "engine life expired", "head gasket"), not the active
    fault name (owner live: "ENGINE DEAD - fuelleak" after an over-rev death). If the reason is not in the
    shared block, add it at the END of EngineCtl with an abi_version bump; panel must accept old ABI.
12. **N47 eid 77 fallback (vanilla SDT only)**: APPROVED - add the absolute floor (water >= 110 C,
    MF_WATER_COOL_MIN_K) like the model's warn_cool_engine.
13. **N14 / N71 R10 notes**: PASS, no change.
14. **radio_pump without explicit g_pump_frozen guard**: APPROVED - add the guard (no request while paused).

N5 (REPAIR on a hot engine) and N6 (pit lane must not clear faults) remain approved as written in the prompt.
Every N3 fix needs its failing-before / passing-after notify_test case, as the prompt says.

## Addendum (Claude, 2026-10-06, after N2 acceptance) - sounds, APPROVED for N3
15. **Backfire pops are silent in the dev build (owner live: "cannot load ...\development\mods\Audio\Vehicle\
    backfire_ext_2/5/6.wav").** `exhaust_pop()` (~engine_failure.c:4302) and the AI variant (~:7164-7181)
    build the game folder as "this DLL's folder, three levels up" - correct only for the plugin copy in
    <game>\plugins\lightloader. The files exist in <game>\Audio\Vehicle (backfire_ext_2/5/6.wav, verified).
    Fix: resolve the game folder from the game EXE (GetModuleFileNameA(NULL, ...), strip the file name),
    for both functions. Log once per session if a file still fails to load.
16. **Sound = its visible/condition trigger.** In the inventory add/verify a row per sound: steam hiss only
    when the steam plume is actually shown (after H10: water >= 100 C); engine-blow only on a bang death
    (not on quiet stops like blow-by/gasket "no bang"); backfire pops only with a flame spit; no sound while
    paused or outside a live drive. Fix any mismatch by these rules only.

## N7 (APPROVED BEHAVIOUR CHANGE, owner 2026-10-06: "the existing over rev system should be overwritten
## when the mod is active") - run AFTER N3 is gated green, as its own batch
Facts (Claude, static + live; do not re-derive):
- The game's player engine life P+7604 (0x7C91DC, P = 0x7C7428) drains each physics step in
  EngineStressSimulation 0x43EF20 by
  `(0.5*(2^((|w| - P+7612) * P+7620) + 2^((oil - P+7628) * P+7636)) + P+7596) * dt(P+104)`
  (machine code 0x43F1A7-0x43F212; |w| = fabs(P+7260) read at function start; pow = 0x5881F0).
  With the F2004's RPM range (15.7 rad/s) the rpm half is ~97/s at the limit and ~6e5/s at 110 %, so an
  over-rev empties the whole clock in one step (live HANDOFF 7.26). That IS the vanilla over-rev death.
- Prologue 0x43EF20 (capstone on F1 Challenge Online.exe): `55 | 8B EC | 83 E4 F8` = push ebp; mov ebp,esp;
  and esp,-8 -> steal 6, site 8-aligned. thiscall, this = P. Player only (AI use another path). No other
  first-party mod detours it.
Spec:
1. Detour 0x43EF20 with the existing atomic `mf_detour_install` (expect bytes 55 8B EC 83 E4 F8, stolen 6),
   removed in the same remove path as the lamp/shift detours. Hook = thiscall void(void *self).
2. PRE-COMPENSATION before calling the trampoline (game thread, exact same step):
   if hooks not stopping AND PLR_FAILRATE != 0 AND engine life > 0 AND |w| = fabs(*(double*)(P+7260)) >
   lim = *(double*)(P+7388) (the CURRENT limit, i.e. the mod-lowered one when a valve/save band is held):
       excess = 0.5 * ( 2^((|w| - P7612) * P7620) - 2^((lim - P7612) * P7620) ) * dt(P+104)
       if finite and > 0: life(P+7604) += excess
   The game then subtracts its full drain, so the net rpm wear above the limit = the wear AT the limit. The
   oil half, stress and everything else stay vanilla. Use the game's own fields only (read in the hook).
   Never write when life <= 0 or not finite; clamp excess to <= 1e6 per call.
3. The mod's over-rev layer becomes the only over-rev consequence (already in the model: torque-zero
   "valve hit piston" death after MF_OVR_HARD_S, valvetrain/rod damage, OVER-REV notice/radio). Keep it.
4. Panel/log: count compensated seconds; log once per over-rev episode
   "OVERREV OWNER: vanilla life drain above the limit neutralised (x s)".
5. Tests: notify/engine harness cannot run the exe - add a pure helper `mf_overrev_excess(w, lim, base,
   inv_range, dt)` in mf_engine_model.h (or mf_notify.h) with tests: w <= lim -> 0; 110 % F2004 numbers ->
   ~6e5*dt; non-finite/negative -> 0. Live check is Claude's/owner's.
6. Stop and report if the prologue bytes differ or any gate drops.
