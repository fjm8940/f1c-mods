# ASSIGNMENT EVF-008 — re-verify the injection-only SHIPPED batch (36 rows)

## Read first, in this order

1. `../DEEPSEEK_INSTRUCTIONS.md` — the whole file, especially §12 (`flt_7CF3C8`/
   `dword_7D0308` guards) and §12z (`data/full_event_dump.json` staleness).
2. `HANDOFF_2026-07-28.md` and `docs/PITSTOP_METHODOLOGY.md` — prior art on
   proving a trigger versus proving audio plays.
3. `findings.jsonl` at the corpus root — `python tools/ask.py findings
   "isolation-injection"` and `python tools/ask.py findings "SHIPPED"` from
   repo root. Read every result in full, not just the first page.

**You own:** `docs/SPEC_EVF-008_*.md` and nothing else. Do not touch
`data/progress.json` directly — propose status changes in your spec, a human
applies them via `tools/voice_browser.py`.

---

## Why this assignment exists — read this twice

This session, 40 clips were marked `SHIPPED` using one method: inject the eid
in isolation with `commentary_inject.py`, listen, confirm the audio content
matches the clip's description. That method proves **the audio clip is
intact and the queue mechanics work.** It does **not** prove the game's own
code ever reaches that call during real play.

Two clips were caught red-handed: `PWS_SLOWCAR01` and the `SeasonDisplayIDResolver`
family (`PWS_MISC03/04/05/46/47`, `PWS_PENALTY03/04/07`) both traced back to a
**post-race standings-screen** caller (`DelegatedFieldGetter(dword_7D2DB0)==3`
combined with a `.bik` win-movie `FileLoadWithRetryLoop` call) — the clip
plays fine on injection, but the code path is a menu screen, not a race tick.
Both reverted to `WIP`.

Then a THIRD, worse case: `PWS_GREEN01` / `PWS_YELLOW04/05/06`. Static trace
showed the emitter (`PitSelectionStateMachine_0041b3c0`) genuinely runs
**per-frame** (`priority: score 4 on_frame` in the priority index — a
deterministic, not guessed, tag), called from `sub_4BCCC0`, a real live
incident-detection function, with no menu fingerprint anywhere in the chain.
By every static signal available, this looked like a live, correctly-wired
trigger. **The human then stated flatly: this audio has never fired in real
play, full stop.** Static analysis said "reachable." Reality said "no."

**Conclusion you must carry into every row below: a clean `on_frame` tag and
a live caller chain are necessary but not sufficient.** The actual gate
condition inside the function can still be permanently false, or effectively
never true under real gameplay, for reasons static reading alone will not
surface (a comparison against a value that's always initialized wrong, a
band-crossing check that needs an exact frame timing static analysis can't
model, a companion-queue-slot check that always loses a priority fight, etc).
Your job is not to re-confirm the audio plays — that part is already proven
for all 36 rows. **Your job is to find out whether the code that decides to
call it can ever actually make that decision true, and if you cannot prove
it can, say so plainly instead of marking it live.**

---

## The rows (36, grouped by shared emitter — investigate per group, not per row)

### Group A — corner blister family (8 rows) — emitter chain: `PitStateSlotActivation_0041ab30` (severe) / `AISessionPhaseCounter_0041aa10` (light)

| Clip | SDT | EID | Description |
|---|---:|---|---|
| `RE_PD_FL_BAD_BL` | 93 | 94 | Front Left severe blistering |
| `RE_PD_FL_BLIST` | 94 | 95 | Front Left light blistering |
| `RE_PD_FR_BAD_BL` | 95 | 96 | Front Right severe blistering |
| `RE_PD_FR_BLIST` | 96 | 97 | Front Right light blistering |
| `RE_PD_RL_BAD_BL` | 97 | 98 | Rear Left severe blistering |
| `RE_PD_RL_BLIST` | 98 | 99 | Rear Left light blistering |
| `RE_PD_RR_BAD_BL` | 99 | 100 | Rear Right severe blistering |
| `RE_PD_RR_BLIST` | 100 | 101 | Rear Right light blistering |

Same threshold chain already `SHIPPED` (blog + this session, `PWS_TYRES01/02`,
FL/FR corners) — but confirm the *other three corners* (RL/RR specifically)
actually get evaluated by the same loop, not hardcoded to FL/FR only. Read
`PitStateSlotActivation_0041ab30.c` and `AISessionPhaseCounter_0041aa10.c` in
full; find the per-corner index/loop variable and confirm it iterates 0-3, not
a fixed constant.

