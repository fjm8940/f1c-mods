# H7 session and flag lines (design batch, research only)

Output of batch H7_session_flags. Machine-readable rows: `data/design/H7_session_flags.json`.
No game code, no live tests, no edits to EVF, other mods, or the corpus. Tiers: FACT > CONFIRMED > DERIVED > GUESS.

## Clip table

| Clip | Says | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|
| PWS_QUAL29 | Some drivers on track | owned_by_other_mod | quali, 6-10 active cars, latched | none (live-confirmed 07-28) |
| PWS_QUAL28 | Very few drivers on track | owned_by_other_mod | quali, 1-5 active cars, latched | none |
| PWS_QUAL30 | Most drivers on track | owned_by_other_mod | quali, 11+ active cars, latched | none |
| PWS_2001_17 | AI driver starting 1st run | game_state_emitter | quali, AI leaves pit exit onto first run | run counter UNKNOWN |
| PWS_2001_18 | AI driver starting 2nd run | game_state_emitter | as 1st, run counter 2 | run counter UNKNOWN |
| PWS_2001_19 | AI driver starting 3rd run | game_state_emitter | as 1st, run counter 3 | run counter UNKNOWN |
| PWS_2001_20 | AI driver starting 4th run | game_state_emitter | as 1st, run counter 4 | run counter UNKNOWN |
| PWS_2001_14 | AI driver ahead on Flying Lap | game_state_emitter | AI on flying lap and ahead of player | flying-lap detector and "ahead" comparator UNKNOWN |
| PWS_2001_15 | AI driver ahead on In Lap | game_state_emitter | AI with pittingStatus 1 and ahead of player | "ahead" comparator UNKNOWN; priority 1 |
| PWS_2001_13 | AI driver ahead on Out Lap | game_state_emitter | AI on out-lap and ahead of player | out-lap transition and comparator UNKNOWN |
| PWS_QUAL32 | Team front row lockout | carrier_score | P1 and P2 share a team id | no PWS eid; carrier not chosen; predicate polarity unclear |
| PWS_GREEN01 | Green flag conditions | hook_native | flag-clear path, eid 126 | RacePhase gate at line 36 (suspected) |
| PWS_YELLOW04 | Sector 1 cleared | hook_native | sector-1 yellow clear, eid 133 | same RacePhase gate |
| PWS_YELLOW05 | Sector 2 cleared | hook_native | sector-2 yellow clear, eid 135 | same RacePhase gate |
| PWS_YELLOW06 | Sector 3 cleared | hook_native | sector-3 yellow clear, eid 137 | same RacePhase gate |
| PWS_RED01 | Red flag - session stopped | owned_by_other_mod | red_flag NUMPAD2 procedure | none (owned) |
| PWS_SCAR01 | Safety car deployed, exiting pits | owned_by_other_mod | full_safety_car deploy release | none (owned) |
| PWS_SCAR02 | Safety car on track | owned_by_other_mod | full_safety_car on-track state | none (owned) |
| PWS_SCAR03 | Safety car ending, green this lap | owned_by_other_mod | full_safety_car recall warning | none (owned) |
| PWS_SCAR05 | Safety car in pit lane (2) | owned_by_other_mod | full_safety_car pit entry | none (owned) |
| PWS_2001_163 | 25% race distance complete | game_state_emitter | race, lapNumber >= ceil(0.25 x laps), latched | formation lap and timed-race behaviour (calibration) |
| PWS_2001_164 | 50% race distance complete | game_state_emitter | as 25%, 0.5 | as 25% |
| PWS_2001_165 | 75% race distance complete | game_state_emitter | as 25%, 0.75 | ties with finish line (priority 40) |
| PWS_2001_159 | Session ends | hook_native | call native sub_41A6B0 (eid 275) at race end | caller of sub_41A6B0 not in corpus |

## Findings

**Yellow and green clears: a gate, not a cooldown (DERIVED).** `functions/PitSelectionStateMachine_0041b3c0.c` (sub_41B3C0) emits 133/135/137 and 126 only on the clear path (a2==0). The clear path returns early at line 36 when `dword_7C56A8 == 5`. Racing phase 5 is read as "racing" by `functions/sub_4D1730_004d1730.c:48` (gated on RacePhase 5 in race) and by the SC mod comment at `development/mods/full_safety_car/dll/pace_probe_standings.c:336`. The top gate (line 11) also blocks everything at RacePhase 8 (safety car). If the owner's yellow clears happen while RacePhase is 5, this line explains the silence without any cooldown problem. EVF's SPEC_EVF-002A gate list omits this line-36 gate.

**Raise and clear split (CONFIRMED by code).** `functions/AISessionModeTransition_004193a0.c` (sub_4193A0) emits the raise lines 134/136/138 and only pends 133/135/137 (BooleanOROfTwoChecks at lines 22, 28, 34). It also sets `this[band+316] = 2` (line 38), which the clear path requires. This matches the EVF CLAUDE row of 2026-08-01.

