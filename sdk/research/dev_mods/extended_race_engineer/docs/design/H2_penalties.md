# H2 penalties and red-flag exit: design summary

Batch: `H2_penalties` (34 clips). Read-only static analysis, 2026-10-18. No live tests were run in this batch.
Machine-readable rows: `data/design/H2_penalties.json`. Every row carries its own sources (file:line) and trust tier.

## Headline findings

- **The native penalty emitter is `sub_41A720` and it is player/AI aware.** The player branch gives eids 151, 153, 154, 155. The AI branch gives 252, 253, 254, 256. The AI branch is only reached when a caller passes an AI car. Today the only AI caller is the jump-start check (`sub_4BC9B0`, reached from `PitCloseTeamReadyCheck` for every car).
- **The native tracker `sub_4BCCC0` is player-only** (`MainGameTickUpdate` line 86-87 calls it for `dword_7C54E8` only). So AI cut-track, AI blue-flag and AI pit-speeding penalties never fire natively. The fix is either to run the tracker per AI car (owner decision, changes AI race results) or for ERE to call `StopGoPenaltyHandler` / `WarningHandler` on the AI tracker itself.
- **Exit-under-red is a real native DQ with no audio.** `sub_4A5F20` calls `DisqualificationHandler("Exiting Pits Under Red")` for any car in the race. `sub_41A940` has no branch for that string, so it is silent. This affects both player (38) and AI (36). The owner's note that AI never exits under red conflicts with the code.
- **Dangerous driving has no native infraction** (no such string in `strings_index.json`). Six clips (148, 150, 152, 249-251) need an owner-defined predicate first.
- **Overtaking under yellow has no native infraction** either. Clips 146, 156, 247, 255 need an owner-approved rule. The steward yellow-zone table (`0x8ACEE0`) gives the state needed.
- **Blue-flag warnings 123/124/125 are real and player-only.** They are fired by `PitSelectionStateMachine` at blue timer 0, 10 and 20 s. The native penalty (154 plus stop-go) comes at 30 s, if blue persists. This is the basis for PWS_PENALTY08 and PWS_MISC05.
- **TV twins (478-503, 327-330) are only emitted from post-race screens** (`sub_46FC30` with `dword_7D0410==4`, `GdbSeasonSelector`). In-race ERE should design for the PWS eids only, as the brief says.
- **AI stop-go and AI DQ are reachable** through the jump-start route. `StopGoPenaltyHandler` queues event 23 (`StopGoPenaltyHandler_004bbd60.c:41`), which feeds the pending-stop-go counter `veh+0x4070` (`AccumulatorWithFixedArray_004bbed0.c:8-13`). An unserved AI stop-go then reaches eid 243 through `LapCompletion`. Not yet observed live.

## Per-clip table