### Group B — AI gear-loss / stuck-gear family (13 rows, SDT-name-collided) — emitter: `RaceCommentary_0041d630`

| SDT | EID | What was heard |
|---:|---|---|
| 214 | 201 | lost 1st gear (AI) |
| 215 | 202 | lost 2nd gear (AI) |
| 216 | 203 | lost 3rd gear (AI) |
| 217 | 204 | lost 4th gear (AI) |
| 218 | 205 | lost 5th gear (AI) |
| 219 | 206 | lost 6th gear (AI) |
| 220 | 207 | "periodically slowing down" (AI) |
| 221 | 208 | "smoke coming from engine" (AI) |
| 225 | 212 | stuck in 1st gear (AI) |
| 226 | 213 | stuck in 2nd gear (AI) |
| 227 | 214 | stuck in 3rd gear (AI) |
| 228 | 215 | stuck in 4th gear (AI) |
| 229 | 216 | stuck in 5th gear (AI) |
| 230 | 217 | stuck in 6th gear (AI) |

All truncate to `PWS_AIMECHPROB0`/`PWS_AIMECHPROB1` in the SDT (see
`DEEPSEEK_INSTRUCTIONS.md` §3 — do not treat these as 2 rows, they are 14
distinct game-side conditions sharing a name collision). Trace
`RaceCommentary_0041d630`'s single caller `sub_46BF10`, and that function's
two callers (`DispatchChainWithVTableCalls`, `sub_46DC60`). Both go through
`SessionStateCheckWrapper`/`StateCheckWrapper` gates before reaching
`RaceCommentary` — read what those two wrapper functions actually check.
**Specifically determine: does AI gear-loss/stuck-gear ever actually get
triggered for an AI opponent during a real race, or is the per-gear branch
selection driven by a value that's static/never-changing per car (e.g. reading
a car's *base* gear-ratio config instead of a live per-frame fault state)?**
That would explain "audio plays on injection, never heard in 100+ hours" —
the branch exists in code but the upstream AI-fault-injection system that's
supposed to *pick* a gear and *set* the fault flag may not exist or may never
run for AI cars specifically (only for the player, or not at all).

### Group C — AI tyre blistering (2 rows) — same `RaceCommentary_0041d630` chain

| Clip | SDT | EID |
|---|---:|---|
| `PWS_AITYRES01` | 232 | 219 |
| `PWS_AITYRES02` | 231 | 218 |

Human's own prior note (see `docs/WORK_CATEGORIES.md` T2): *"AI tire wear is a
lot slower than player, and it is dictated in .tbc file."* If AI tyre wear
literally never crosses the blister threshold at the rate the game runs AI
degradation, this is not a wiring problem — it's a data/balance problem
disguised as a silent clip. Find the `.tbc` degradation-rate field(s) for AI
vs player and state, with the actual values, whether the AI-side threshold is
reachable in a normal-length race at all.

### Group D — AI collision pair (2 rows) — emitter: `sub_41AD80_0041ad80`

| Clip | SDT | EID |
|---|---:|---|
| `PWS_AIINCID10` | 238 | 225 |
| `PWS_AIINCID11` | 239 | 226 |

`on_frame` tagged, caller not yet traced this session — trace it. Determine
what specifically discriminates "AI drivers collide (1)" from "(2)" — two
distinct severity/context bands, or arbitrary duplicate lines. State which.

### Group E — pit-related cluster (7 rows) — emitter: `PitRaceStatusMessageDispatcher_0041be60`, reached via `sub_40DE60` <- `PitTimer_004d2e50`

| Clip | SDT | EID | Description |
|---|---:|---|---|
| `PWS_2001_138` | 213 | 200 | AI driver positioned to secure WDC |
| `PWS_2001_141` | 212 | 199 | Team positioned to secure WCC |
| `PWS_AIPIT11` | 207 | 195 | AI pits to serve stop-go penalty |
| `RE_OD_MONSOON` | 204 | 192 | Other driver leaves pits on Monsoons |
| `RE_OD_LOST_7` | 222 | 209 | Other driver lost 7th gear |
| `RE_OD_STUCK_7` | 224 | 211 | Other driver stuck in 7th gear |
| `PWS_2001_131` | 196 | 184 | AI driver engine stalled in pits |

