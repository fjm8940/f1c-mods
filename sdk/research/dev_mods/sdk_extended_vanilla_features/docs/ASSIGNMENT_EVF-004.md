# ASSIGNMENT EVF-004 — the pit-stop procedure & pit-failure subsystem (13 rows)

## Read first, in this order

1. `docs/ASSIGNMENT_PARALLEL_RULES.md` — **you are one of several workers running
   concurrently.** File ownership, the live-game lock, and the append rule for
   `findings.jsonl` are all in there. Violating them destroys another worker's output.
2. `../DEEPSEEK_INSTRUCTIONS.md` — operating rules, especially §12z (stale
   `full_event_dump.json`), §12a (settled globals), §12b (uncited numbers), and the
   ban on truncating prior-art searches.
3. `HANDOFF_2026-07-28.md` — **section 0 first**, it supersedes the rest.
4. `../dll/README.md` — **mandatory.** Six traps from five failed DLL iterations.
5. `WORK_CATEGORIES.md` + `TRIAGE_NOTES.txt` — the human's note for each row below.

**You own:** `docs/SPEC_EVF-004_*.md` and nothing else.

---

## The domain

Everything that happens between "the player commits to a pit stop" and "the player
is back on track", plus the pit-stop *failures* that the 2001 audio set has lines
for but the game never generates.

This is one domain on purpose: all 13 rows read the same small set of state (pit
request flag, pit-lane/pit-box occupancy, the pit-box timer, fuel level, per-wheel
change flags). Find that state **once**, then every row is a predicate over it.

---

## The rows

### Group A — pit-box timer, phase detection (T5, 4 rows)
The core unknown. Everything else in this assignment depends on it.

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_MISC54` | 45 | 46 | plays just as player drives into the pit box; is there a signal *before* the timer starts? |
| `PWS_PIT09` | 43 | 44, 335 | second-to-last sound, "lollipop turns" — mechanism unknown |
| `PWS_PIT10` | 44 | 45, 336 | pit stop ends, player exits the box — which address changes? |
| `PWS_MISC18` | 46 | 47 | fuel for one lap OR scheduled stop next lap; may already play |

**Do this first.** Produce a **pit-stop state timeline**: name every field that
changes between pit-lane entry and pit-lane exit, in order, with the offset, the
value before and after, and the tier. That timeline is the deliverable the other
three groups consume. If you can only get part of it statically, say which part
and emit a `LIVE READ REQUEST` for the rest.

Prior art to read before deriving anything: the `red_flag` script's NUMPAD2
pit-lane-entry detection (the human cites it repeatedly), and `SPEC_EVF-002F`.

### Group B — refuelling & stall (T5, 4 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_2001_79` | 55 | 56, 365 | refuelling delay: either lengthen the stop, or make fuel level not start rising instantly |
| `PWS_2001_80` | 53 | 54, 337 | "stall" — what counts? Possibly: player stationary on the grid >1 s after lights out while the field moves |
| `PWS_2001_68` | 61 | 62, 369 | successful front-left tyre change. Note the human's own objection: **four similar lines may take longer to play than the pit stop itself** — address this, don't ignore it |
| `PWS_2001_131` | 196 | 184, 516 | AI stalls the engine in the pits; note points at red_flag NUMPAD2's AI-pit-retirement handling |

`PWS_2001_80` is sdt 53 = **work queue 53**, a row the human has looked at
directly. Be precise about it.

### Group C — pit-stop failures, the new subsystem (T4, 5 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_2001_76` | 60 | 61, 372 | wheel-gun failure: artificial delay in the pit-box timer; pick a realistic number of seconds |
| `PWS_2001_72` | 62 | 63, 358 | front-left problem; randomly rolled from a pool of pit-stop failures |
| `PWS_2001_77` | 65 | 66, 371 | failure extended far enough to end the race — "WHEEL JAMMED" text like red_flag? |
| `PWS_2001_133` | 198 | 186 | AI front-left wheel stuck; should the AI be able to continue? |
| `PWS_TEAMPIT01` | 57 | 58, 366 | team-mate has a pit order active AND player is about to pit within one lap |

These are T4 because they need the game to *do* something new, not just say
something. The design question you must answer explicitly:

> **Can the pit-box timer be extended from a tick hook, and does the game's own
> pit logic tolerate it** (car released late, position recalculated correctly,
> no desync with the AI pit queue) — or does extending it require a patch to the
> function that owns the timer?

Answer with the function that writes the timer and its persistence verdict
(`persistence_map.jsonl`, or `grep '"off": "0xNNNN"'`). `RECALCULATED` means a
poke is useless and you must name the writer instead.

`PWS_TEAMPIT01` needs the *team-mate's* pit order — a second vehicle slot. State
how you identify the team-mate (the driver name at `veh+0x4620` makes a slot
self-identifying; team name lives in the 1060-byte driver struct at +124).

---

## Specific things not to get wrong

- **Randomness needs a seed policy.** Group C's "randomly rolled" failures must be
  deterministic per (race, lap, driver) or a race restart will re-roll differently
  and the latch logic will lie. Specify it.
- **A pit stop is not once-per-race.** Most of these are `repeatable + cooldown`.
  State the cooldown and what resets it.
- Several rows say "may already play". For each, first answer *does it already
  fire, and if not, what exactly blocks it* — check whether the eid is blocked in
  the **regenerated** `data/full_event_dump.json` (blocked iff the float at
  `0x005BF9A0 + eid*0x14 + 0x0C` >= 1e37).
- Do not propose a second DLL or a second tick-hook site. See rules §7.

---

## Deliverables

- `docs/SPEC_EVF-004A_pit_timeline.md` — the state timeline + Group A rows
- `docs/SPEC_EVF-004B_refuel_stall.md`
- `docs/SPEC_EVF-004C_pit_failures.md`
- one findings line per group in `batch/evf_pending/EVF-004.jsonl` (append-only
  file that is yours alone), tier <= DERIVED, `needs_review: true`
- unbuildable rows in a separate list, with the search you already did

## Order

**Group A first, then stop and report.** The pit-stop state timeline decides
whether B and C are cheap or impossible. Do not spec B or C before A's timeline
is on paper.
