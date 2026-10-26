# SILENT clip wiring effort — Bucket A/B/C for all 222 SILENT clips

Cross-references `docs/SHIPPED_MECHANISMS.md` (20 SHIPPED detector functions + 4
trivial emitters), `tools/evf_classify.py` output (`docs/RESTORATION_PLAN.md`,
T0-T6 tiers), and `batch/evf_spec/merged.jsonl` (10-category emitter specs,
`docs/EMITTER_SPECS.md`). Method is described in each section; nothing here is
an LLM re-derivation of a trigger condition — every row cites the classifier
tier or spec entry it came from.

Bucket definitions (as given in the task):
- **A** — reuses one of the 20 SHIPPED_MECHANISMS.md detector functions (or a
  trivial-emitter twin of the 4 one-off shapes): same function already fires a
  sibling clip, so the SILENT clip is a branch/threshold/cooldown fix, not a
  new hook.
- **B** — a genuinely different function/hook site, but the data the condition
  needs is already read by *some* existing function (an offset/global in
  `name_index.json` or read by a named function in `functions/*.c`), OR
  `evf_classify.py` shows the eid already has a live, unblocked call site
  (T4_CONDITION) via a function outside the 20. Needs a new small check
  inserted at an existing tick site, not new state tracking.
- **C** — no existing detector nearby, EMITTER_SPECS.md marks all of the
  clip's conditions "needs calibration" (offset/threshold unproven), or the
  clip is a T6_ORPHAN (no event resolves to it at all — nothing to hook onto
  yet).

## 1. Summary counts (222 SILENT clips)

| Bucket | Count | Composition |
|---|--:|--:|
| **A — reuse existing wiring** | **22** | 3 from T3_PATCH_APPLIED (`PWS_RETIRE01/02/04`, already registered, same function as SHIPPED `PWS_AIRETIRE*`) + 19 from T4_CONDITION whose live emitter is one of the 20 SHIPPED functions |
| **B — new wiring, small effort** | **108** | 68 from T4_CONDITION (emitter exists, live, unblocked, but the function is NOT one of the 20 SHIPPED detectors) + 40 from T5_NEW_EMITTER where `merged.jsonl` marks at least one condition `threshold_known: true` (EMITTER_SPECS.md "directly hookable") |
| **C — from scratch** | **92** | 69 from T5_NEW_EMITTER where every condition is `threshold_known: false` (needs live calibration) + 23 T6_ORPHAN (no carrier event at all) |
| **Total** | **222** | matches `data/progress.json` SILENT count exactly |

Cross-check: `python tools/evf_classify.py` → T3=3, T4=87 (19 A + 68 B),
T5=109 (40 B + 69 C), T6=23 (all C). 3+19=22 (A); 68+40=108 (B); 69+23=92 (C);
22+108+92=222. ✓

## 2. Bucket A — reuse existing wiring (22 clips)

### A1. `sub_41B250_0041b250` retirement-reason classifier (3 clips, T3_PATCH_APPLIED)

Same function that SHIPS `PWS_AIRETIRE02/03/04/05` (AI branch) — this is
literally the player branch of the identical function, already read in full
in SHIPPED_MECHANISMS.md §"Damage / retirement — AI" and family 5. Cooldown
patches for all three are already registered (not yet applied to the exe).

| Clip | EIDs | Reasoning | Citation |
|---|---|---|---|
| `PWS_RETIRE01` | 118, 411 | Player engine stall — same function, reason code `a1+16488==?`, cooldown already patched | `patches/README.md`; RESTORATION_PLAN.md T3 row; `functions/sub_41B250_0041b250.c:19` |
| `PWS_RETIRE02` | 117, 410 | Player retired (generic 1) — same function/family | `functions/sub_41B250_0041b250.c:23` |
| `PWS_RETIRE04` | 119, 412 | Player blown engine — same function/family | `functions/sub_41B250_0041b250.c:15` |

