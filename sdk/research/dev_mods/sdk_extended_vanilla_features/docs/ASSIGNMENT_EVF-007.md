# ASSIGNMENT EVF-007 — penalty enforcement: the mechanism question (6 rows)

> **STAGING: hold until EVF-003 reports its first group.** EVF-003's largest
> observer group is `PENALTY_STEWARD` (24 T3 clips) and it is being worked right
> now. This assignment deliberately covers the **non-T3** penalty rows and the
> upstream mechanism question that EVF-003 has to assume an answer to. Starting
> both at once means two workers deriving the same steward state in parallel.
> Launch this one after EVF-003's first report lands.

## Read first, in this order

1. `docs/ASSIGNMENT_PARALLEL_RULES.md` — concurrent-worker rules, file ownership,
   the live-game lock, the findings append rule.
2. `docs/OBSERVER_TABLE.md` — **EVF-003's output; read it, do not edit it.**
   Its `PENALTY_STEWARD` group is your starting map. If it names the steward state
   fields, use them and cite them; do not re-derive.
3. `../DEEPSEEK_INSTRUCTIONS.md` — §12z, §12a, §12b, no truncated prior-art searches.
4. `HANDOFF_2026-07-28.md` — **section 0 first.**
5. `../dll/README.md` — **mandatory**, six traps.

**You own:** `docs/SPEC_EVF-007_*.md` and nothing else. **You do not spec any T3
clip** — those 24 rows belong to EVF-003.

---

## The one question this assignment exists to answer

> **Does the game enforce penalties on AI drivers at all?**

The human flags this uncertainty on row after row: "not sure if the game enforces
AI penalties", "determine if AI even does this", "enforced?". Roughly a dozen
clips across EVF-003 and EVF-007 are worthless if the answer is no, and cheap if
the answer is yes. Nobody has checked.

Settle it with evidence, not inference. Starting points:

- `strings_modules.json` names the original 2002 source file **`steward.cpp`** —
  find its functions and read them. That is the module that owns this.
- root `findings.jsonl` via `python tools/ask.py findings penalty` / `stop_go` /
  `steward` — prior art may already answer part of it.
- the known FACT that `RE_PD_DQ` (eid 149) **fires from pure gameplay**: ignore a
  stop-go long enough and the game disqualifies *the player*. So the player-side
  steward is real and it enforces. The open question is whether the same code
  path runs for AI slots, or whether the AI branch is stubbed.
- `veh+0x342C` is named `aiPitStrategyIndex` and **reads 3 on every car** — a
  known root cause of the AIPit10 bug. Treat any AI-side field that reads
  identically across all slots as suspect: it may be uninitialised, not meaningful.

Report the verdict as `ENFORCED | NOT_ENFORCED | PARTIAL` with the function and
file:line that proves it. `PARTIAL` (e.g. tracked but never actioned) is the most
likely answer and is a perfectly good result — it means the commentary can be
wired to the *tracking*, not the enforcement.

---

## The rows

### Group A — player penalties needing a definition (T5, 3 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_PENALTY07` | 150 | 141, 490 | overtaking under yellow. Two checks: player is in a yellow sector AND position improves. Human says this is doable |
| `RE_PD_25_DD` | 157 | 148, 478 | 25 s dangerous driving — **what does the game consider dangerous driving?** |
| `RE_PD_SG_DD` | 161 | 152, 501 | stop/go for dangerous driving; human is not sure it exists at all |

`RE_PD_25_DD` and `RE_PD_SG_DD` are the same definitional question in two
severities. If `steward.cpp` has no dangerous-driving concept, both go on the
unbuildable list and we would be inventing a rule the game does not have — say
that plainly rather than designing one.

`PWS_PENALTY07` is the buildable one. Note the yellow-sector state is shared with
`SPEC_EVF-002A` (yellow-flag clear) — **read it and reuse its field, do not
re-derive the sector flags.**

### Group B — the "under investigation" framework (T5, 1 row)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_MISC05` | 152 | 143, 494 | the first "under investigation" line. Human's design: when a penalty would normally apply, play this + show an "UNDER INVESTIGATION" box, run a random 30-60 s timer, then apply the already-decided penalty. Purely for tension |

This is a **design** row, and the human has already designed it. Your job is
feasibility, not redesign:

- can the penalty application be **deferred** by 30-60 s without the game's own
  steward re-applying it in the meantime?
- is there an existing on-screen text mechanism to reuse (red_flag's message box
  is cited elsewhere in this project) — name it, or say there isn't one
- the random delay needs a **seed policy**: deterministic per (race, lap, driver),
  or a race restart re-rolls and the latch lies

It also sets a pattern: several `RE_PD_INV_*` / `RE_OD_INV_*` investigation clips
sit in EVF-003's T3 group and would all consume this framework. **Say what the
framework's interface should be**, so EVF-003's observer can call it. That is the
most valuable thing in this assignment after the enforcement verdict.

### Group C — AI penalties (T5 + T4, 2 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_AIPENALTY08` | 255 | 239 | AI blocking under blue, 25 s penalty. "not sure if the game enforces AI penalties but if it does, this should play; investigate how AI penalties are enforced and whether they can be activated by script" |
| `PWS_MISC09` | 260 | 244 | AI under investigation for blocking under blue — depends on both the enforcement verdict and Group B's framework |

These two are **downstream of the enforcement verdict**. Do not spec them until
that verdict exists. If the answer is `NOT_ENFORCED`, the honest outcome is:
the commentary can still play if we track the infraction ourselves and never
actually penalise the AI — state whether that is acceptable and what it costs.

---

## Specific things not to get wrong

- A penalty line under a **once-per-race latch** is exactly the case where a false
  positive eats the real event. Every row here needs a `cancel_condition`, not an
  eager trigger (`dll/README.md`).
- **`lapNumber` going backwards is the only reliable race-restart signal.**
  `SessionType` and `RaceActiveFlag` do not change on a restart. Penalty latches
  must reset on it.
- Penalties are announced *after* the fact and standings may have changed by then
  (the human notes this on `PWS_PENALTY08`). Say whether your predicate reads
  standings at arm time or fire time — and re-validate at fire time.
- AI rows address another vehicle slot: live iff int32 at `veh+0x32C8` != -1, name
  at `veh+0x4620`. Never hardcode a grid list.
- Do not propose a second DLL or tick-hook site. Rules §7.

---

## Deliverables

- `docs/SPEC_EVF-007_enforcement_verdict.md` — the mechanism answer, **first**
- `docs/SPEC_EVF-007A_player_penalties.md`, `docs/SPEC_EVF-007B_investigation.md`,
  `docs/SPEC_EVF-007C_ai_penalties.md`
- verdict per clip: `WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN`
- one findings line per group in `batch/evf_pending/EVF-007.jsonl`, tier <= DERIVED,
  `needs_review: true`
- non-empty uncertainty per group; unbuildable rows separate, with your search shown

## Order

**The enforcement verdict first, alone, then stop and report.** It re-prices this
whole assignment and about a dozen of EVF-003's rows.
