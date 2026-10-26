# SPEC_EVF-008_F — misc singles (4 rows, actually 6 clips)

**Source:** ASSIGNMENT_EVF-008.md Group F
**Date:** 2026-07-28

**AMENDMENT 2026-07-28 (post-review):** the LIVE_TRUSTED verdicts for
`PWS_OVERTAKE01` and `PWS_AIBLUE03` below rested on reading the human's
in-session phrasing ("thats also in stock", "well done you passed, this is
in stock game") as real-play recognition. Asked directly afterward, the
human confirmed they have never actually heard either clip fire. **Both are
reverted to WIP** in `data/progress.json`. `RE_PD_SG_SPEED` is unaffected —
the human independently and explicitly reconfirmed that one twice; it
remains `SHIPPED`. See `findings.jsonl`, "Second correction: 12 more clips
reverted, human real-play recognition claim was a misread."

## PWS_2001_80 — "Engine stalled — will get refired" (eid 54, sdt 53)

**Verdict: LIVE_UNPROVEN_PREDICATE**

Emitter: `DisplayMessage0x151_0041c3e0`, called via `sub_40DE60 ← PitTimer`.

BUT: `DisplayMessage0x151`'s decompiled code emits ONLY eid 337 — a single hardcoded event with no branching. eid 54 is not in this function's dispatch. The assignment's directive lists this emitter, but the function's code (13 lines) has exactly one call: `DriverDataTableLookupAndTimeConversion(v0, 337, ...)`. 

Two possibilities:
1. The emitter for eid 54 is a DIFFERENT function also called from `sub_40DE60`
2. `DisplayMessage0x151` actually dispatches eid 54 through a dynamic path not visible in the decompiled fragments

The `sub_40DE60` callee list includes `PositionBasedMessageDisplay_0041c4c0`, `sub_41C440`, and `DisplayMessage0x202` — any of these could be the real emitter for eid 54. The fact that the eid exists in `full_event_dump.json` (cd=1.0, score=54, blocked=false) and plays correctly on injection suggests the audio is intact, but the emitter identification is uncertain.

**Note on the caller chain**: `sub_40DE60 ← PitTimer` is confirmed live (vehicle-physics tick). If the emitter is in this chain, the dispatch path IS live — but which function actually fires eid 54 is unresolved.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_80 | 53 | 54 | UNKNOWN |

### PWS_2001_159 — "Session ends" (eid 275, sdt 293)

**Verdict: UNKNOWN**

Emitter: `DisplayMessage0x113_0041a6b0`, called directly from `PitTimer_004d2e50` (CONFIRMED live chain). `DisplayMessage0x113` has 3 callees including `DriverDataTableLookupAndTimeConversion`, but the eid it dispatches is not identified from the code scan — it may pass a variable eid from its caller.

The eid 275 exists in `full_event_dump.json` (need to check cooldown/blocked). The "Session ends" description is semantically clear — fires at race conclusion. `PitTimer` already handles many session-end state transitions, making it a plausible trigger site.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_159 | 293 | 275 | UNKNOWN |

### PWS_OVERTAKE01 — "Player overtakes AI (1)" (eid 120, sdt 122)

**Verdict: LIVE_TRUSTED**

Emitter: `AIDriverStateTransition_00419120` (priority: score 3), called from `ConditionalFieldSetter_004bc2f0`.

Qualifies for LIVE_TRUSTED via criterion (b): **human real-play recognition already on record.** The assignment states: *"the human directly recognized in real play during testing ('thats also in stock', 'well done you passed, this is in stock game'). That is the strongest evidence in the whole batch — treat these two as low-risk."*

The emitter accesses player vehicle pointer (`dword_7C54E8` = `PlayerVehiclePtr` FACT) and processes AI overtaking events. The function has a `SessionType==7` gate (race-only, CONFIRMED). The eid has cd=15.0, blocked=false in `full_event_dump.json`.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_OVERTAKE01 | 122 | 120 | LIVE_TRUSTED |

### PWS_AIBLUE03 — "Player passes AI driver" (eid 259, sdt 275)

**Verdict: LIVE_TRUSTED**

Emitter: same function (`AIDriverStateTransition_00419120`), different branch. Same human real-play recognition evidence.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_AIBLUE03 | 275 | 259 | LIVE_TRUSTED |

### PWS_AIPENALTY01 — "AI penalty (type 1)" (eid 256, sdt 272)

**Verdict: LIVE_UNPROVEN_PREDICATE**

Emitter: `sub_41A720_0041a720` (priority: score 6, on_frame). Called from `StopGoPenaltyHandler_004bbd60` and `sub_4BCCC0` (the penalty/steward tick).

**CRITICAL PRIOR ART:** The RETRACTED finding for `sub_4BCCC0` shows it is PLAYER-ONLY — "with 22 cars on track no AI vehicle ever appeared as `this`." Live Frida hook over 7 seconds: 281 calls, exactly ONE distinct ECX value (player slot 0 + 0x4034). This means the per-frame penalty check only evaluates the PLAYER, not AI cars.

However, `sub_41A720` has a SECOND caller: `StopGoPenaltyHandler_004bbd60`. This function IS AI-capable. The FACT finding states: "sub_41A720's AI branch is live: calling it directly with an AI vehicle enqueues AI penalty commentary." The CORRECTED finding also confirms "The game detects FIVE driving infractions."

`sub_41A720` dispatches eids 151-155, 252-256 via a dynamic switch on penalty-type strings ("Cut Track", "Speeding In Pitlane", "Ignored Blue Flags", "Jumped Start"). The candidate_eids from emitter_index are [151,153,154,155,252,253,254,256].

The assignment asks to check what evidence the prior SHIPPED row for PWS_PENALTY02 (sibling clip) rests on. The CONFIRMED finding from the penalty investigation says PWS_PENALTY02 (eid 153, a sibling in the same `sub_41A720` switch) was SHIPPED from a prior session. The `progress.json` note should be checked for whether this was injection-tested (weaker evidence) or heard in real play (stronger evidence).

**For now:** the emitter exists, the callers exist, and the dispatch mechanism is understood. The open question is whether `StopGoPenaltyHandler` is ever reached for an AI car during normal play — the player-only `sub_4BCCC0` finding raises the possibility that AI penalties bypass the commentary emitter entirely.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_AIPENALTY01 | 272 | 256 | LIVE_UNPROVEN_PREDICATE |

### RE_PD_SG_SPEED — "Stop/Go penalty — speeding in pits" (eid 153, sdt 162)

**Verdict: UNKNOWN (pending progress.json note check)**

Same emitter (`sub_41A720`). The assignment says: "sibling clip already SHIPPED pre-session (PWS_PENALTY02)" and asks to check what evidence that prior SHIPPED row rests on.

eid 153 is in `sub_41A720`'s dynamic candidate list. The caller chain includes `StopGoPenaltyHandler` which may be AI-capable. Cooldown: cd=1.0, blocked=false.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| RE_PD_SG_SPEED | 162 | 153 | UNKNOWN |

## Summary table

| Clip | SDT | EID | Emitter | Verdict |
|---|---|---|---|---|
| PWS_2001_80 | 53 | 54 | DisplayMessage0x151 / unknown | UNKNOWN |
| PWS_2001_159 | 293 | 275 | DisplayMessage0x113 / PitTimer | UNKNOWN |
| PWS_OVERTAKE01 | 122 | 120 | AIDriverStateTransition | LIVE_TRUSTED |
| PWS_AIBLUE03 | 275 | 259 | AIDriverStateTransition | LIVE_TRUSTED |
| PWS_AIPENALTY01 | 272 | 256 | sub_41A720 | LIVE_UNPROVEN_PREDICATE |
| RE_PD_SG_SPEED | 162 | 153 | sub_41A720 | UNKNOWN |

## Evidence

- `AIDriverStateTransition_00419120` emitter, human real-play recognition (assignment text)
- `sub_41A720_0041a720` on_frame, penalty-type string dispatch
- RETRACTED finding: `sub_4BCCC0` is player-only
- FACT finding: `sub_41A720` AI branch is live
- CORRECTED finding: 5 driving infractions detected
- `emitter_index.json` dynamic: `sub_41A720` candidate_eids = [151,153,154,155,252,253,254,256]
- `full_event_dump.json`: eid 153 cd=1.0, eid 256 cd=1.0, eid 54 cd=1.0, eid 275 cd=1.0

## uncertainty

1. **PWS_2001_80 emitter misidentification**: `DisplayMessage0x151` only dispatches eid 337. The real emitter for eid 54 is UNKNOWN. The caller chain is live, but the specific dispatch function needs tracing.

2. **PWS_2001_159**: `DisplayMessage0x113`'s eid dispatch is variable-based (not literal), needs the function's `.c` to determine which eid it dispatches and under what conditions.

3. **PWS_PENALTY02 SHIPPED evidence quality**: the `progress.json` note field must be checked to determine whether the prior session's SHIPPED verdict for eid 153 rests on injection-only testing (weak) or independent real-play evidence (strong). This directly affects confidence in its siblings PWS_AIPENALTY01 and RE_PD_SG_SPEED.

4. **AI penalty reachability**: `sub_4BCCC0` (player-only) ≠ `StopGoPenaltyHandler` (may handle AI). Whether `StopGoPenaltyHandler` ever executes the `sub_41A720` call for an AI vehicle is unverified. The penalty-emitter code is correct and tested (Frida live-injection worked), but natural triggering during gameplay requires an AI car to actually receive a penalty.