### A2. T4_CONDITION clips whose live emitter is one of the 20 SHIPPED functions (19 clips)

These already have an unblocked, live call site — per `evf_classify.py` they
need **zero new code**, only the in-game situation to occur (or, in one known
case, a data-bug fix). All are variants of a function already fully or
partially proven in SHIPPED_MECHANISMS.md.

| Clip | EIDs | SHIPPED family reused | Citation |
|---|---|---|---|
| `PWS_AIBLUE03` | 259 | `AIDriverStateTransition_00419120` (family 11/19, ships MISC14/OVERTAKE01) | RESTORATION_PLAN.md T4; `functions/AIDriverStateTransition_00419120.c:39` |
| `PWS_OVERTAKE01` | 120, 413 | same function, different branch | `functions/AIDriverStateTransition_00419120.c:37` |
| `PWS_AIINCID10` | 225 | `sub_41AD80_0041ad80` (family 5, ships `PWS_HIT08/10`) | `functions/sub_41AD80_0041ad80.c:53` |
| `PWS_AIINCID11` | 226 | same function | `functions/sub_41AD80_0041ad80.c:88` |
| `PWS_AIPIT08` | 180 | `AIFormationLap_0041a3a0` (family 4, ships `PWS_AIPIT10`/`PWS_QUAL01`) — **known bug**: strategy index (`veh+0x342C`) always reads 3, so this branch is unreachable until that offset's real writer is found (findings.jsonl CONFIRMED) | `functions/AIFormationLap_0041a3a0.c:24`; findings.jsonl "aiPitStrategyIndex" |
| `PWS_AIPIT09` | 181 | same function/bug | `functions/AIFormationLap_0041a3a0.c:24` |
| `PWS_GREEN01` | 126, 419 | `PitSelectionStateMachine_0041b3c0` (family 9/21, ships `PWS_AILAPPIN02/03/04`) | `functions/PitSelectionStateMachine_0041b3c0.c:72` |
| `PWS_RED01` | 131, 416 | same function | `functions/PitSelectionStateMachine_0041b3c0.c:151` |
| `PWS_YELLOW04` | 133 | same function | `functions/PitSelectionStateMachine_0041b3c0.c:101` |
| `PWS_YELLOW05` | 135 | same function | `functions/PitSelectionStateMachine_0041b3c0.c:83` |
| `PWS_YELLOW06` | 137 | same function | `functions/PitSelectionStateMachine_0041b3c0.c:55` |
| `RE_PD_FL_BAD_BL` | 94, 384 | `PitStateSlotActivation_0041ab30` (family 17 tyre chain, ships `PWS_TYRES01/02`) | `functions/PitStateSlotActivation_0041ab30.c:28` |
| `RE_PD_FL_BLIST` | 95, 385 | `AISessionPhaseCounter_0041aa10` (same family 17 chain) | `functions/AISessionPhaseCounter_0041aa10.c:28` |
| `RE_PD_FR_BAD_BL` | 96, 386 | `PitStateSlotActivation_0041ab30` | `functions/PitStateSlotActivation_0041ab30.c:31` |
| `RE_PD_FR_BLIST` | 97, 392 | `AISessionPhaseCounter_0041aa10` | `functions/AISessionPhaseCounter_0041aa10.c:31` |
| `RE_PD_RL_BAD_BL` | 98, 391 | `PitStateSlotActivation_0041ab30` | `functions/PitStateSlotActivation_0041ab30.c:34` |
| `RE_PD_RL_BLIST` | 99, 394 | `AISessionPhaseCounter_0041aa10` | `functions/AISessionPhaseCounter_0041aa10.c:34` |
| `RE_PD_RR_BAD_BL` | 100, 395 | `PitStateSlotActivation_0041ab30` | `functions/PitStateSlotActivation_0041ab30.c:37` |
| `RE_PD_RR_BLIST` | 101, 396 | `AISessionPhaseCounter_0041aa10` | `functions/AISessionPhaseCounter_0041aa10.c:37` |