`PitTimer` touches `RevLimiter`/`EngineAngVel` — strong evidence of a live
per-car simulation tick, not a menu. But `PWS_2001_131`'s caller chain also
passes through `CarSelectionStateMachine_00434460` — already checked once
this session and identified as the **pre-race grid/formation-lap camera
state machine** (`skeletons_filled/CarSelectionStateMachine_00434460.md`),
not a menu — re-verify that reading is correct, and specifically confirm
whether "engine stalled in pits" can even occur during grid/formation-lap
(is the AI car near a pit at that point in a race weekend?) or whether the
description implies a moment this state machine can't represent.

### Group F — misc singles (4 rows)

| Clip | SDT | EID | Emitter | Description |
|---|---:|---|---|---|
| `PWS_2001_80` | 53 | 54 | `DisplayMessage0x151_0041c3e0`, via `sub_40DE60`<-`PitTimer` | Engine stalled - will get refired |
| `PWS_2001_159` | 293 | 275 | `DisplayMessage0x113_0041a6b0`, via `PitTimer` directly | Session ends |
| `PWS_OVERTAKE01` | 122 | 120 | `AIDriverStateTransition_00419120` | Player overtakes AI (1) |
| `PWS_AIBLUE03` | 275 | 259 | same function, different branch | Player passes AI driver |
| `PWS_AIPENALTY01` | 272 | 256 | `sub_41A720_0041a720` | AI penalty (type 1) |
| `RE_PD_SG_SPEED` | 162 | 153 | same function, sibling clip already `SHIPPED` pre-session (`PWS_PENALTY02`) | Stop/Go penalty - speeding in pits |

For `PWS_OVERTAKE01`/`PWS_AIBLUE03`: these two share a function with a
clip the human directly recognized in real play during testing ("thats also
in stock", "well done you passed, this is in stock game"). That is the
**strongest evidence in the whole batch** — treat these two as low-risk, spend
minimal time, note why (human recognition, not just injection match).

For `PWS_AIPENALTY01`/`RE_PD_SG_SPEED`: `sub_41A720` already has a sibling eid
(`PENALTY02`) that was `SHIPPED` from a **prior session**, before today's
injection-only method existed — check what evidence that prior `SHIPPED` row
actually rests on (read its `note` field in `data/progress.json`). If it was
also only ever injection-tested, downgrade your confidence accordingly; if it
has independent real-play evidence, that's a much stronger signal for its
siblings.

---

## What "investigate" means here — do not skip straight to a verdict

For every group:

1. Read the full source of the entry emitter function (not just the
   `DriverDataTableLookupAndTimeConversion(..., eid, ...)` call site).
2. Trace every caller up to the first function you can positively identify as
   either (a) a per-frame simulation tick (cite the `on_frame` tag or a
   caller like `PitTimer`/`MainGameTickUpdate`), or (b) a menu/screen
   dispatcher (cite the string table entry or `.bik`/`CH_*` screen name that
   proves it).
3. If it's (a), do not stop there — read the actual branch predicate that
   selects *this specific eid* versus its siblings, and ask: what real,
   observable game event has to happen for that predicate to go true? Name
   it in plain English (e.g. "AI vehicle's `veh+0xNNNN` gear-fault flag must
   be nonzero AND the car must still be running"). If you cannot find what
   sets that flag/condition anywhere in the corpus (`grep` every writer of
   the field), say so explicitly — an emitter with no known writer for its
   trigger condition is functionally silent even if the read-side code is
   perfectly live.
4. Cross-check `persistence_map.jsonl` for any vehicle-struct offset
   involved — `RECALCULATED` on a field your predicate treats as a stable
   flag is a strong candidate for "why this never fires as expected."

---

## Deliverables

- `docs/SPEC_EVF-008_A_blister.md`, `_B_gearloss.md`, `_C_aityres.md`,
  `_D_aiincid.md`, `_E_pit.md`, `_F_misc.md`
- Per clip, one line: `verdict: LIVE_TRUSTED | LIVE_UNPROVEN_PREDICATE |
  MENU_TRAP | NO_KNOWN_WRITER | UNKNOWN`, plus the plain-English trigger
  description from step 3 above, plus evidence citations.
- `LIVE_TRUSTED` requires either (a) an independent writer for the trigger
  condition that you can point to and explain, or (b) human real-play
  recognition already on record (cite the `progress.json` note) — injection
  audio match ALONE never qualifies a row for `LIVE_TRUSTED`.
- One `findings.jsonl`-format entry per group, tier `<= DERIVED`,
  `needs_review: true`.
- Non-empty `uncertainty` per group, mandatory.

## Order

Group B (gear-loss, 13 rows, biggest block) first — if the AI fault-injection
system genuinely doesn't exist for AI cars, that one finding downgrades 13
rows and probably Group C too. Then E, A, D, F.
