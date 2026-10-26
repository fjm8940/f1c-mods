# ASSIGNMENT EVF-003 — the observer table (96 T3 rows)

## Read first, in this order

1. `../DEEPSEEK_INSTRUCTIONS.md` — operating rules. **Updated since EVF-002**:
   §12z (stale `full_event_dump.json`), §12a (settled globals), §12b (uncited
   numbers), and the rule against truncating prior-art searches.
2. `HANDOFF_2026-07-28.md` — **section 0 first**, it supersedes the rest.
3. `../dll/README.md` — **mandatory.** Six traps found across five failed DLL
   iterations. Any predicate you spec will run inside that engine, so its
   constraints are your constraints.
4. `WORK_CATEGORIES.md` — your queue.

## What changed since your last batch (do not re-derive)

- `RE_PD_DQ` and `PWS_RETIRE02` are **DONE**, both heard in-game.
- Injection plays the **race engineer (PWS)**, not the TV commentator. That
  open risk is closed — build emitters with confidence.
- `PWS_RETIRE01/04` are already handled by the shipped DLL; they only await
  `ReasonCode` 7 and 1 occurring live. **No spec work needed.**
- Your EVF-002B eid-117 row is superseded by that DLL.
- `data/full_event_dump.json` was regenerated (26 → 16 truly-blocked events).
  Re-read it; the copy you reasoned over was stale.

## The task

The 96 T3 rows are not 96 features. Corpus doctrine (see `findings.jsonl`) is
that the ~109 emitterless clips collapse into ~9 **state observers**. Your job is
to produce that collapse as a concrete table, because a config-driven observer
engine is being built and this table is its input.

**Step 1 — group.** Partition the 96 T3 rows into shared observers. A group is
"one thing being watched", not "one thing being said": all the penalty lines
likely share one observer, all the gear-loss lines another, all the pit-procedure
lines another. State your grouping rule. Report the count per group and, more
usefully, **any row that refuses to group** — those are the expensive ones.

**Step 2 — one table row per clip**, in this schema. Every field cited or marked
unknown; no invented offsets.

```
clip | eid | observer_group
watch_offsets   : veh+0xNNNN (name, tier) — the fields the predicate reads
arm_condition   : the predicate that starts it
fire_condition  : threshold + dwell (in seconds) that commits it
cancel_condition: what disarms it (this is what stopped v3 lying)
latch           : once-per-race | once-per-lap | repeatable+cooldown
reset_on        : what clears the latch (note: lapNumber going backwards is the
                  ONLY reliable race-restart signal — SessionType and
                  RaceActiveFlag do not change on a restart)
uncertainty     : mandatory, non-empty
```

**Step 3 — flag the unbuildable.** Any row whose predicate needs a field nobody
has identified (Group E's car-to-car gap is the known example) goes in a separate
list with the search you already did. Do not invent an offset to fill the cell.

## Constraints from the working DLL — your predicates must satisfy these

- **Never fire on a transition that coincides with session teardown.**
  `FinishState -> 2` injects fine and is never audible. Find an earlier signal
  and state the window in seconds.
- The commentary queue is a **single slot** the game competes for. The engine
  handles waiting/retrying — but if your line must beat the game's own chatter,
  say so explicitly, because that lever (queue priority at `qhead+0x1C`) is
  **unverified**.
- A **false positive is worse than silence** with a once-per-race latch: it
  consumes the real event. Prefer a `cancel_condition` over an eager trigger.
- Re-validate arming conditions at fire time; latched state goes stale.

## Deliverables

- `docs/OBSERVER_TABLE.md` — the table, grouped by observer
- `docs/SPEC_EVF-003_<group>.md` — one per observer group, existing spec format
- one `findings.jsonl` entry per group, tier ≤ DERIVED, `needs_review: true`
- **do not** modify `data/progress.json`, hand-edit generated docs, or apply patches

## Order

Start with the **largest** group — it justifies the engine. Stop and report
after the first group so the schema can be corrected before you apply it 96 times.