Note: the 8 `RE_PD_*_BLIST`/`*_BAD_BL` rows are literally the same tyre-tread
threshold chain already FACT-proven for `PWS_TYRES01/02` — just the other 3
corners (front-left/right, rear-left/right vs. the shipped corner). This is
the single cheapest bucket-A group: same live-disassembled threshold
constants, different corner index.

## 3. Bucket B — new wiring, small effort (108 clips)

### B1. T4_CONDITION clips with a live, unblocked emitter that is NOT one of the 20 SHIPPED functions (68 clips)

`evf_classify.py` confirms these eids already have a live call site in the
exe (no cooldown blocking them) — so strictly no new *code* is needed, only
the right in-game situation — but the emitting function itself (e.g.
`TeamNameDisplayIDLookup_0041b980`, `SeasonDisplayIDResolver_0041daf0`,
`PitRaceStatusMessageDispatcher_0041be60`, `sub_41A720_0041a720`,
`RaceCommentary_0041d630`, `GdbSeasonSelector_00420cf0`,
`ConstructorStandingsParser_00420820`, `ScoringClassificationDisplay_0041e320`,
`AISessionModeTransition_004193a0`, `PositionChangeDetector_00420490`,
`PitExit_0041c0f0`, `sub_41A940_0041a940`) is not part of the 20-function
SHIPPED_MECHANISMS list, so it isn't a documented "reuse" case — bucketed B
rather than A per the task's stricter Bucket-A definition. Full per-clip list
(citation format: `eid live, emitter <function>.c:<line>` per
`docs/RESTORATION_PLAN.md` T4_CONDITION table):

`PWS_2001_13`, `PWS_2001_131`, `PWS_2001_138`, `PWS_2001_14`, `PWS_2001_141`,
`PWS_2001_15`, `PWS_2001_159`, `PWS_2001_17`, `PWS_2001_18`, `PWS_2001_19`,
`PWS_2001_20`, `PWS_2001_22`, `PWS_2001_24`, `PWS_2001_25`, `PWS_2001_33`,
`PWS_2001_80`, `PWS_AIMECHPROB0`, `PWS_AIMECHPROB1`, `PWS_AIPENALTY01`,
`PWS_AIPENALTY02`, `PWS_AIPENALTY04`, `PWS_AIPENALTY13`, `PWS_AIPENALTY22`,
`PWS_AIPIT11`, `PWS_AITYRES01`, `PWS_AITYRES02`, `PWS_CHANGEPIT01`,
`PWS_MISC02`, `PWS_MISC03`, `PWS_MISC04`, `PWS_MISC05`, `PWS_MISC46`,
`PWS_MISC47`, `PWS_PENALTY02`, `PWS_PENALTY03`, `PWS_PENALTY04`,
`PWS_PENALTY05`, `PWS_PENALTY07`, `PWS_PENALTY14`, `PWS_PIT09`, `PWS_PIT10`,
`PWS_PIT12`, `PWS_QUAL28`, `PWS_QUAL29`, `PWS_QUAL30`, `PWS_QUAL32`,
`PWS_SCAR01`, `PWS_SCAR03`, `PWS_SLOWCAR01`, `PWS_WIN20`, `PWS_WIN21`,
`PWS_YELLOW01`, `PWS_YELLOW02`, `PWS_YELLOW03`, `RE_OD_EXIT_RED`,
`RE_OD_LAP_DISAL`, `RE_OD_LOST_7`, `RE_OD_MONSOON`, `RE_OD_PUNCTURE`,
`RE_OD_STUCK_7`, `RE_OD_WET`, `RE_PD_25_DD`, `RE_PD_DQ`, `RE_PD_EXIT_RED`,
`RE_PD_INV_DD`, `RE_PD_LAP_DISAL`, `RE_PD_SG_CUT`, `RE_PD_SG_SPEED`.