| Clip | PWS eid | Says (short) | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|---|
| RE_OD_EXIT_RED | 36 | Other driver under investigation, exiting red | hook_native | AI car DQ with arg `Exiting Pits Under Red` (0x5CD698) | Wording says investigation; native is immediate DQ. Owner |
| RE_OD_LAP_DISAL | 37 | Other driver lap disallowed | game_state_emitter | Same as 36, plus AI best-lap check | No native lap-disallow; owner |
| RE_PD_EXIT_RED | 38 | Under investigation, exiting under red | hook_native | Player DQ with `Exiting Pits Under Red` | Wording mismatch; owner |
| RE_PD_LAP_DISAL | 39 | Fastest lap disallowed, exited under red | game_state_emitter | Same as 38, plus best-lap check | No native consequence; owner |
| PWS_PENALTY08 | 139 | Penalty (type 8) | game_state_emitter | Blue timer >= 20 s after eid 125, blue still set | 25 s has no native path; stacks with native 30 s stop-go; owner |
| PWS_PENALTY07 | 141 | Stop/go penalty | game_state_emitter | Player StopGo issued, queue 2 s after native line | Duplicates 151/153/154/155; owner |
| PWS_MISC05 | 143 | Player blocking blue, investigation | game_state_emitter | Blue timer 25-30 s | Investigation step is new; owner |
| PWS_MISC47 | 144 | Player cutting track, investigation | hook_native | First cut: `WarningHandler(Cut Track)` on player | None blocking (DERIVED) |
| PWS_MISC04 | 146 | Player overtaking yellow, investigation | game_state_emitter | Position gain inside yellow zone (state 2) | New penalty rule; owner |
| PWS_MISC03 | 147 | Player speeding pits, investigation | game_state_emitter | Pit-lane exit with accumulator in [half, 7.5) | Threshold choice; owner |
| RE_PD_25_DD | 148 | 25 s penalty, dangerous driving | game_state_emitter | Undefined predicate | No DD infraction; owner |
| RE_PD_DQ | 149 | Disqualified, failed stop/go | owned_by_other_mod | Already live | `player_dq_commentary` patch (FACT) |
| RE_PD_INV_DD | 150 | Under investigation, dangerous driving | game_state_emitter | Undefined predicate | No DD infraction; owner |
| RE_PD_SG_DD | 152 | Stop/go, dangerous driving | game_state_emitter | Undefined predicate, queue 152 directly | No DD infraction; owner |
| PWS_PENALTY04 | 154 | Blocking blue penalty | hook_native | Native 30 s blue hold (`sub_4BCCC0` L375) | Hold resets on any break (L265-268); live test |
| PWS_PENALTY03 | 156 | Overtaking yellow penalty | game_state_emitter | Yellow-zone overtake, owner penalty | New penalty rule; owner |
| PWS_AIPENALTY08 | 239 | AI penalty (type 8) | game_state_emitter | Undetermined | Need infraction identity (isolation listen) |
| PWS_AIPENALTY14 | 240 | AI penalty (type 14) | game_state_emitter | AI cut detector (EVF label) | Label; 25 s; AI tracker |
| PWS_AIPENALTY07 | 241 | AI stop/go penalty | game_state_emitter | AI stop-go issued | Needs AI stop-go source |
| PWS_AIPENALTY05 | 242 | AI cutting track penalty | game_state_emitter | AI cut stop-go | Label conflict with 253 |
| PWS_AIPENALTY22 | 243 | AI penalty (type 22), failed stop/go | hook_native | Unserved AI stop-go reaches DQ (LapCompletion L140-153) | Live proof AI gets a stop-go |
| PWS_MISC09 | 244 | AI blocking blue, investigation | hook_native | AI blue timer | AI tracker; owner |
| PWS_MISC06 | 246 | AI false start, investigation | game_state_emitter | Jump-start predicate, queue before 254 | Investigation step new; owner |
| PWS_MISC08 | 247 | AI overtaking yellow, investigation | game_state_emitter | Yellow-zone overtake (AI) | New rule; owner |
| PWS_MISC07 | 248 | AI speeding pits, investigation | hook_native | AI pit-speeding accumulator | AI tracker; owner |
| PWS_MISC48 | 245 | AI cutting track, investigation | hook_native | AI cut warning (`WarningHandler`) | AI tracker or AI detector; owner |
| RE_OD_25_DD | 249 | Other driver 25 s, dangerous driving | game_state_emitter | Undefined predicate | No DD infraction; owner |
| RE_OD_INV_DD | 250 | Other driver investigation, dangerous driving | game_state_emitter | Undefined predicate | No DD infraction; owner |
| RE_OD_SG_DD | 251 | Other driver stop/go, dangerous driving | game_state_emitter | Undefined predicate | No DD infraction; owner |
| PWS_AIPENALTY04 | 252 | AI blocking blue penalty | hook_native | AI blue 30 s hold (native AI branch) | AI tracker; fairness |
| PWS_AIPENALTY13 | 253 | AI penalty (type 13), cut track | hook_native | AI cut stop-go via native branch | AI tracker; label conflict |
| PWS_AIPENALTY02 | 254 | AI speeding in pits (owner label) | hook_native | AI jump start in pit box (only reachable AI path today) | Label conflict: code says jump start |
| PWS_AIPENALTY03 | 255 | AI overtaking yellow penalty | game_state_emitter | Yellow-zone overtake (AI) | New rule; owner |
| PWS_AIPENALTY01 | 256 | AI penalty (type 1), pit speeding | hook_native | AI pit speeding via native branch | AI tracker; label |

