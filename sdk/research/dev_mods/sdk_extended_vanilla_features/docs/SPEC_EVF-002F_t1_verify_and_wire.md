# SPEC EVF-002F — T1 verify-and-wire (Group F, 7 clips)

```
verdict summary:
  RE_PD_DQ            [T1] emitter exists, FLT_MAX cooldown — apply registered player-dq patch
  RE_OD_STUCK_7       [T1] emitter exists (sub_41AC50), cooldown 15.0, SessionType gate
  PWS_AIMECHPROB1     [T1] emitter exists (sub_41AC50, eid 208) + DriverNameIDLookup variants (203-207)
  PWS_PIT06           [T2] no emitter — needs pit-entry observer
  PWS_SLOWCAR01       [T2] no emitter — needs gap/speed observer
  RE_OD_PUNCTURE      [T2] no emitter — F1CUI external or new observer
  PWS_2001_221        [T2] no emitter — weather observer

evidence:     cross of full_event_dump.json cooldowns, grep functions/*.c for eid references,
              live reads (SessionType=7, RaceActiveFlag=3)
offsets:      RE_PD_DQ/sub_41A940: this[..] (vehicle struct, confirmed write path)
              sub_41AC50: veh+0x39FC (CurrentGear, FACT), veh+0x4064 (FinishState, FACT)
trigger:      per-clip, see below
uncertainty:  RE_OD_STUCK_7 — why does human triage say "X PLAY" if emitter exists?
              PWS_AIMECHPROB1 — which specific gear-loss scenario each eid 203-207 maps to.
              PWS_PIT06 — red_flag script pit detection mechanism not yet reviewed.
```

---

## 1. RE_PD_DQ (eid 149) — T1: cooldown unblock

**Emitter:** `sub_41A940_0041a940.c:12` — `v2 = 149`.

**Gates:**
- `dword_7D0308` (RaceActiveFlag, currently 3) ✓
- `flt_7CF3C8 != 0.0` (currently 0.27317) ✓
- `a1` = vehicle pointer, must equal player vehicle (line 8: `dword_7C54E8 == a1`)
- `a2` = string "Ignored Stop/Go Penalty" match

**Blocker:** cooldown = `3.4028234663852886e+38` (FLT_MAX). `full_event_dump.json` eid 149: blocked=true.

**Fix:** apply the already-registered `player-dq-commentary` patch from `patches/manifest.json` (changes FLT_MAX to 1.0, matching RE_PD_SG_* cooldowns). No code change needed.

**Verdict:** WIRED (by existing unapplied patch). The emitter already exists, the guard passes in-race, only the FLT_MAX cooldown blocks it.

**Uncertainty:** the `strcmp(aIgnoredStopGoP, a2)` requires the caller to pass the exact string. Who calls sub_41A940 and does it pass the right string? Callers not listed in ask.py — needs verification. The eid 149's `blocked=true` status in full_event_dump.json needs independent confirmation that applying the cooldown patch actually makes it not-blocked.

---

## 2. RE_OD_STUCK_7 (eid 211) — T1: emitter exists

**Emitter:** `sub_41AC50_0041ac50.c:43` — `v2 = 211` (default case when gear >= 7).

**Path:** Called by `EngineStressSimulation` and `FuelConsumptionAndWear`. When `a2 == 2` (stuck-in-gear mode) and `a1[3711]` (= `veh+0x39FC` = CurrentGear, FACT) doesn't match cases 0-6 (gears 1-7 mapped to eids 212-217), the default emits eid 211.

**Gates (all currently satisfied):**
- `SessionType == 7` (currently 7) ✓
- `RacePhase != 8` (RacePhase = dword_7C56A8, need live read but race is in progress) ✓
- `dword_7D0308` (currently 3) ✓
- `flt_7CF3C8 != 0.0` (currently 0.27317) ✓
- `a1 != player` (line 15: `dword_7C54E8 != a1`) — only fires for AI cars ✓
- `a1[4121] == 0` — vehicle not finished ({need live check on AI slot})
- `a2 == 2` — caller passes 2 for stuck-gear scenario