**Sector mapping (DERIVED).** Band index 1 = sector 1, 2 = sector 2, 0 = sector 3. Raise (v5 1/2/else) emits 134/136/138; clear (band 1/2/0) emits 133/135/137. Consistent with docs/56 names.

**Red flag (CONFIRMED).** Native case 3 of sub_41B3C0 emits 131 (line 151), but the playing path is the red_flag mod's NUMPAD2 hotkey (`development/mods/red_flag/src/gr44.c:10641`). Owned, not ERE work.

**Safety car (CONFIRMED).** The SC mod uses carrier eid 132 (score rewritten to 131-135) and its own state machine (`pace_probe_standings.c:550, 615-619, 4149, 4835, 4864, 4932`). Native TV sites exist for 423 and 424 in `functions/ConstructorStandingsParser_00420820.c:163,245` (misleading file name). None for 425 and 428.

**Qualifying traffic (CONFIRMED).** Owned by `development/mods/traffic_density_commentary/traffic_density_commentary.c` (eids 11/12/15, latched per band, live-confirmed 2026-07-28).

**Team front row (DERIVED).** Native eid 397 site in `functions/PositionChangeDetector_00420490.c:111,189`. Eid 293 is also the 1999 Arrows team-name line (`functions/TeamNameDisplayIDLookup_0041b980.c:13`), so the TV twin cannot be the play path. Route is a PWS carrier (carrier_score). Polarity of the native predicate is unclear (v36/v37 are set on any teammate).

**Race distance (DERIVED).** Lap counter veh+0x403C and TotalRaceLaps 0x8ACF8C are CONFIRMED (SPEC_EVF-001 section 1). No native emitter for 270/271/274 in the emitter index. Cooldown 1.0 cannot gate; a latch is required.

**Session end (CONFIRMED site, unknown caller).** `functions/DisplayMessage0x113_0041a6b0.c:10` emits 275 as a literal, gated only on RaceActiveFlag and the commentary level. The caller is not in the corpus.

## Prior-art contradictions

1. SPEC_EVF-002A says the dispatcher "fires on RAISE, not clear" and omits the RacePhase gate. Code: raise emits 134/136/138; clear emits 133/135/137 and 126 (line 55/72/83/101), but returns early at line 36 when RacePhase == 5.
2. SPEC_EVF-001 section 1 cites `SessionCheckEndCondition_0046d0d0.c:23` for the race lap check. Line 23 is in the SessionType==5 (qualifying) branch (lines 20-23). The race branch (14-18) tests veh+0x4064 (16484).
3. EVF-008 (session end, eid 275) says the dispatch is variable. `DisplayMessage0x113_0041a6b0.c:10` is a literal 275.
4. Team display lines (`functions/TeamNameDisplayIDLookup_0041b980.c`) use TV eids 293-336, including TV twins of qualifying lines 302, 303, 306, 314-317, 321-323. Not a PWS collision, but it limits TV-side use.

## Shared state and hooks this batch needs

- `dword_7C56A8` (RacePhase) read per frame: CONFIRMED location, value 5 = racing (DERIVED), 8 = safety car (DERIVED).
- `dword_7C56A4` (SessionType), `dword_7D0308` (RaceActiveFlag), `flt_7CF3C8` (commentary level): FACT.
- `veh+0x32C8` (active), `veh+0x3450` (pittingStatus), `veh+0x4060` (position), `veh+0x403C` (lapNumber), `veh+0x4044` (lap distance), `veh+0x489C` (subject id): mixed tiers, cited above.
- A per-frame observer that wraps the flag dispatcher's clear edge (this[band+316] 2 to 0) to emit 133/135/137/126 under our own latch, if the RacePhase gate is confirmed.
- Carrier-score write for QUAL32 (PWS carrier eid TBD, score 0).

## Open questions

- Owner check: dword_7C56A8 value during a real yellow raise and clear. Prediction: 5 while racing, which blocks 126/133/135/137.
- Which caller passes a2 = 0 / 2 / 3 to sub_41B3C0. The inspect tool lists callers 0x4BCCC0 and 0x4D1730, but the call text is not in the decompiled files.
- Run counter and flying/in/out-lap detector for the qualifying lines (no source found).
- Safe free PWS carrier eid for QUAL32 (eid 0 has no clip, score -1; not proven free).
- Caller of sub_41A6B0 (eid 275) and why it is silent.
- Race milestone behaviour on formation lap and timed races.

## Verification done

- Every row lists its eid source (full_event_dump.json, docs/56, emitter_index.json).
- Line references checked against the decompiled files and the mod sources. Nothing live was run.
- Pending validation: all DERIVED rows; the RacePhase check; the carrier choice; the run and comparator definitions.