Counts: game_state_emitter 21, hook_native 12, owned_by_other_mod 1, cooldown_unblock 0, carrier_score 0, skip 0.
No clip is set to skip, because each has a native event, a real trigger, or an owner-decision blocker.

## Shared state and hooks this batch needs

1. **Penalty tracker per vehicle:** tracker = `veh+0x4034`. Reads: off-track `veh+0x40E8`, blue byte `veh+0x4108`, blue timer `veh+0x4110`, cut count `veh+0x4104`, pit-speeding accumulator `veh+0x40E0`, pending stop-go count `veh+0x4070`, pit-lane flag `veh+0x344C`.
2. **Hooks (entry points, pointer compare for strings):** `WarningHandler` `0x4BBC60`, `StopGoPenaltyHandler` `0x4BBD60`, `DisqualificationHandler` `0x4BC0E0`, `sub_41A720` `0x41A720`.
3. **Infraction string pointers:** Ignored Blue Flags `0x5C26EC`, Jumped start `0x5C2700`, Cut Track `0x5C2710`, Speeding In Pitlane `0x5C271C`, Ignored Stop/Go Penalty `0x5C2730`, Exiting Pits Under Red `0x5CD698`, Entering Pits Under Red `0x5CD680`, Wrong Way `0x5D25B0`.
4. **Per-car AI tracker run (owner decision):** either run `sub_4BCCC0` for AI cars or let ERE call the handlers on the AI tracker. This single decision unlocks about 10 AI rows.
5. **Queue discipline:** single slot `0x5EA618 +0xAE4`. Every injected line waits for idle, then retries (per the EVF queue lesson).
6. **Yellow-zone table** `0x8ACEE0` (+0x10C count, +0x110 stride 8, state 2 = yellow), needed for the yellow rows (146, 156, 247, 255).

## Open questions (for owner)

1. **Eid label conflicts:** owner labels 155 (`PWS_PENALTY02`) and 254 (`PWS_AIPENALTY02`) as speeding in pits. Code sends jump start to those eids. Isolation listen needed before any mapping.
2. **Penalty magnitudes:** owner rules say 25 s. No native 25 s type exists (stop-go value is 10). Need the time-penalty mechanism.
3. **Dangerous driving predicate:** must be defined (repeated cutting, contact, etc.). Three player and three other-driver rows depend on it.
4. **Investigation wording:** the native game has no investigation step. Lines that say investigation describe a step that does not exist unless ERE adds one. Owner decides wording or new mechanic.
5. **Red-flag lap disallowed:** no native lap consequence in race. Confirm whether the DQ removes best lap (veh+0x409C).
6. **Blue 30 s hold:** does the native 30 s hold ever happen in real play? Timer resets on any break of the predicate.

## Prior-art contradictions found

- `TRIAGE_NOTES` line 20 says AI never exits under red. `sub_4A5F20` shows the same branch runs for AI cars. Needs a live check.
- `SPEC_EVF-003_penalty.md` lists 13 AI clips as dead (retracted in the same file). Current status: AI penalties are reachable only via the jump-start path; other AI penalty branches exist but have no caller. Use the RETRACTED entry, not the older "dead" verdict.
- `docs/56` and `VOICE_LINES` label 254 as AI speeding in pits, but the code and EVF say jump start (see open question 1).

## Pending validation

- Live: AI jump start in pit box (254), red-flag exit by AI and player (36, 38), 30 s blue hold (154), 20 s blue then 25 s owner window (139, 143), warning on first cut (144).
- Isolation listen: 139, 239-256 (labels).
- Static only: all tiers are DERIVED except RE_PD_DQ (FACT, owned).