**Cooldown:** 15.0. NOT blocked. NOT FLT_MAX.

**Verdict:** WIRED. The emitter exists, the gates pass in-race, cooldown is normal. The human triage says "X PLAY" (never heard) but the code suggests it should fire when `EngineStressSimulation` / `FuelConsumptionAndWear` trigger the stuck-gear scenario on an AI car. This is a **false negative** from the triage — the clip likely works but the human never triggered the specific AI stuck-gear condition.

**Uncertainty:** `a1[4121]` (finish state?) has no named offset. Triage "X PLAY" vs emitter existing is contradictory — one needs resolving. If the stuck-gear trigger condition in `EngineStressSimulation`/`FuelConsumptionAndWear` is rarely met, this explains the silence without implying a bug.

```
LIVE READ REQUEST — RE_OD_STUCK_7 gate
  precondition: race session, find an AI car in the midfield
  commands:
    python tools/poke.py read 0x007C56A8:int32    # RacePhase
    python tools/poke.py veh 0x4064:int32 --slot 1 # FinishState for AI slot 1
  distinguishes:
    RacePhase != 8 is expected. FinishState == 0 means the car hasn't finished.
    If both satisfied, the emitter gate passes. The real question is whether
    EngineStressSimulation ever calls sub_41AC50 with a2=2 on an AI car.
```

---

## 3. PWS_AIMECHPROB1 (eids 203-208) — T1: emitter exists + driver variants

**Emitter for eid 208:** `sub_41AC50_0041ac50.c:56` — `v2 = 208` when `a2 == 0` (generic mechanical problem).

**Eids 203-207:** `DriverNameIDLookup_00415d90.c` maps driver names to these eids with different context parameters (a3=session mode, a4=polarity, a5=sub-mode). These are driver-name-contextual variants of the same "mechanical problem" audio clip. Example: `v9 = a4 != 0 ? 149 : 203` (driver-specific DQ eid).

**Gates:** same as sub_41AC50 (SessionType==7, RacePhase!=8, guard gates, AI-car only).

**Cooldown:** all 15.0. NOT blocked. NOT FLT_MAX.

**Verdict:** WIRED (partial). eid 208 (generic mechanical problem) fires from sub_41AC50. eids 203-207 fire from driver-name context in `DriverNameIDLookup` when the session context matches. Both emitters exist. The human was correct: "played already I think."

**Caveat 3 (truncated names):** `PWS_AIMECHPROB1` covers 6 eids (203-208). `PWS_AIMECHPROB0` covers 6 more (212-217). Both are truncated names — `progress.json` merges distinct gear-loss/mechanical-problem scenarios into two rows. Do not treat them as single features.

**Uncertainty:** which gear-loss/stuck scenario maps to which specific eid (203-207) is only derivable from DriverNameIDLookup parameter combinations, not from sub_41AC50 directly. The mapping table in CLAUDE.md documents some but not all.

---

## 4. PWS_PIT06 (eid 51) — T2: no emitter

**No function in the decompiled codebase references eid 51 as a commentary event.** Grep confirmed: no `vN = 51` pattern in commentary-emitter context.

**Human pointer:** red_flag NUMPAD2 script already detects pit-lane entry. Copy its mechanism.

**Needed:** find the red_flag script's pit-lane detection method (likely reads vehicle position against pit-lane zone bounding box, or checks a pit-lane state flag). Then wire that event to emit eid 51.

**Verdict:** NEEDS_TRACKER (T2). Detection mechanism exists externally, wiring needed.

**Uncertainty:** red_flag script not yet in this repo — need to locate it (likely in `mods/` or external tools). Pit-lane zone boundaries are track-specific (.aiw file). If detection uses position-checks rather than a boolean state flag, every track needs its own zone data.

---

## 5. PWS_SLOWCAR01 (eid 132) — T2: no emitter

**No function references eid 132 as commentary event.**

**Human pointer:** "reuse the same mechanism used for detection of faster AI car." This is likely the `AIOvertakeDefendStateChange_00419690` or `OpponentProximityScanTeamRecognition_0042a7d0` functions which compare car speeds and intervals.

