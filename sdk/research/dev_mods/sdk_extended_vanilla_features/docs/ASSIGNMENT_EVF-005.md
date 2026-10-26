# ASSIGNMENT EVF-005 — mechanical failure, damage & track surface (9 rows)

## Read first, in this order

1. `docs/ASSIGNMENT_PARALLEL_RULES.md` — **you are one of several workers running
   concurrently.** File ownership, live-game lock, findings append rule. Violating
   them destroys another worker's output.
2. `../DEEPSEEK_INSTRUCTIONS.md` — §12z, §12a, §12b, and the ban on truncating
   prior-art searches.
3. `HANDOFF_2026-07-28.md` — **section 0 first.**
4. `../dll/README.md` — **mandatory**, six traps.
5. `WORK_CATEGORIES.md` + `TRIAGE_NOTES.txt` for each row's human note.

**You own:** `docs/SPEC_EVF-005_*.md` and nothing else.

---

## Already settled — do NOT re-derive

`PWS_RETIRE01`, `PWS_RETIRE02`, `PWS_RETIRE04` are **handled by
`dll/retire_commentary_v5.dll`**. RETIRE02 is heard in-game. 01 and 04 need only
`ReasonCode` 7 and 1 to occur live. **No spec work.** `PWS_RETIRE01` appears in
your T5 list only because the generated table has not caught up. Skip it, and say
in one line that you skipped it and why.

That leaves **8 real rows**.

---

## The rows

### Group A — failure modes the game may or may not have (T5, 4 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_MECHPROB16` | 74 | 75, 345 | suspension damage. Happens in vanilla. See `SeasonData/Vehicles/damage.ini`. Suspension damage often coincides with wing loss — **how does the wing line not suppress this one?** |
| `PWS_MECHPROB14` | 75 | 76, 348 | electronics periodically malfunctioning. Human's idea: reuse the overheating RPM-cut behaviour |
| `PWS_MECHPROB27` | 86 | 87, 377 | stuck in gear. Unclear it happens to the player at all. Fallback idea: disable shift input and speak the gear |
| `PWS_MECHANWARN0` | 134 | 130, 415 | generic player mechanical warning — what determines it? |

**Start here, with `PWS_MECHPROB16`,** because it has the strongest existing
anchor: the player wing/wheel-loss lines `PWS_HIT01-06` are already **restored and
heard** (cooldown FLT_MAX -> 15.0, a 4-byte write). Read that prior art first —
`patches/README.md` and the `findings.jsonl` FACT entry. The question that matters:

> Is `PWS_MECHPROB16` a real emitter with a FLT_MAX cooldown (i.e. a 4-byte fix
> like PWS_HIT), or is it genuinely emitterless (needs an observer)?

That single check may collapse this row from T5 to T1. Do it before anything else.
Apply the same check to all four Group A rows and report the answer per row — it
is cheap and it re-tiers the group.

The human's "wing damage suppresses suspension damage" observation is a **queue
contention** question, not a damage question. `dll/README.md` covers the single
contested slot. Answer it in those terms.

### Group B — engine stall (T5, 1 row)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `RE_PD_STALLED` | 111 | 112, 405 | engine stall while working on the T-car/spare; possibly teleport the player to the garage after a timer and restart |

Related but **not yours**: `RE_PD_STALLED_T` (sdt 112, T3) belongs to EVF-003.
Do not spec it. If your finding changes its shape, note that in your uncertainty
section.

Prior art: `tools/teleport_car.py` exists; the de-retirement work is solved
(3/3 in-race) and the stale-waypoint trap at `veh+0x4354` is a known cost. Search
findings for `deretire`, `retire`, `garage` before proposing a teleport.

### Group C — oil / localised grip loss (T5, 3 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_OIL01` | 131 | 127, 420 | oil on track — determine if an AI incident happened on the racing line |
| `PWS_OIL02` | 132 | 128, 417 | same |
| `PWS_OIL03` | 133 | 129, 418 | same |

The hard part is **not** the commentary, it is the human's second question: is
there any per-track-segment grip modifier the game already owns? Two things to
check before concluding no:

- root `config_schema.json` / `CONFIG_SCHEMA.md` for grip/wetness tokens, and the
  `.aiw`/`.tbc` key ranges in `data_index.json`
- `OnLineWetness` / `OffLineWetness` — a per-segment wetness array, if it exists,
  is the mechanism

If there is no such field, these three are **commentary-only**: say the oil is on
track, change no physics. That is a legitimate and much cheaper answer. Say so
explicitly rather than designing a grip subsystem nobody asked for.

---

## Specific things not to get wrong

- Check `persistence_map.jsonl` for every offset before proposing a poke.
  `RECALCULATED` = poking is useless, name the writer instead.
- `damage.ini` is a **config file**, not code. A token in it must be traced to the
  parser and the offset it writes (`config_schema.json`, `assoc=inline` is strong,
  `assoc=lookahead` flagged `suspect` must be read in the `.c` first).
- Do not invent an offset for "electronics malfunction". If nothing in the corpus
  names it, that row goes on the unbuildable list with your search shown.
- Do not propose a second DLL or a second tick-hook site. Rules §7.

---

## Deliverables

- `docs/SPEC_EVF-005A_mech_failure.md`, `docs/SPEC_EVF-005B_stall.md`,
  `docs/SPEC_EVF-005C_oil_grip.md`
- verdict per clip: `WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN`
- one findings line per group in `batch/evf_pending/EVF-005.jsonl`, tier <= DERIVED,
  `needs_review: true`
- non-empty uncertainty per group; unbuildable rows listed separately

## Order

**Group A's FLT_MAX-cooldown check across all four rows first, then stop and
report.** If any of them is a PWS_HIT-style 4-byte fix, that changes what the rest
of this assignment is worth.