**Note on `PWS_QUAL32`** (Team front row lockout, eid 397): the emitter is
`PositionChangeDetector_00420490`, adjacent to but *not* the same function as
family 3's `AIPositionChangeEvent_004198b0` — close cousin, still B not A
(different symbol, confirmed by reading the function per this project's own
"Event emission chain for PWS_QUAL32" note in the local CLAUDE.md).

Citation for the whole group: `docs/RESTORATION_PLAN.md` §T4_CONDITION
(generated by `tools/evf_classify.py`, rule-based, no LLM judgement).

### B2. T5_NEW_EMITTER clips with at least one directly-hookable condition (40 clips)

`batch/evf_spec/merged.jsonl` marks these `threshold_known: true` for at
least one of their eid conditions — i.e. EMITTER_SPECS.md's "68 directly
hookable" pool. All share one of the 6 `MainGameTickUpdate`-family hook sites
already used by 6/10 spec categories, so — per this project's CLAUDE.md rule
that same-tick-site features must share one DLL — these become new branches
in one shared observer, not 40 separate hooks.

| Clip | Category (EMITTER_SPECS.md) | Reasoning |
|---|---|---|
| `PWS_2001_102` | unclassified | lap-time-improvement check reuses `veh+0x60`/`veh+0x68` (already read by `LapCompletion_004bc4e0`); threshold known |
| `PWS_2001_163/164/165` | session_phase | leader lap fraction vs `TotalRaceLaps` (CONFIRMED global), threshold known |
| `PWS_2001_218/220/221` | weather_track | `OnLineWetness` threshold crossing, CONFIRMED global, thresholds stated |
| `PWS_2001_310/311/312` | weather_track | fastest-AI tire-compound check via `nextChosenTires`, CONFIRMED offset |
| `PWS_AIBLUE01` | flags_incident | player blue-flag byte transition, DERIVED offset but threshold known |
| `PWS_AIINCID03/06` | flags_incident | off-track byte transition (2.5m threshold FACT-cited in `sub_4BCCC0_004bccc0.c:210-220`) |
| `PWS_AIPENALTY05/07` | penalty_steward | cut-track / stop-go penalty thresholds cited in `sub_4BCCC0` |
| `PWS_MECHPROB06/07/08/09` | tyre_condition | tread crosses 0.75 downward — literally the sibling threshold below the already-FACT 0.85 blister band (same `VehicleDynamicsForceAccumulatorIntegration_00438130`, next line down) |
| `PWS_MECHPROB14/16/20-26` | mechanical | `veh+0x16484`/`0x16488`/`0x39FC`/`0x39EE` all CONFIRMED/FACT reads from `FuelConsumptionAndWear_004ed960` |
| `PWS_MISC07/09` | pit_procedure / penalty_steward | pit-speed-limit and blue-flag-investigation thresholds known |
| `PWS_MISC48` | penalty_steward | cut-track threshold known |
| `PWS_MISC54` | pit_procedure | `callToBox` 0→1 transition, CONFIRMED offset, threshold known |
| `PWS_OFFROAD01` | flags_incident | same 2.5m off-track byte as AIINCID03/06 |
| `PWS_OIL01/02/03` | weather_track | engine-failure ReasonCode==9 known, only sector-location unresolved (doesn't block the base clip) |
| `PWS_SCAR02` | flags_incident | `RacePhase` transitions to 8, FACT/CONFIRMED global |
| `PWS_TEAMPIT01/02` | pit_procedure | `callToBox`/`pittingStatus` transitions, CONFIRMED offsets |
| `RE_OD_FASTEST_M` | weather_track | fastest-AI-on-monsoon-tires, same mechanism as 310/311/312 |

Full 40-clip list (machine-derived, see `batch/evf_spec/merged.jsonl`):
`PWS_2001_102, PWS_2001_163, PWS_2001_164, PWS_2001_165, PWS_2001_218,
PWS_2001_220, PWS_2001_221, PWS_2001_310, PWS_2001_311, PWS_2001_312,
PWS_AIBLUE01, PWS_AIINCID03, PWS_AIINCID06, PWS_AIPENALTY05, PWS_AIPENALTY07,
PWS_MECHPROB06, PWS_MECHPROB07, PWS_MECHPROB08, PWS_MECHPROB09,
PWS_MECHPROB14, PWS_MECHPROB16, PWS_MECHPROB20, PWS_MECHPROB21,
PWS_MECHPROB22, PWS_MECHPROB23, PWS_MECHPROB24, PWS_MECHPROB25,
PWS_MECHPROB26, PWS_MISC07, PWS_MISC09, PWS_MISC48, PWS_MISC54,
PWS_OFFROAD01, PWS_OIL01, PWS_OIL02, PWS_OIL03, PWS_SCAR02, PWS_TEAMPIT01,
PWS_TEAMPIT02, RE_OD_FASTEST_M`.

Citation: `docs/EMITTER_SPECS.md` "68 directly hookable" + `threshold_known:
true` fields in `batch/evf_spec/merged.jsonl`, cross-checked clip-by-clip
against `docs/RESTORATION_PLAN.md` T5_NEW_EMITTER list.

## 4. Bucket C — from scratch (92 clips)

### C1. T5_NEW_EMITTER clips where every condition needs live calibration (69 clips)

`merged.jsonl` marks `threshold_known: false` for all of the clip's eid
conditions — EMITTER_SPECS.md's "needs calibration" pool. A hook site is
proposed (mostly `MainGameTickUpdate`) but the actual trigger value (a
threshold, an enum mapping, or an unlocated offset) is unproven statically.

`PWS_2001_132, PWS_2001_133, PWS_2001_134, PWS_2001_135, PWS_2001_136,
PWS_2001_149, PWS_2001_222, PWS_2001_308, PWS_2001_309, PWS_2001_68,
PWS_2001_69, PWS_2001_70, PWS_2001_71, PWS_2001_72, PWS_2001_73, PWS_2001_74,
PWS_2001_75, PWS_2001_76, PWS_2001_77, PWS_2001_78, PWS_2001_79,
PWS_2001_96, PWS_AIBLUE02, PWS_AIFASTLAP03, PWS_AIINCID01, PWS_AIINCID04,
PWS_AIINCID07, PWS_AIINCID08, PWS_AILAPPIN01, PWS_AILAPPIN05,
PWS_AIOFFTRACK0, PWS_AIPENALTY03, PWS_AIPENALTY08, PWS_AIPENALTY14,
PWS_AIPIT13, PWS_MECHANWARN0, PWS_MECHPROB27, PWS_MECHPROB28,
PWS_MECHPROB29, PWS_MECHPROB30, PWS_MECHPROB31, PWS_MECHPROB32,
PWS_MECHPROB33, PWS_MISC06, PWS_MISC08, PWS_MISC18, PWS_MISC25, PWS_MISC26,
PWS_MISC27, PWS_MISC28, PWS_MISC29, PWS_MISC30, PWS_OD_HOLDING,
PWS_PENALTY08, PWS_PIT01, PWS_PIT03, PWS_PIT06, PWS_PIT08, PWS_PIT13,
PWS_RACEFAST03, PWS_SCAR05, RE_OD_25_DD, RE_OD_INTER, RE_OD_INV_DD,
RE_OD_SG_DD, RE_OD_SLICKS, RE_PD_SG_DD, RE_PD_STALLED, RE_PD_STALLED_T`.

Representative reasons (full detail in `docs/EMITTER_SPECS.md` "Needs live
calibration" / "Open questions" per category):
- Tire-change/refuel family (`PWS_2001_68-79`): needs `SLOW_THRESHOLD`,
  `FUEL_JAM_TIMEOUT`, `REFUEL_RATE_THRESHOLD` — none located statically.
- Stuck-gear family (`PWS_MECHPROB27-33`): DERIVED discriminator
  (`veh+0x39EE==0 && veh+0x3D60==1`) not live-verified — genuinely unproven,
  unlike the sibling "lost gear" family (Bucket B) whose predicate is
  `threshold_known: true`.
- Pit-lane zone detection (`PWS_PIT01/03/06/08`, `RE_PD_STALLED*`): "no single
  'InPitLane' flag found" — needs waypoint-based zone reverse engineering.
- AI incident/pace family (`PWS_AIINCID*`, `PWS_MISC25-29`, `PWS_OD_HOLDING`):
  gap/angular-velocity/pace-delta thresholds all unmeasured.
- Penalty-type enumeration (`PWS_AIPENALTY*`, `RE_OD/RE_PD *_DD`): penalty
  type codes (3/5/7/8/14) and an "investigation phase" concept are not
  confirmed to exist in the code at all — open question in EMITTER_SPECS.md,
  not just an unmeasured constant.

### C2. T6_ORPHAN — no carrier event resolves to the clip at all (23 clips)

Per `evf_classify.py`: `data/full_event_dump.json` has no event id that maps
to these clips' SDT slot — there's nothing to hook onto until a carrier event
is chosen and its score/priority repointed to the clip. This is a strictly
harder starting point than C1 (which at least has a target eid), so all 23
are Bucket C regardless of how simple their eventual condition might be:

`PWS_2001_09, PWS_2001_110, PWS_2001_161, PWS_2001_219, PWS_2001_223,
PWS_2001_225, PWS_AIINCID02, PWS_AIINCID05, PWS_AIPIT05, PWS_AIPIT07,
PWS_AIRETIRE01, PWS_AIWIN03, PWS_HIT09, PWS_MISC12, PWS_OFFROAD02,
PWS_OVERTAKE02, PWS_OVERTAKE03, PWS_RETIRE03, PWS_SCAR04, RE_PORSCHE_03,
RE_PORSCHE_23, RE_PORSCHE_24, RE_PORSCHE_25`.

Citation: `docs/RESTORATION_PLAN.md` §T6_ORPHAN (all rows: "no event resolves
to this clip").

## 5. Honesty notes on the bucket mapping

- The task's Bucket A is defined narrowly (must match one of the 20
  SHIPPED_MECHANISMS.md detectors). Several B1 clips (e.g. `PWS_QUAL32`,
  `PWS_YELLOW01-03`) sit right next to a SHIPPED family — same subsystem,
  different symbol — and a future session could plausibly fold them into a
  family if the two functions turn out to share a common caller. Flagged, not
  forced into A.
- The B/C split for T5 is a direct read of `threshold_known` in
  `merged.jsonl`, which is itself DERIVED (single static reading by a
  worker, merger-verified for citation existence only, not for correctness).
  Treat "Bucket B, T5" as "as buildable as EMITTER_SPECS.md says", not as
  independently re-verified here.
- A clip appearing hookable (B) does NOT mean cheap in wall-clock terms if it
  requires DLL plumbing shared across many other B clips at the same tick
  site — "small effort" is per-condition-check effort, not first-DLL effort.

## 6. Driver / constructor / track-name audio: spliced at runtime, or separate static clips?

**Answer: driver, constructor, and track identity are resolved at runtime
to a *numeric event ID* that selects one of a fixed, finite set of
pre-recorded clips — not text spliced into a shared audio clip, and (as far
as static analysis shows) not a separate "name" audio clip concatenated onto
a generic phrase either.**

Trust tier: **CONFIRMED** for "selection is a numeric-ID lookup, not text
substitution into one shared clip" (the code was read in full and the string
formatting is unambiguous). **UNKNOWN** for "is the driver's name ever
spoken at all in these lines" — no evidence either way was found in the
static corpus; that requires listening to the actual clips, not reading code.

Evidence:

1. **File path builder never puts a driver/team name into the played
   filename** — `functions/sub_414600_00414600.c:18` builds the PWS path as
   `sprintf(buf, "%spws%s_", dir, name)` where `name` = `*this` (a *category*
   tag, e.g. "fb"/"fe"/"sb"/"se" for the 4 audio-quality/format variants
   appended right after at lines 19/24/29/34 via `aFb`/`aFe`/`aSb`/`aSe`), NOT
   the driver's name string. The driver/constructor identity only enters
   through the **call to `DriverNameIDLookup(this+4, this[2], 0, 1, 1)`**
   (line 21) — `this[2]` is a driver index, but its return value overwrites
   a numeric field in the struct (`a1+8`), not the path string. TV paths
   (line 45, `"%stv%s_"`) follow the identical pattern.
2. **`DriverNameIDLookup_00415d90.c:1-40`** (`sub_415D90`) takes the driver
   index (`a2`), reads the driver's name string at
   `dword_8AC394 + 1060*a2 + 188`, and does a literal `sub_566810`
   string-compare against embedded name constants (`aMichaelS`, `aRubensB`,
   `aMikaH`, ...). Each match returns a **small integer event ID** (204/402,
   209/411, 403/17, ... — see this folder's own CLAUDE.md
   "Driver-name → event-ID mapping table" section, itself built from this
   exact function). `sub_417640_00417640.c` does the identical thing for
   constructor names (`FERRARI`→409/420/426/437, etc.) reading
   `dword_8AC394 + 1060*a2 + 124`.
3. **Track names work the same way inside `PerDriverRaceCommentary_00419c50`**
   (SHIPPED_MECHANISMS.md family 1): a `sub_566810` string-compare chain
   against literal track-name constants (`aAlbertPark`, `aInterlagos`, ...,
   lines 24-104) selects one of 17 win-clip event IDs.
4. **The clip catalog itself is a small fixed table, not per-driver/per-track
   files**: `data/event_clip_map.json` has 482 entries total, each mapping a
   numeric event id straight to a *generic* SDT clip code — e.g. eid 161 →
   `PWS_WIN01.mp2` (one clip for "Player wins Australian GP" as a whole
   phrase), not `PWS_WIN01_Schumacher.mp2` or similar. There is exactly one
   audio file per (event, GP) combination, confirming the 17 win lines
   really are 17 separately pre-recorded files (one per track), matching the
   code-level string-compare chain 1:1 — this is the CONFIRMED part of
   SHIPPED_MECHANISMS.md's own WIN-family note.
5. No `strcat`/`sprintf` call anywhere in `sub_414600`, `DriverNameIDLookup`,
   or `sub_417640` builds a filename or buffer containing the driver's own
   name text (`grep -n "sprintf\|strcat\|%s" functions/sub_414600_00414600.c`
   shows only the 4 fixed category tags, never `this[2]`'s string). This is
   the basis for "not text spliced into a shared clip."
6. What is genuinely **UNKNOWN**: whether the resulting static clip itself
   contains a *spoken* driver/team name (recorded once per phrase, e.g. "And
   it's Schumacher who takes the win at Monza") baked into the one .mp2 for
   that eid, or whether the phrase is generic ("Our driver wins here") and
   the game simply never speaks names at all. Nothing in `strings_index.json`
   or the decompiled commentary-controller code plays a second "name" clip
   layered on top of the phrase clip — no second SDT lookup or audio-mixing
   call was found near the PWS/TV playback path. This would need someone to
   actually listen to `PC_RE_MAIN.SDT`'s `PWS_WIN01`-style clips (or check
   `findings.jsonl` for a prior listening session) to resolve — not
   re-derivable from static code alone, so left UNKNOWN rather than guessed.

No matching entry was found in `findings.jsonl` for "commentary audio name
splicing" via keyword search at the time of writing — if one is added later
by an actual listening pass, this section should be updated to CONFIRMED or
FACT accordingly.