**Needed:** find the existing speed/interval comparison, port its predicate to an observer that emits eid 132 when the player approaches a car significantly slower than race pace (about to be lapped).

**Verdict:** NEEDS_TRACKER (T2). Existing detection handles gap/speed, but no emitter.

**Uncertainty:** "slow car" detection threshold is undefined — what speed delta qualifies? The human triage says "player approaching a car about to be lapped" — this implies the target car is a lap down, which can be detected via `lapNumber` comparison between player and target.

---

## 6. RE_OD_PUNCTURE (eid 210) — T2: no emitter

**No function references eid 210 as commentary event.**

**Human pointer:** "F1CUI mod already does AI punctures." This is an external mod, not in the vanilla codebase.

**Needed:** either port the F1CUI puncture mechanism into a DLL observer, or detect the puncture state via the same `this[916] <= 0.75` threshold in sub_438130 (the puncture/blowout trigger at lines 154-173). When an AI car crosses the 0.75 tread threshold, emit eid 210.

**Verdict:** NEEDS_TRACKER (T2). The physics trigger exists (sub_438130 lines 154-173) but pushes player-focused eids. Branching on AI slot to emit eid 210 instead is the same pattern as Group C (AI tyres).

**Collapse opportunity:** Group C (AITYRES01/02) and RE_OD_PUNCTURE share the same physics observer site (sub_438130). Combine into one hook that dispatches AI-vs-player eid based on slot.

**Uncertainty:** the existing puncture threshold (tread <= 0.75, at sub_438130:154) needs live calibration. Also, eid 458 (TV variant) mapping needs verification.

---

## 7. PWS_2001_221 (eid 280) — T2: no emitter

**No function references eid 280 as commentary event.**

**Human pointer:** "F1CUI radio messages have the weather mechanism solved." This is the rain-starting commentary ("it's starting to rain").

**Needed:** detect the transition from no-rain to rain. The weather state is trackable via:
- `OnLineWetness` / `OffLineWetness` (name_index, DERIVED)
- `AmbientTempC` (name_index, DERIVED)
- The race-condition globals `flt_7CF3C8` (track condition) and related weather scalars

The observer detects when wetness transitions from near-zero to non-zero, and emits eid 280 once (edge-detect + latch, like EVF-001 race milestones).

**Verdict:** NEEDS_TRACKER (T3 — requires latch). Edge-detect: `wetness > 0 AND previous_wetness == 0 AND latch false`.

**Uncertainty:** which exact wetness field the F1CUI mod reads is unknown. Multiple wetness scales exist (0.0-1.0 grip reduction vs. mm-of-water). The threshold for "raining" vs. "damp" needs calibration.

```
LIVE READ REQUEST — weather field identification
  precondition: race session, dry track
  commands:
    python tools/poke.py list wetness
    python tools/poke.py list rain
    python tools/poke.py list OnLine
  distinguishes:
    Identify which global/field holds track wetness. In dry conditions,
    whatever reads ~0.0 is the candidate. Read again when rain starts
    (wait for visual rain on screen) to confirm it rises.
```

---

## Summary table

| Clip | SDT | EIDs | Emitter | Blocker | Verdict |
|------|-----|------|---------|---------|---------|
| RE_PD_DQ | 158 | 149 | sub_41A940:12 ✓ | FLT_MAX cooldown | T1: patch registered, not applied |
| RE_OD_STUCK_7 | 224 | 211 | sub_41AC50:43 ✓ | none (SessionType gate OK) | T1: emitter exists, should fire |
| PWS_AIMECHPROB1 | 221 | 203-208 | sub_41AC50:56 (208) + DriverNameIDLookup (203-207) ✓ | none | T1: emitter exists |
| PWS_PIT06 | 50 | 51 | none | no emitter | T2: copy red_flag script |
| PWS_SLOWCAR01 | 136 | 132 | none | no emitter | T2: reuse AI speed detection |
| RE_OD_PUNCTURE | 223 | 210 | none | no emitter | T2: reuse sub_438130 threshold |
| PWS_2001_221 | 298 | 280 | none | no emitter | T3: weather edge-detect + latch |
